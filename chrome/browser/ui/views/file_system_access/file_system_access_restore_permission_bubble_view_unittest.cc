// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_restore_permission_bubble_view.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using AccessType = FileSystemAccessPermissionRequestManager::Access;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using RequestData = FileSystemAccessPermissionRequestManager::RequestData;
using RequestType = FileSystemAccessPermissionRequestManager::RequestType;

class FileSystemAccessRestorePermissionBubbleViewTest
    : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    AddTab(browser(), GURL("http://foo"));
    web_content_ = browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void TearDown() override {
    web_content_ = nullptr;
    TestWithBrowserView::TearDown();
  }

 protected:
  const RequestData kRequestData =
      RequestData(RequestType::kRestorePermissions,
                  url::Origin::Create(GURL("https://example.com")),
                  {{content::PathInfo(FILE_PATH_LITERAL("/foo/bar.txt")),
                    HandleType::kFile, AccessType::kRead}});
  raw_ptr<content::WebContents> web_content_;
};

TEST_F(FileSystemAccessRestorePermissionBubbleViewTest,
       AllowOnceButtonPressed) {
  permissions::PermissionAction callback_result;
  auto* bubble = GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      web_content_);
  bubble->OnButtonPressed(FileSystemAccessRestorePermissionBubbleView::
                              RestorePermissionButton::kAllowOnce);

  EXPECT_EQ(callback_result, permissions::PermissionAction::GRANTED_ONCE);
}

TEST_F(FileSystemAccessRestorePermissionBubbleViewTest,
       AllowAlwaysButtonPressed) {
  permissions::PermissionAction callback_result;
  auto* bubble = GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      web_content_);
  bubble->OnButtonPressed(FileSystemAccessRestorePermissionBubbleView::
                              RestorePermissionButton::kAllowAlways);

  EXPECT_EQ(callback_result, permissions::PermissionAction::GRANTED);
}

TEST_F(FileSystemAccessRestorePermissionBubbleViewTest, DenyButtonPressed) {
  permissions::PermissionAction callback_result;
  auto* bubble = GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      web_content_);
  bubble->OnButtonPressed(FileSystemAccessRestorePermissionBubbleView::
                              RestorePermissionButton::kDeny);

  EXPECT_EQ(callback_result, permissions::PermissionAction::DENIED);
}

TEST_F(FileSystemAccessRestorePermissionBubbleViewTest, CloseButtonPressed) {
  permissions::PermissionAction callback_result;
  auto* bubble = GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      web_content_);
  bubble->Close();

  EXPECT_EQ(callback_result, permissions::PermissionAction::DISMISSED);
}

TEST_F(FileSystemAccessRestorePermissionBubbleViewTest,
       BubbleDismissedOnNavigation) {
  permissions::PermissionAction callback_result;
  GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      web_content_);
  NavigateAndCommit(web_content_, GURL("http://bar"));

  EXPECT_EQ(callback_result, permissions::PermissionAction::DISMISSED);
}
