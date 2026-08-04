// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler_v2.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/skills/public/skill.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace skills {
namespace {

class MockSkillsDialogDelegate : public SkillsDialogDelegate {
 public:
  MOCK_METHOD(void, CloseDialog, (), (override));
  MOCK_METHOD(void, OnSkillSaved, (const std::string& skill_id), (override));
  MOCK_METHOD(void, OnSkillDeleted, (const std::string& skill_id), (override));
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
  MOCK_METHOD(void,
              InvokeSkill,
              (std::string_view skill_id,
               std::string_view skill_name,
               std::string_view skill_icon),
              (override));
  MOCK_METHOD(void, SendPrompt, (std::string_view prompt), (override));
};

class SkillsPageHandlerV2Test : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    handler_ = std::make_unique<SkillsPageHandlerV2>(
        remote_handler_.BindNewPipeAndPassReceiver(), profile(),
        identity_test_env_.identity_manager(), web_contents());
  }

  void TearDown() override {
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
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
                          std::string_view("test_icon")))
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

TEST_F(SkillsPageHandlerV2Test, CloseDialog) {
  MockSkillsDialogDelegate mock_delegate;
  base::WeakPtrFactory<SkillsDialogDelegate> weak_factory(&mock_delegate);

  mojo::Remote<::skills::mojom::SkillsPageHandler> remote_handler;
  auto handler = std::make_unique<SkillsPageHandlerV2>(
      remote_handler.BindNewPipeAndPassReceiver(), profile(),
      identity_test_env_.identity_manager(), web_contents(),
      weak_factory.GetWeakPtr());

  EXPECT_CALL(mock_delegate, CloseDialog()).Times(1);

  remote_handler->CloseDialog();
  remote_handler.FlushForTesting();
}

}  // namespace
}  // namespace skills
