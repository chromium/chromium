// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/ios/content/permission_prompt/permission_dialog_delegate.h"

#include "components/permissions/ios/content/permission_prompt/permission_prompt_test_util.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface PermissionDialogDelegate (Testing)
- (void)handleAllowAction;
- (void)handleAllowThisTimeAction;
- (void)handleBlockAction;
- (void)cleanup;
@end

namespace permissions {

class PermissionDialogDelegateTest : public content::RenderViewHostTestHarness {
 public:
  PermissionDialogDelegateTest() = default;
  ~PermissionDialogDelegateTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    SetContents(CreateTestWebContents());

    delegate_ = std::make_unique<MockPermissionPromptDelegate>();
    delegate_->AddRequest(
        std::make_unique<MockPermissionRequest>(RequestType::kGeolocation));

    prompt_ = std::make_unique<MockPermissionPromptIOS>(web_contents(),
                                                        delegate_.get());
  }

  void TearDown() override {
    dialog_delegate_ = nil;
    prompt_.reset();
    delegate_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  MockPermissionPromptDelegate* delegate() { return delegate_.get(); }
  MockPermissionPromptIOS* prompt() { return prompt_.get(); }

  PermissionDialogDelegate* CreateDialogDelegate() {
    dialog_delegate_ =
        [[PermissionDialogDelegate alloc] initWithPrompt:prompt()
                                             webContents:web_contents()];
    return dialog_delegate_;
  }

  __strong PermissionDialogDelegate* dialog_delegate_;

 private:
  std::unique_ptr<MockPermissionPromptDelegate> delegate_;
  std::unique_ptr<MockPermissionPromptIOS> prompt_;
};

TEST_F(PermissionDialogDelegateTest, Initialization) {
  auto* dialog_delegate = CreateDialogDelegate();
  EXPECT_NE(nullptr, dialog_delegate);
}

TEST_F(PermissionDialogDelegateTest, HandleAllowAction) {
  auto* dialog_delegate = CreateDialogDelegate();
  EXPECT_FALSE(delegate()->accept_called());

  [dialog_delegate handleAllowAction];
  EXPECT_TRUE(delegate()->accept_called());
}

TEST_F(PermissionDialogDelegateTest, HandleAllowThisTimeAction) {
  auto* dialog_delegate = CreateDialogDelegate();
  EXPECT_FALSE(delegate()->accept_this_time_called());

  [dialog_delegate handleAllowThisTimeAction];
  EXPECT_TRUE(delegate()->accept_this_time_called());
}

TEST_F(PermissionDialogDelegateTest, HandleBlockAction) {
  auto* dialog_delegate = CreateDialogDelegate();
  EXPECT_FALSE(delegate()->deny_called());

  [dialog_delegate handleBlockAction];
  EXPECT_TRUE(delegate()->deny_called());
}

TEST_F(PermissionDialogDelegateTest, Cleanup) {
  auto* dialog_delegate = CreateDialogDelegate();

  // Call cleanup - should not crash
  [dialog_delegate cleanup];
}

TEST_F(PermissionDialogDelegateTest, ActionsAfterCleanup) {
  auto* dialog_delegate = CreateDialogDelegate();

  [dialog_delegate cleanup];

  // These should not crash, though they may not call the delegate
  [dialog_delegate handleAllowAction];
  [dialog_delegate handleBlockAction];
}

}  // namespace permissions
