// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/mocks/mock_skills_service.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::StrictMock;

class MockSkillsPage : public skills::mojom::SkillsPage {
 public:
  mojo::PendingRemote<skills::mojom::SkillsPage> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, UpdateSkill, (const skills::Skill& skill), (override));
  MOCK_METHOD(void, RemoveSkill, (const std::string& skill_id), (override));
  MOCK_METHOD(
      void,
      Update1PMap,
      ((const base::flat_map<std::string, std::vector<skills::Skill>>&)),
      (override));

  mojo::Receiver<skills::mojom::SkillsPage> receiver_{this};
};

class SkillsPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    SkillsServiceFactory::GetInstance()->SetTestingFactory(
        &profile_,
        base::BindLambdaForTesting([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockSkillsService>>();
        }));

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&profile_));
    web_ui_.set_web_contents(web_contents_.get());

    handler_ = std::make_unique<SkillsPageHandler>(
        page_handler_.BindNewPipeAndPassReceiver(),
        mock_page_.BindAndGetRemote(), web_ui_.GetWebContents());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  mojo::Remote<skills::mojom::PageHandler> page_handler_;
  StrictMock<MockSkillsPage> mock_page_;
  std::unique_ptr<SkillsPageHandler> handler_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SkillsPageHandlerTest, OnDiscoverySkillsUpdated) {
  auto skills_map = std::make_unique<SkillsDownloader::SkillsMap>();

  skills::proto::Skill skill_proto;
  skill_proto.set_id("skill_id");
  skill_proto.set_name("Skill Name");
  skill_proto.set_icon("icon");
  skill_proto.set_prompt("Skill prompt");
  skill_proto.set_category("Category");
  skill_proto.set_description("Skill description");

  skills_map->insert({"skill_id", skill_proto});

  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, Update1PMap(_))
      .WillOnce([&run_loop](
                    const base::flat_map</*category=*/std::string,
                                         std::vector<skills::Skill>>& map) {
        ASSERT_EQ(1u, map.size());
        ASSERT_TRUE(map.contains("Category"));
        const auto& skills = map.at("Category");
        ASSERT_EQ(1u, skills.size());
        const auto& skill = skills[0];
        EXPECT_EQ("skill_id", skill.id);
        EXPECT_EQ("Skill Name", skill.name);
        EXPECT_EQ("icon", skill.icon);
        EXPECT_EQ("Skill prompt", skill.prompt);
        EXPECT_EQ("Skill description", skill.description);
        EXPECT_EQ(sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY, skill.source);
        run_loop.Quit();
      });

  handler_->OnDiscoverySkillsUpdated(skills_map.get());

  run_loop.Run();
}

TEST_F(SkillsPageHandlerTest, MaybeSave1PSkill_Success) {
  base::test::TestFuture<bool> future;
  // Make a save skill request
  handler_->MaybeSave1PSkill("skill_id", future.GetCallback());

  // Manually trigger map update with valid map
  skills::proto::Skill skill_proto;
  skill_proto.set_id("skill_id");
  SkillsDownloader::SkillsMap skills_map = {{"skill_id", skill_proto}};
  handler_->OnDiscoverySkillsUpdated(&skills_map);
  EXPECT_TRUE(future.Get());
  EXPECT_FALSE(handler_->Is1PDownloadTimerRunning());
  histogram_tester_.ExpectBucketCount(
      "Skills.Management.FirstParty.DownloadRequestStatus",
      SkillsDownloadRequestStatus::kSent, 1);
  histogram_tester_.ExpectBucketCount(
      "Skills.Management.FirstParty.DownloadRequestStatus",
      SkillsDownloadRequestStatus::kResponseReceived, 1);
}

TEST_F(SkillsPageHandlerTest, MaybeSave1PSkill_NotFound) {
  base::test::TestFuture<bool> future;
  // Make a save skill request with invalid skill id
  handler_->MaybeSave1PSkill("nonexistent_skill_id", future.GetCallback());

  // Manually trigger map update with valid map
  skills::proto::Skill skill_proto;
  skill_proto.set_id("skill_id");
  SkillsDownloader::SkillsMap skills_map = {{"skill_id", skill_proto}};
  handler_->OnDiscoverySkillsUpdated(&skills_map);
  EXPECT_FALSE(future.Get());
  EXPECT_FALSE(handler_->Is1PDownloadTimerRunning());
  histogram_tester_.ExpectUniqueSample("Skills.Management.Error",
                                       SkillsManagementError::k1pSkillDNE, 1);
}

TEST_F(SkillsPageHandlerTest, MaybeSave1PSkill_Timeout) {
  base::test::TestFuture<bool> future;
  handler_->MaybeSave1PSkill("skill_id", future.GetCallback());
  // Fast forward past the timeout
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(future.Get());
  EXPECT_FALSE(handler_->Is1PDownloadTimerRunning());
  histogram_tester_.ExpectBucketCount(
      "Skills.Management.FirstParty.DownloadRequestStatus",
      SkillsDownloadRequestStatus::kTimedOut, 1);
}

TEST_F(SkillsPageHandlerTest, GetInitialUserSkills_ServiceNotReady) {
  // Mock service check to fail
  EXPECT_CALL(*static_cast<MockSkillsService*>(
                  SkillsServiceFactory::GetForProfile(&profile_)),
              GetServiceStatus())
      .WillRepeatedly(
          testing::Return(SkillsService::ServiceStatus::kNotInitialized));

  base::test::TestFuture<std::vector<skills::Skill>> future;
  handler_->GetInitialUserSkills(base::BindLambdaForTesting(
      [&future](const std::vector<skills::Skill>& skills) {
        future.SetValue(skills);
      }));

  histogram_tester_.ExpectUniqueSample(
      "Skills.Management.Error", SkillsManagementError::kSkillsServiceNotReady,
      1);
}

TEST_F(SkillsPageHandlerTest, Request1PSkills_DownloadAlreadyRunning) {
  handler_->Request1PSkills();
  EXPECT_TRUE(handler_->Is1PDownloadTimerRunning());

  // Second request should log kAlreadyRunning
  handler_->Request1PSkills();
  histogram_tester_.ExpectBucketCount(
      "Skills.Management.FirstParty.DownloadRequestStatus",
      SkillsDownloadRequestStatus::kAlreadyRunning, 1);
}

}  // namespace
}  // namespace skills
