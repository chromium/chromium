// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler_v2.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/skills/features.h"
#include "components/skills/mocks/mock_skills_service.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_prefs.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace skills {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

inline constexpr char kProvidedSkillId[] = "provided_id";
inline constexpr char kProvidedSkillName[] = "Provided Skill";
inline constexpr char kProvidedSkillIcon[] = "provided_icon";
inline constexpr char kProvidedSkillPrompt[] = "Provided Prompt";

class MockSkillsPageV2 : public skills::mojom::SkillsPageV2 {
 public:
  mojo::PendingRemote<skills::mojom::SkillsPageV2> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              LoadProvidedSkills,
              ((const std::vector<skills::Skill>&)),
              (override));
  MOCK_METHOD(void, OnUserSkillsUpdated, (), (override));

  mojo::Receiver<skills::mojom::SkillsPageV2> receiver_{this};
};

class MockSkillsDialogDelegate : public SkillsDialogDelegate {
 public:
  MOCK_METHOD(void, CloseDialog, (), (override));
  MOCK_METHOD(void, OnSkillSaved, (const std::string& skill_id), (override));
  MOCK_METHOD(void, OnSkillDeleted, (const std::string& skill_id), (override));
  MOCK_METHOD(BrowserWindowInterface*,
              GetBrowserWindowInterface,
              (),
              (override));
};

class MockSkillsUiTabController : public SkillsUiTabControllerInterface {
 public:
  explicit MockSkillsUiTabController(tabs::TabInterface& tab)
      : SkillsUiTabControllerInterface(tab) {}
  ~MockSkillsUiTabController() override = default;

  MOCK_METHOD(void,
              ShowDialog,
              (Skill skill,
               SkillsDialogEntryPoint entrypoint,
               skills::mojom::SkillsDialogType dialog_type,
               std::unique_ptr<glic::Target> target),
              (override));
  MOCK_METHOD(void, CloseDialog, (), (override));
  MOCK_METHOD(bool, IsShowing, (), (const, override));
  MOCK_METHOD(void,
              InvokeSkill,
              (std::string_view skill_id,
               std::string_view skill_name,
               std::string_view skill_icon,
               bool auto_submit),
              (override));
  MOCK_METHOD(void, SendPrompt, (std::string_view prompt), (override));
};

class SkillsPageHandlerV2Test : public ChromeRenderViewHostTestHarness {
 public:
  SkillsPageHandlerV2Test() {
    feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SkillsServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindLambdaForTesting([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockSkillsService>>();
        }));

    handler_ = std::make_unique<SkillsPageHandlerV2>(
        remote_handler_.BindNewPipeAndPassReceiver(), profile(),
        identity_test_env_.identity_manager(), web_contents());
  }

  void TearDown() override {
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockSkillsService* mock_skills_service() {
    return static_cast<MockSkillsService*>(
        SkillsServiceFactory::GetForProfile(profile()));
  }

  base::test::ScopedFeatureList feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  mojo::Remote<::skills::mojom::SkillsPageHandler> remote_handler_;
  std::unique_ptr<SkillsPageHandlerV2> handler_;
};

