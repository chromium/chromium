// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_downloader.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_types.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

namespace {

std::string CreateValidSkillsListString() {
  skills::proto::SkillsList skills_list;

  auto* skill = skills_list.add_skills();
  skill->set_name("/test-skill");
  skill->set_category("Test Category");
  skill->set_prompt("Test prompt.");
  skill->set_id("test-id");
  skill->set_image_url("https://example.com/some-image.png");

  skills_list.add_topics_list("test-topic");

  return skills_list.SerializeAsString();
}

}  // namespace

class SkillsDownloaderTest : public testing::Test {
 public:
  SkillsDownloaderTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        downloader_(shared_url_loader_factory_) {}

 protected:
  // Helper to execute the async fetch synchronously for testing.
  std::unique_ptr<FirstPartySkillData> FetchDiscoverySkillsSync() {
    std::unique_ptr<FirstPartySkillData> result;
    base::RunLoop run_loop;
    downloader_.FetchDiscoverySkills(base::BindLambdaForTesting(
        [&](std::unique_ptr<FirstPartySkillData> first_party_skill_data) {
          result = std::move(first_party_skill_data);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void AddValidSkillsResponse() {
    test_url_loader_factory_.AddResponse(kSkillsDownloaderGstaticUrl,
                                         CreateValidSkillsListString());
  }

  void AddInvalidSkillsResponse() {
    test_url_loader_factory_.AddResponse(kSkillsDownloaderGstaticUrl,
                                         "This is not a protobuf");
  }

  void AddErrorResponse(net::HttpStatusCode status_code) {
    test_url_loader_factory_.AddResponse(kSkillsDownloaderGstaticUrl, "",
                                         status_code);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  SkillsDownloader downloader_;
};

TEST_F(SkillsDownloaderTest, ReturnsValidSkills) {
  AddValidSkillsResponse();
  auto result = FetchDiscoverySkillsSync();

  ASSERT_TRUE(result);
  ASSERT_EQ(result->skills_list.size(), 1u);

  const auto& skill = result->skills_list[0];
  EXPECT_EQ(skill.id(), "test-id");
  EXPECT_EQ(skill.name(), "/test-skill");
  EXPECT_EQ(skill.image_url(), "https://example.com/some-image.png");

  ASSERT_EQ(result->topics_list.size(), 1u);
  EXPECT_EQ(result->topics_list[0], "test-topic");

  histogram_tester_.ExpectUniqueSample(
      "Skills.Downloader.FirstParty.FetchResult", SkillsFetchResult::kSuccess,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Skills.Downloader.FirstParty.HttpResponseCode", net::HTTP_OK, 1);
}

TEST_F(SkillsDownloaderTest, ReturnsValidSkillsWithRetry) {
  // 1. First call triggers a 404 (Not Found). We use 404 so that the
  // RETRY_ON_5XX is not triggered.
  AddErrorResponse(net::HTTP_NOT_FOUND);
  EXPECT_FALSE(FetchDiscoverySkillsSync());
  histogram_tester_.ExpectUniqueSample(
      "Skills.Downloader.FirstParty.HttpResponseCode", net::HTTP_NOT_FOUND, 1);

  // 2. Second call returns valid data.
  AddValidSkillsResponse();
  auto result = FetchDiscoverySkillsSync();

  ASSERT_TRUE(result);
  EXPECT_EQ(result->skills_list.begin()->id(), "test-id");
}

TEST_F(SkillsDownloaderTest, ReturnsInvalidProtocolBuffer) {
  AddInvalidSkillsResponse();
  EXPECT_FALSE(FetchDiscoverySkillsSync());
  histogram_tester_.ExpectUniqueSample(
      "Skills.Downloader.FirstParty.FetchResult",
      SkillsFetchResult::kProtoParseFailure, 1);
}

TEST_F(SkillsDownloaderTest, ReturnsEmptyResponse) {
  test_url_loader_factory_.AddResponse(kSkillsDownloaderGstaticUrl, "");
  EXPECT_FALSE(FetchDiscoverySkillsSync());
  histogram_tester_.ExpectUniqueSample(
      "Skills.Downloader.FirstParty.FetchResult",
      SkillsFetchResult::kEmptyResponseBody, 1);
}

TEST_F(SkillsDownloaderTest, ReturnsNotModifiedError) {
  AddErrorResponse(net::HTTP_NOT_MODIFIED);
  EXPECT_FALSE(FetchDiscoverySkillsSync());
  histogram_tester_.ExpectUniqueSample(
      "Skills.Downloader.FirstParty.FetchResult",
      SkillsFetchResult::kNetworkError, 1);
  histogram_tester_.ExpectUniqueSample(
      "Skills.Downloader.FirstParty.HttpResponseCode", net::HTTP_NOT_MODIFIED,
      1);
}

}  // namespace skills
