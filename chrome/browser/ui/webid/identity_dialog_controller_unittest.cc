// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

class IdentityDialogControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  IdentityDialogControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~IdentityDialogControllerTest() override = default;
  IdentityDialogControllerTest(IdentityDialogControllerTest&) = delete;
  IdentityDialogControllerTest& operator=(IdentityDialogControllerTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  void WaitForBubbleToBeShown(permissions::PermissionRequestManager* manager) {
    manager->DocumentOnLoadCompletedInPrimaryMainFrame();
    task_environment()->RunUntilIdle();
  }

  void Accept(permissions::PermissionRequestManager* manager) {
    manager->Accept();
    task_environment()->RunUntilIdle();
  }

  void Deny(permissions::PermissionRequestManager* manager) {
    manager->Deny();
    task_environment()->RunUntilIdle();
  }

  void Dismiss(permissions::PermissionRequestManager* manager) {
    manager->Dismiss();
    task_environment()->RunUntilIdle();
  }
};

TEST_F(IdentityDialogControllerTest, Accept) {
  IdentityDialogController controller(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(true)).WillOnce(testing::Return());
  controller.RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Accept(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

TEST_F(IdentityDialogControllerTest, Deny) {
  IdentityDialogController controller(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  controller.RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Deny(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

TEST_F(IdentityDialogControllerTest, Dismiss) {
  IdentityDialogController controller(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  controller.RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Dismiss(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}
