// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_handler.h"

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

// Mock implementation for the BaseDialogUIDelegate interface.
class MockBaseDialogUIDelegate : public BaseDialogUIDelegate {
 public:
  ~MockBaseDialogUIDelegate() override = default;

  MOCK_METHOD(void, ResizeNativeView, (int height), (override));
  MOCK_METHOD(void, ShowNativeView, (), (override));
  MOCK_METHOD(void, CloseNativeView, (), (override));
};

class PrivacySandboxBaseDialogHandlerTest : public testing::Test {
 public:
  PrivacySandboxBaseDialogHandlerTest() = default;

 protected:
  MockBaseDialogUIDelegate mock_delegate_;
  BaseDialogHandler handler_{mojo::NullReceiver(), &mock_delegate_};
};

TEST_F(PrivacySandboxBaseDialogHandlerTest, ShowDialog) {
  EXPECT_CALL(mock_delegate_, ShowNativeView()).Times(1);
  handler_.ShowDialog();
}

TEST_F(PrivacySandboxBaseDialogHandlerTest, CloseDialog) {
  EXPECT_CALL(mock_delegate_, CloseNativeView()).Times(1);
  handler_.CloseDialog();
}

TEST_F(PrivacySandboxBaseDialogHandlerTest, ShowThenCloseDialog) {
  EXPECT_CALL(mock_delegate_, ShowNativeView()).Times(1);
  EXPECT_CALL(mock_delegate_, CloseNativeView()).Times(1);
  handler_.ShowDialog();
  handler_.CloseDialog();
}

TEST_F(PrivacySandboxBaseDialogHandlerTest, ResizeDialog) {
  const int kTestHeight = 500;
  const int kTestHeight2 = 400;

  EXPECT_CALL(mock_delegate_, ResizeNativeView(kTestHeight)).Times(1);
  handler_.ResizeDialog(kTestHeight);
  // Crashes if ResizeDialog is called twice.
  EXPECT_DEATH_IF_SUPPORTED(handler_.ResizeDialog(kTestHeight2), "");
}

class PrivacySandboxBaseDialogHandlerNullDelegateTest : public testing::Test {
 public:
  PrivacySandboxBaseDialogHandlerNullDelegateTest() = default;

 protected:
  BaseDialogHandler handler_{mojo::NullReceiver(), nullptr};
};

TEST_F(PrivacySandboxBaseDialogHandlerNullDelegateTest, ShowDialog) {
  EXPECT_NO_FATAL_FAILURE(handler_.ShowDialog());
}

TEST_F(PrivacySandboxBaseDialogHandlerNullDelegateTest, CloseDialog) {
  EXPECT_NO_FATAL_FAILURE(handler_.CloseDialog());
}

TEST_F(PrivacySandboxBaseDialogHandlerNullDelegateTest, ResizeDialog) {
  const int kTestHeight = 500;
  EXPECT_NO_FATAL_FAILURE(handler_.ResizeDialog(kTestHeight));
}

}  // namespace
}  // namespace privacy_sandbox
