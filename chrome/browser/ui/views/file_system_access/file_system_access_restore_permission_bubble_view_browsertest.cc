// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_restore_permission_bubble_view.h"

#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using AccessType = FileSystemAccessPermissionRequestManager::Access;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using RequestData = FileSystemAccessPermissionRequestManager::RequestData;
using RequestType = FileSystemAccessPermissionRequestManager::RequestType;

class FileSystemAccessRestorePermissionBubbleViewTest
    : public InProcessBrowserTest {
 public:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

 protected:
  const RequestData kRequestData =
      RequestData(RequestType::kRestorePermissions,
                  url::Origin::Create(GURL("https://example.com")),
                  {{content::PathInfo(FILE_PATH_LITERAL("/foo/bar.txt")),
                    HandleType::kFile, AccessType::kRead}});
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessRestorePermissionBubbleViewTest,
                       AllowOnceButtonPressed) {
  permissions::PermissionAction callback_result;
  auto* bubble = GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      GetWebContents());
  bubble->OnButtonPressed(FileSystemAccessRestorePermissionBubbleView::
                              RestorePermissionButton::kAllowOnce);

  EXPECT_EQ(callback_result, permissions::PermissionAction::GRANTED_ONCE);
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessRestorePermissionBubbleViewTest,
                       AllowAlwaysButtonPressed) {
  permissions::PermissionAction callback_result;
  auto* bubble = GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      GetWebContents());
  bubble->OnButtonPressed(FileSystemAccessRestorePermissionBubbleView::
                              RestorePermissionButton::kAllowAlways);

  EXPECT_EQ(callback_result, permissions::PermissionAction::GRANTED);
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessRestorePermissionBubbleViewTest,
                       DenyButtonPressed) {
  permissions::PermissionAction callback_result;
  auto* bubble = GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      GetWebContents());
  bubble->OnButtonPressed(FileSystemAccessRestorePermissionBubbleView::
                              RestorePermissionButton::kDeny);

  EXPECT_EQ(callback_result, permissions::PermissionAction::DENIED);
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessRestorePermissionBubbleViewTest,
                       CloseButtonPressed) {
  permissions::PermissionAction callback_result;
  auto* bubble = GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      GetWebContents());
  bubble->Close();

  EXPECT_EQ(callback_result, permissions::PermissionAction::DISMISSED);
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessRestorePermissionBubbleViewTest,
                       BubbleDismissedOnNavigation) {
  permissions::PermissionAction callback_result;
  GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData,
      base::BindLambdaForTesting([&](permissions::PermissionAction result) {
        callback_result = result;
      }),
      GetWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("http://bar")));

  EXPECT_EQ(callback_result, permissions::PermissionAction::DISMISSED);
}
