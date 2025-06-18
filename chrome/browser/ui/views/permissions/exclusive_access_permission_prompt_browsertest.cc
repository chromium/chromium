// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt.h"

#include <memory>
#include <vector>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_browser_test_util.h"
#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "content/public/test/browser_test.h"

using testing::_;

class PermissionPromptDelegate : public TestPermissionBubbleViewDelegate {
 public:
  explicit PermissionPromptDelegate(Browser* browser) : browser_(browser) {}

  content::WebContents* GetAssociatedWebContents() override {
    return browser_->tab_strip_model()->GetActiveWebContents();
  }

  void Accept() override {
    for (const auto& request : Requests()) {
      request->PermissionGranted(/*is_one_time=*/false);
    }
  }

  void AcceptThisTime() override {
    for (const auto& request : Requests()) {
      request->PermissionGranted(/*is_one_time=*/true);
    }
  }

  void Deny() override {
    for (const auto& request : Requests()) {
      request->PermissionDenied();
    }
  }

 private:
  raw_ptr<Browser> browser_;
};

class ExclusiveAccessPermissionPromptInteractiveTest
    : public InProcessBrowserTest {
 public:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    prompt_delegate_ = std::make_unique<PermissionPromptDelegate>(browser());
  }

  void PostRunTestOnMainThread() override {
    prompt_delegate_.reset();
    InProcessBrowserTest::PostRunTestOnMainThread();
  }

 protected:
  std::unique_ptr<permissions::PermissionRequest> CreateKeyboardRequest() {
    return std::make_unique<permissions::PermissionRequest>(
        std::make_unique<permissions::PermissionRequestData>(
            std::make_unique<permissions::ContentSettingPermissionResolver>(
                ContentSettingsType::KEYBOARD_LOCK),
            /*user_gesture=*/false, GURL("https://example.com")),
        keyboard_callback_.Get());
  }

  base::MockCallback<permissions::PermissionRequest::PermissionDecidedCallback>
      pointer_callback_;

  std::unique_ptr<permissions::PermissionRequest> CreatePointerRequest() {
    return std::make_unique<permissions::PermissionRequest>(
        std::make_unique<permissions::PermissionRequestData>(
            std::make_unique<permissions::ContentSettingPermissionResolver>(
                ContentSettingsType::POINTER_LOCK),
            /*user_gesture=*/false, GURL("https://example.com")),
        pointer_callback_.Get());
  }

  std::unique_ptr<ExclusiveAccessPermissionPrompt> CreatePrompt(
      std::vector<std::unique_ptr<permissions::PermissionRequest>> requests) {
    prompt_delegate_->set_requests(std::move(requests));
    return std::make_unique<ExclusiveAccessPermissionPrompt>(
        browser(), browser()->tab_strip_model()->GetActiveWebContents(),
        prompt_delegate_.get());
  }

  void PressAllowButton(ExclusiveAccessPermissionPrompt* prompt) {
    PressButton(prompt,
                ExclusiveAccessPermissionPromptView::ButtonType::kAlwaysAllow);
  }

  void PressAllowThisTimeButton(ExclusiveAccessPermissionPrompt* prompt) {
    PressButton(
        prompt,
        ExclusiveAccessPermissionPromptView::ButtonType::kAllowThisTime);
  }

  void PressDenyButton(ExclusiveAccessPermissionPrompt* prompt) {
    PressButton(prompt,
                ExclusiveAccessPermissionPromptView::ButtonType::kNeverAllow);
  }

  void PressButton(
      ExclusiveAccessPermissionPrompt* prompt,
      ExclusiveAccessPermissionPromptView::ButtonType button_type) {
    prompt->GetViewForTesting()->RunButtonCallback(
        static_cast<int>(button_type));
  }

  base::MockCallback<permissions::PermissionRequest::PermissionDecidedCallback>
      keyboard_callback_;

  std::unique_ptr<PermissionPromptDelegate> prompt_delegate_;
};

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowPermission) {
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.emplace_back(CreateKeyboardRequest());
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt(std::move(requests));
  EXPECT_CALL(keyboard_callback_, Run(PermissionDecision::kAllow, _, _));
  PressAllowButton(prompt.get());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowPermissionThisTime) {
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.emplace_back(CreateKeyboardRequest());
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt(std::move(requests));
  EXPECT_CALL(keyboard_callback_,
              Run(PermissionDecision::kAllowThisTime, _, _));
  PressAllowThisTimeButton(prompt.get());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       DenyPermisison) {
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.emplace_back(CreateKeyboardRequest());
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt(std::move(requests));
  EXPECT_CALL(keyboard_callback_, Run(PermissionDecision::kDeny, _, _));
  PressDenyButton(prompt.get());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowMultiplePermisisons) {
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.emplace_back(CreateKeyboardRequest());
  requests.emplace_back(CreatePointerRequest());
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt(std::move(requests));
  EXPECT_CALL(keyboard_callback_, Run(PermissionDecision::kAllow, _, _));
  EXPECT_CALL(pointer_callback_, Run(PermissionDecision::kAllow, _, _));
  PressAllowButton(prompt.get());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       DenyMultiplePermisisons) {
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.emplace_back(CreateKeyboardRequest());
  requests.emplace_back(CreatePointerRequest());
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt(std::move(requests));
  EXPECT_CALL(keyboard_callback_, Run(PermissionDecision::kDeny, _, _));
  EXPECT_CALL(pointer_callback_, Run(PermissionDecision::kDeny, _, _));
  PressDenyButton(prompt.get());
}
