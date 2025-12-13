// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using ui::WebDialogWebContentsDelegate;

namespace {

class TestWebContentsDelegate : public WebDialogWebContentsDelegate {
 public:
  explicit TestWebContentsDelegate(content::BrowserContext* context)
      : WebDialogWebContentsDelegate(
            context,
            std::make_unique<ChromeWebContentsHandler>()) {}

  TestWebContentsDelegate(const TestWebContentsDelegate&) = delete;
  TestWebContentsDelegate& operator=(const TestWebContentsDelegate&) = delete;

  ~TestWebContentsDelegate() override = default;
};

class WebDialogWebContentsDelegateTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_web_contents_delegate_ =
        std::make_unique<TestWebContentsDelegate>(browser()->profile());
  }

  void TearDownOnMainThread() override {
    test_web_contents_delegate_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<TestWebContentsDelegate> test_web_contents_delegate_;
};

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest, DoNothingMethodsTest) {
  // None of the following calls should add more tabs or browsers.
  history::HistoryAddPageArgs should_add_args(
      GURL(), base::Time::Now(), 0, 0, std::nullopt, GURL(),
      history::RedirectList(), ui::PAGE_TRANSITION_TYPED, false,
      history::SOURCE_SYNCED, history::VisitResponseCodeCategory::kNot404,
      false, true);
  test_web_contents_delegate_->NavigationStateChanged(
      nullptr, content::InvalidateTypes(0));
  test_web_contents_delegate_->ActivateContents(nullptr);
  test_web_contents_delegate_->LoadingStateChanged(nullptr, true);
  test_web_contents_delegate_->CloseContents(nullptr);
  test_web_contents_delegate_->UpdateTargetURL(nullptr, GURL());
  test_web_contents_delegate_->SetContentsBounds(nullptr, gfx::Rect());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest, OpenURLFromTabTest) {
  test_web_contents_delegate_->OpenURLFromTab(
      nullptr,
      OpenURLParams(GURL(url::kAboutBlankURL), Referrer(),
                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                    ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
  // This should create a new foreground tab in the existing browser.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest,
                       AddNewContentsForegroundTabTest) {
  std::unique_ptr<WebContents> contents =
      WebContents::Create(WebContents::CreateParams(browser()->profile()));
  test_web_contents_delegate_->AddNewContents(
      nullptr, std::move(contents), GURL(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, blink::mojom::WindowFeatures(),
      false, nullptr);
  // This should create a new foreground tab in the existing browser.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(WebDialogWebContentsDelegateTest, DetachTest) {
  EXPECT_EQ(static_cast<content::BrowserContext*>(browser()->profile()),
            test_web_contents_delegate_->browser_context());
  test_web_contents_delegate_->Detach();
  EXPECT_EQ(nullptr, test_web_contents_delegate_->browser_context());
  // None of the following calls should add more tabs or browsers.
  GURL url(url::kAboutBlankURL);
  test_web_contents_delegate_->OpenURLFromTab(
      nullptr,
      OpenURLParams(url, Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
                    ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
  test_web_contents_delegate_->AddNewContents(
      nullptr, nullptr, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      blink::mojom::WindowFeatures(), false, nullptr);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
}

}  // namespace
