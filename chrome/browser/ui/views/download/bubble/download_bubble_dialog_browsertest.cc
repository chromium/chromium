// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"

#include <memory>
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/slow_download_http_response.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_utils.h"
#include "url/gurl.h"

namespace {
enum Testcase {
  kInProgressDownload,
  kCompletedDownload,
  kFullView,
  kDangerousDownloadSubpage
};

auto kNameToTestcase = base::MakeFixedFlatMap<std::string, Testcase>({
    {"InProgressDownload", kInProgressDownload},
    {"CompletedDownload", kCompletedDownload},
    {"FullView", kFullView},
    {"DangerousDownloadSubpage", kDangerousDownloadSubpage},
});

constexpr base::StringPiece kDangerousUrl =
    "/downloads/dangerous/dangerous.swf";
}  // namespace

class DownloadBubbleDialogBrowserTest : public DialogBrowserTest {
 public:
  DownloadBubbleDialogBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{safe_browsing::kDownloadBubble},
        /*disabled_features=*/{});
  }
  DownloadBubbleDialogBrowserTest(const DownloadBubbleDialogBrowserTest& test) =
      delete;
  DownloadBubbleDialogBrowserTest& operator=(
      const DownloadBubbleDialogBrowserTest& test) = delete;

  DownloadToolbarButtonView* GetDownloadToolbarButtonView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return (browser_view && browser_view->toolbar())
               ? browser_view->toolbar()->download_button()
               : nullptr;
  }

  void ClickDownloadItem() {
    DownloadToolbarButtonView* button = GetDownloadToolbarButtonView();
    const views::View::Views& row_list =
        button->download_row_list_view_->children();
    ASSERT_EQ(row_list.size(), 1u);
    DownloadBubbleRowView* row =
        static_cast<DownloadBubbleRowView*>(row_list[0]);

    base::RunLoop dangerous_wait;
    row->SetNotifyDangerousDownloadCallbackForTesting(
        dangerous_wait.QuitClosure());
    dangerous_wait.Run();

    ui::test::EventGenerator generator(
        GetRootWindow(button->bubble_delegate_->GetWidget()));
    generator.MoveMouseTo(row->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();
  }

  void ClickDownloadToolbarButton(bool is_creation) {
    DownloadToolbarButtonView* button = GetDownloadToolbarButtonView();
    base::RunLoop wait;
    if (is_creation) {
      button->SetBubbleCreatedCallbackForTesting(wait.QuitClosure());
    } else {
      button->SetBubbleDestroyedCallbackForTesting(wait.QuitClosure());
    }

    ui::test::EventGenerator generator(GetRootWindow(button->GetWidget()));
    generator.MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();
    wait.Run();
  }

  content::DownloadManager::DownloadVector GetDownloadItems() {
    content::DownloadManager::DownloadVector items;
    browser()->profile()->GetDownloadManager()->GetAllDownloads(&items);
    return items;
  }

  void StartServer() {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    embedded_test_server()->ServeFilesFromDirectory(test_file_directory);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void StartDownload(base::StringPiece url, bool observe_terminally) {
    std::unique_ptr<content::DownloadTestObserver> observer;
    if (observe_terminally) {
      observer = std::make_unique<content::DownloadTestObserverTerminal>(
          browser()->profile()->GetDownloadManager(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
    } else {
      observer = std::make_unique<content::DownloadTestObserverInProgress>(
          browser()->profile()->GetDownloadManager(), 1);
    }

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), embedded_test_server()->GetURL(url),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    observer->WaitForFinished();
  }

  void WaitForBubbleCreation() {
    // Download Toolbar button should exist.
    ASSERT_NE(GetDownloadToolbarButtonView(), nullptr);
    base::RunLoop creation_wait;
    GetDownloadToolbarButtonView()->SetBubbleCreatedCallbackForTesting(
        creation_wait.QuitClosure());
    creation_wait.Run();
  }

  void CompleteSlowDownload() {
    content::DownloadTestObserverTerminal observer(
        browser()->profile()->GetDownloadManager(), 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        embedded_test_server()->GetURL(
            content::SlowHttpResponse::kFinishSlowResponseUrl),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    observer.WaitForFinished();
  }

  void SetUp() override {
    StartServer();
    InProcessBrowserTest::SetUp();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::DownloadManager::DownloadVector items;

    auto* testcase_iter = kNameToTestcase.find(name);
    ASSERT_TRUE(testcase_iter != kNameToTestcase.end());
    testcase_ = testcase_iter->second;
    switch (testcase_) {
      case Testcase::kInProgressDownload:
        StartDownload(content::SlowDownloadHttpResponse::kKnownSizeUrl,
                      /*observe_terminally=*/false);
        WaitForBubbleCreation();
        break;
      case Testcase::kCompletedDownload:
        StartDownload(content::SlowDownloadHttpResponse::kKnownSizeUrl,
                      /*observe_terminally=*/false);
        WaitForBubbleCreation();
        EXPECT_TRUE(GetDownloadToolbarButtonView()->IsBubbleVisible());
        ClickDownloadToolbarButton(/*is_creation=*/false);
        EXPECT_FALSE(GetDownloadToolbarButtonView()->IsBubbleVisible());
        items = GetDownloadItems();
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0]->GetState(), download::DownloadItem::IN_PROGRESS);
        CompleteSlowDownload();
        WaitForBubbleCreation();
        break;
      case Testcase::kFullView:
        StartDownload(content::SlowDownloadHttpResponse::kKnownSizeUrl,
                      /*observe_terminally=*/false);
        CompleteSlowDownload();
        WaitForBubbleCreation();
        EXPECT_TRUE(GetDownloadToolbarButtonView()->IsBubbleVisible());
        ClickDownloadToolbarButton(/*is_creation=*/false);
        EXPECT_FALSE(GetDownloadToolbarButtonView()->IsBubbleVisible());
        ClickDownloadToolbarButton(/*is_creation=*/true);
        break;
      case Testcase::kDangerousDownloadSubpage:
        StartDownload(kDangerousUrl, /*observe_terminally=*/true);
        WaitForBubbleCreation();
        ClickDownloadItem();
        break;
    }
  }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi())
      return false;
    DownloadToolbarButtonView* button = GetDownloadToolbarButtonView();
    content::DownloadManager::DownloadVector items = GetDownloadItems();
    if (items.size() != 1u) {
      LOG(ERROR) << "There should be only one download item, not "
                 << items.size();
      return false;
    }
    switch (testcase_) {
      case Testcase::kInProgressDownload:
        EXPECT_TRUE(button->IsBubbleVisible());
        EXPECT_TRUE(button->is_primary_partial_view_);
        EXPECT_FALSE(button->security_view_->GetVisible());
        EXPECT_EQ(button->download_list_size_, 1u);
        EXPECT_EQ(items[0]->GetState(), download::DownloadItem::IN_PROGRESS);
        items[0]->Cancel(true);
        return true;
      case Testcase::kCompletedDownload:
        EXPECT_TRUE(button->IsBubbleVisible());
        EXPECT_TRUE(button->is_primary_partial_view_);
        EXPECT_FALSE(button->security_view_->GetVisible());
        EXPECT_EQ(button->download_list_size_, 1u);
        EXPECT_EQ(items[0]->GetState(), download::DownloadItem::COMPLETE);
        return true;
      case Testcase::kFullView:
        EXPECT_TRUE(button->IsBubbleVisible());
        EXPECT_FALSE(button->is_primary_partial_view_);
        EXPECT_FALSE(button->security_view_->GetVisible());
        EXPECT_EQ(button->download_list_size_, 1u);
        EXPECT_EQ(items[0]->GetState(), download::DownloadItem::COMPLETE);
        return true;
      case Testcase::kDangerousDownloadSubpage:
        EXPECT_TRUE(button->IsBubbleVisible());
        EXPECT_TRUE(button->is_primary_partial_view_);
        EXPECT_TRUE(button->security_view_->GetVisible());
        EXPECT_EQ(button->download_list_size_, 1u);
        items[0]->Cancel(true);
        return true;
    }
    return false;
  }

  void TearDown() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDown();
  }

 private:
  Testcase testcase_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DownloadBubbleDialogBrowserTest,
                       InvokeUi_InProgressDownload) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleDialogBrowserTest,
                       InvokeUi_CompletedDownload) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleDialogBrowserTest, InvokeUi_FullView) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleDialogBrowserTest,
                       InvokeUi_DangerousDownloadSubpage) {
  ShowAndVerifyUi();
}
