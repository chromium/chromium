// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"

using ::testing::Exactly;

namespace permissions {
namespace test {
class MockPermissionDialogJavaDelegate : public PermissionDialogJavaDelegate {
 public:
  MockPermissionDialogJavaDelegate() : PermissionDialogJavaDelegate(nullptr) {}
  MOCK_METHOD(void,
              CreateJavaDelegate,
              (content::WebContents * web_contents,
               PermissionDialogDelegate* owner),
              (override));
  MOCK_METHOD(void,
              CreateDialog,
              (content::WebContents * web_contents),
              (override));
  MOCK_METHOD(void, DismissDialog, (), (override));
};

class PermissionDialogDelegateTest : public content::RenderViewHostTestHarness {
};

TEST_F(PermissionDialogDelegateTest, DismissDialogWhenPrimaryPageChanges) {
  auto mock_permission_dialog_java_delegate =
      std::make_unique<testing::NiceMock<MockPermissionDialogJavaDelegate>>();
  EXPECT_CALL(*mock_permission_dialog_java_delegate, DismissDialog())
      .Times(Exactly(1));

  PermissionDialogDelegate* dialog = PermissionDialogDelegate::CreateForTesting(
      web_contents(), nullptr, std::move(mock_permission_dialog_java_delegate));

  // Use NavigationSimulator to simulate a navigation from the browser to make
  // sure that PermissionDialogDelegate::PrimaryPageChanged gets called.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(url::kAboutBlankURL));

  // Free PermissionDialogDelegate pointer.
  dialog->Destroy(nullptr, nullptr);
}

}  // namespace test
}  // namespace permissions