TEST_F(SkillsPageHandlerV2Test, SyncCookies) {
  base::test::TestFuture<bool> future;
  remote_handler_->SyncCookies(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(SkillsPageHandlerV2Test, ShowSaveToast) {
  remote_handler_->ShowSaveToast();
  remote_handler_.FlushForTesting();
}

TEST_F(SkillsPageHandlerV2Test, ShowDeleteToast) {
  base::test::TestFuture<bool> future;
  remote_handler_->ShowDeleteToast("test_skill_id", future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(SkillsPageHandlerV2Test, InvokeSkill) {
  tabs::MockTabInterface mock_tab;
  ::ui::UnownedUserDataHost user_data_host;
  EXPECT_CALL(mock_tab, GetUnownedUserDataHost())
      .WillRepeatedly(testing::ReturnRef(user_data_host));

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);

  MockSkillsUiTabController mock_tab_controller(mock_tab);

  EXPECT_CALL(mock_tab_controller,
              InvokeSkill("test_skill_id", std::string_view("test_name"),
                          std::string_view("test_icon"),
                          /*auto_submit=*/true))
      .Times(1);

  remote_handler_->InvokeSkill("test_skill_id", "test_name", "test_icon");
  remote_handler_.FlushForTesting();
}

TEST_F(SkillsPageHandlerV2Test, SendPrompt) {
  tabs::MockTabInterface mock_tab;
  ::ui::UnownedUserDataHost user_data_host;
  EXPECT_CALL(mock_tab, GetUnownedUserDataHost())
      .WillRepeatedly(testing::ReturnRef(user_data_host));

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);

  MockSkillsUiTabController mock_tab_controller(mock_tab);

  EXPECT_CALL(mock_tab_controller, SendPrompt("test_prompt")).Times(1);

  remote_handler_->SendPrompt("test_prompt");
  remote_handler_.FlushForTesting();
}

TEST_F(SkillsPageHandlerV2Test, InvokeSkill_NoOpWhenDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kChromeSkillsEnabled, false);
  tabs::MockTabInterface mock_tab;
  ::ui::UnownedUserDataHost user_data_host;
  EXPECT_CALL(mock_tab, GetUnownedUserDataHost())
      .WillRepeatedly(testing::ReturnRef(user_data_host));

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);

  MockSkillsUiTabController mock_tab_controller(mock_tab);

  EXPECT_CALL(mock_tab_controller,
              InvokeSkill(testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  remote_handler_->InvokeSkill("test_skill_id", "test_name", "test_icon");
  remote_handler_.FlushForTesting();
}

TEST_F(SkillsPageHandlerV2Test, CloseDialog) {
  MockSkillsDialogDelegate mock_delegate;
  base::WeakPtrFactory<SkillsDialogDelegate> weak_factory(&mock_delegate);

  mojo::Remote<::skills::mojom::SkillsPageHandler> remote_handler;
  auto handler = std::make_unique<SkillsPageHandlerV2>(
      remote_handler.BindNewPipeAndPassReceiver(), profile(),
      identity_test_env_.identity_manager(), web_contents(),
      weak_factory.GetWeakPtr());

  EXPECT_CALL(mock_delegate, CloseDialog()).Times(1);

  remote_handler->CloseDialog(nullptr);
  remote_handler.FlushForTesting();
}

TEST_F(SkillsPageHandlerV2Test, ShowToastNoopWithoutBrowser) {
  remote_handler_->ShowSaveToast();
  remote_handler_->ShowSaveAndInvokeToast("id", "name", "icon");
  remote_handler_.FlushForTesting();
}
TEST_F(SkillsPageHandlerV2Test, GetProvidedSkills) {
  std::unordered_map<std::string, std::unique_ptr<Skill>> provided_skills;
  provided_skills.emplace(
      kProvidedSkillId,
      std::make_unique<Skill>(kProvidedSkillId, kProvidedSkillName,
                              kProvidedSkillIcon, kProvidedSkillPrompt));
  EXPECT_CALL(*mock_skills_service(), GetProvidedSkills())
      .WillOnce(ReturnRef(provided_skills));

  base::test::TestFuture<const std::vector<skills::Skill>&> future;
  remote_handler_->GetProvidedSkills(future.GetCallback());
  const auto& skills = future.Get();
  ASSERT_EQ(1u, skills.size());
  EXPECT_EQ(kProvidedSkillId, skills[0].id);
  EXPECT_EQ(kProvidedSkillName, skills[0].name);
}

TEST_F(SkillsPageHandlerV2Test, GetSkill) {
  Skill skill(kProvidedSkillId, kProvidedSkillName, kProvidedSkillIcon,
              kProvidedSkillPrompt);
  EXPECT_CALL(*mock_skills_service(), GetSkillById(kProvidedSkillId))
      .WillOnce(Return(&skill));

  base::test::TestFuture<const std::optional<skills::Skill>&> future;
  remote_handler_->GetProvidedSkill(kProvidedSkillId, future.GetCallback());
  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kProvidedSkillId, result->id);
  EXPECT_EQ(kProvidedSkillName, result->name);
}

TEST_F(SkillsPageHandlerV2Test, OnProvidedSkillsChanged) {
  MockSkillsPageV2 mock_page;
  remote_handler_->SetPage(mock_page.BindAndGetRemote());
  remote_handler_.FlushForTesting();

  std::unordered_map<std::string, std::unique_ptr<Skill>> provided_skills;
  provided_skills.emplace(
      kProvidedSkillId,
      std::make_unique<Skill>(kProvidedSkillId, kProvidedSkillName,
                              kProvidedSkillIcon, kProvidedSkillPrompt));
  EXPECT_CALL(*mock_skills_service(), GetProvidedSkills())
      .WillOnce(ReturnRef(provided_skills));

  EXPECT_CALL(mock_page, LoadProvidedSkills(_))
      .WillOnce([](const std::vector<skills::Skill>& skills) {
        ASSERT_EQ(1u, skills.size());
        EXPECT_EQ(kProvidedSkillId, skills[0].id);
        EXPECT_EQ(kProvidedSkillName, skills[0].name);
      });

  handler_->OnProvidedSkillsChanged(nullptr);
  mock_page.receiver_.FlushForTesting();
}

TEST_F(SkillsPageHandlerV2Test, OnSkillUpdated_NotifiesPage) {
  MockSkillsPageV2 mock_page;
  remote_handler_->SetPage(mock_page.BindAndGetRemote());
  remote_handler_.FlushForTesting();

  EXPECT_CALL(mock_page, OnUserSkillsUpdated()).Times(1);

  handler_->OnSkillUpdated("skill_id", SkillsService::UpdateSource::kSync,
                           /*is_position_changed=*/false);
  mock_page.receiver_.FlushForTesting();
}

TEST_F(SkillsPageHandlerV2Test, OnSkillUpdated_PageNotBound_NoCrash) {
  handler_->OnSkillUpdated("skill_id", SkillsService::UpdateSource::kSync,
                           /*is_position_changed=*/false);
}

}  // namespace
}  // namespace skills
