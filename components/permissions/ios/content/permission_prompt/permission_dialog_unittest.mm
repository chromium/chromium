// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/ios/content/permission_prompt/permission_dialog.h"

#include "base/strings/sys_string_conversions.h"
#include "components/permissions/ios/content/permission_prompt/permission_prompt_test_util.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

class PermissionDialogTest : public content::RenderViewHostTestHarness {
 public:
  PermissionDialogTest() = default;
  ~PermissionDialogTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Create test web contents
    SetContents(CreateTestWebContents());
    delegate_ = std::make_unique<MockPermissionPromptDelegate>();

    // Add a default request so dialog creation doesn't fail
    AddDefaultRequest();
  }

  void TearDown() override {
    delegate_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  MockPermissionPromptDelegate* delegate() { return delegate_.get(); }

  void AddDefaultRequest() {
    delegate_->AddRequest(CreateMockRequest(RequestType::kGeolocation));
  }

  std::unique_ptr<PermissionDialog> CreateDialog() {
    return PermissionDialog::Create(web_contents(), delegate());
  }

  std::unique_ptr<MockPermissionRequest> CreateMockRequest(
      RequestType request_type = RequestType::kGeolocation) {
    return std::make_unique<MockPermissionRequest>(request_type);
  }

 private:
  std::unique_ptr<MockPermissionPromptDelegate> delegate_;
};

TEST_F(PermissionDialogTest, CreateReturnsValidDialog) {
  auto dialog = CreateDialog();
  EXPECT_NE(nullptr, dialog.get());
}

// Test button text for non-one-time permissions
TEST_F(PermissionDialogTest, GetPositiveButtonTextAllow) {
  auto dialog = CreateDialog();
  NSString* text = dialog->GetPositiveButtonText(false);
  NSString* expected =
      base::SysUTF16ToNSString(l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  EXPECT_TRUE([text isEqualToString:expected]);
}
TEST_F(PermissionDialogTest, GetNegativeButtonTextDeny) {
  auto dialog = CreateDialog();
  NSString* text = dialog->GetNegativeButtonText(false);
  NSString* expected =
      base::SysUTF16ToNSString(l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
  EXPECT_TRUE([text isEqualToString:expected]);
}

// Test button text for one-time permissions
TEST_F(PermissionDialogTest, GetPositiveButtonTextOneTime) {
  auto dialog = CreateDialog();
  NSString* text = dialog->GetPositiveButtonText(true);
  NSString* expected = base::SysUTF16ToNSString(
      l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_WHILE_VISITING));
  EXPECT_TRUE([text isEqualToString:expected]);
}
TEST_F(PermissionDialogTest, GetNegativeButtonTextOneTime) {
  auto dialog = CreateDialog();
  NSString* text = dialog->GetNegativeButtonText(true);
  NSString* expected = base::SysUTF16ToNSString(
      l10n_util::GetStringUTF16(IDS_PERMISSION_NEVER_ALLOW));
  EXPECT_TRUE([text isEqualToString:expected]);
}

// Test ephemeral button text
TEST_F(PermissionDialogTest, GetPositiveEphemeralButtonTextOneTime) {
  auto dialog = CreateDialog();
  NSString* text = dialog->GetPositiveEphemeralButtonText(true);
  NSString* expected = base::SysUTF16ToNSString(
      l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME));
  EXPECT_TRUE([text isEqualToString:expected]);
}
TEST_F(PermissionDialogTest, GetPositiveEphemeralButtonTextNotOneTime) {
  auto dialog = CreateDialog();
  NSString* text = dialog->GetPositiveEphemeralButtonText(false);
  EXPECT_EQ(0u, [text length]);
}

TEST_F(PermissionDialogTest, DialogDelegateIsCreated) {
  auto dialog = CreateDialog();
  EXPECT_NE(nullptr, dialog->permission_dialog_delegate());
}

}  // namespace permissions
