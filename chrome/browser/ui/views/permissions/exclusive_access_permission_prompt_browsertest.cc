// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_browser_test_util.h"
#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "content/public/test/browser_test.h"

using testing::_;

class PermissionPromptDelegate : public TestPermissionBubbleViewDelegate {
 public:
  explicit PermissionPromptDelegate(Browser* browser) : browser_(browser) {}

  content::WebContents* GetAssociatedWebContents() override {
    return browser_->tab_strip_model()->GetActiveWebContents();
  }

  void Accept() override {
    for (auto request : Requests()) {
      request->PermissionGranted(/*is_one_time=*/false);
    }
  }

  void AcceptThisTime() override {
    for (auto request : Requests()) {
      request->PermissionGranted(/*is_one_time=*/true);
    }
  }

  void Deny() override {
    for (auto request : Requests()) {
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
  std::unique_ptr<ExclusiveAccessPermissionPrompt> CreatePrompt(
      std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
          requests) {
    prompt_delegate_->set_requests(requests);
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
  permissions::PermissionRequest keyboard_request_{
      GURL("https://example.com"), permissions::RequestType::kKeyboardLock,
      /*has_gesture=*/false, keyboard_callback_.Get(), base::OnceClosure()};
  base::MockCallback<permissions::PermissionRequest::PermissionDecidedCallback>
      pointer_callback_;
  permissions::PermissionRequest pointer_request_{
      GURL("https://example.com"), permissions::RequestType::kPointerLock,
      /*has_gesture=*/false, pointer_callback_.Get(), base::OnceClosure()};
  std::unique_ptr<PermissionPromptDelegate> prompt_delegate_;
};

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowPermisison) {
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt({&keyboard_request_});
  EXPECT_CALL(keyboard_callback_,
              Run(CONTENT_SETTING_ALLOW, /*is_one_time=*/false, _));
  PressAllowButton(prompt.get());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowPermisisonThisTime) {
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt({&keyboard_request_});
  EXPECT_CALL(keyboard_callback_,
              Run(CONTENT_SETTING_ALLOW, /*is_one_time=*/true, _));
  PressAllowThisTimeButton(prompt.get());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       DenyPermisison) {
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt({&keyboard_request_});
  EXPECT_CALL(keyboard_callback_,
              Run(CONTENT_SETTING_BLOCK, /*is_one_time=*/false, _));
  PressDenyButton(prompt.get());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       AllowMultiplePermisisons) {
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt({&keyboard_request_, &pointer_request_});
  EXPECT_CALL(keyboard_callback_,
              Run(CONTENT_SETTING_ALLOW, /*is_one_time=*/false, _));
  EXPECT_CALL(pointer_callback_,
              Run(CONTENT_SETTING_ALLOW, /*is_one_time=*/false, _));
  PressAllowButton(prompt.get());
}

IN_PROC_BROWSER_TEST_F(ExclusiveAccessPermissionPromptInteractiveTest,
                       DenyMultiplePermisisons) {
  std::unique_ptr<ExclusiveAccessPermissionPrompt> prompt =
      CreatePrompt({&keyboard_request_, &pointer_request_});
  EXPECT_CALL(keyboard_callback_,
              Run(CONTENT_SETTING_BLOCK, /*is_one_time=*/false, _));
  EXPECT_CALL(pointer_callback_,
              Run(CONTENT_SETTING_BLOCK, /*is_one_time=*/false, _));
  PressDenyButton(prompt.get());
}
