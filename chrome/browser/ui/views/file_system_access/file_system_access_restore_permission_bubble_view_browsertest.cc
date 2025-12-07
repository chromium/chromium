// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_restore_permission_bubble_view.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_outline.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_util.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

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

class FileSystemAccessBubbleSplitViewTest
    : public FileSystemAccessRestorePermissionBubbleViewTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSideBySide);
    FileSystemAccessRestorePermissionBubbleViewTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  GURL GetURL(const char* hostname) const {
    return embedded_test_server()->GetURL(hostname, "/title1.html");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessBubbleSplitViewTest,
                       ShowFileSystemAccessDialog) {
  ASSERT_TRUE(AddTabAtIndex(0, GetURL("example.com"),
                            ui::PageTransition::PAGE_TRANSITION_TYPED));
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0);
  tab_strip_model->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);

  std::vector<ContentsContainerView*> contents_container_views =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->multi_contents_view()
          ->contents_container_views();
  ASSERT_EQ(contents_container_views.size(), 2U);
  EXPECT_FALSE(
      contents_container_views[0]->contents_outline_view()->is_highlighted());
  EXPECT_FALSE(
      contents_container_views[1]->contents_outline_view()->is_highlighted());

  GetFileSystemAccessRestorePermissionDialogForTesting(
      kRequestData, base::DoNothing(), GetWebContents());

  EXPECT_TRUE(
      contents_container_views[0]->contents_outline_view()->is_highlighted());
  EXPECT_FALSE(
      contents_container_views[1]->contents_outline_view()->is_highlighted());
}
