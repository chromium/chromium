// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/any_widget_observer.h"

// Tests ShoppingUiHandlerDelegate.
class ShoppingUiHandlerDelegateUiTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    profile_ = Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile_);
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToURL(
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    base::PlatformThread::Sleep(base::Seconds(2));
    base::RunLoop().RunUntilIdle();
  }

  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<bookmarks::BookmarkModel, DanglingUntriaged> bookmark_model_;
};

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateUiTest,
                       TestShowBookmarkEditorForCurrentUrl_WithBookmark) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr, profile_);
  const GURL url = GURL("https://www.example.com");
  NavigateToURL(url);

  const bookmarks::BookmarkNode* other_node = bookmark_model_->other_node();
  bookmark_model_->AddNewURL(other_node, other_node->children().size(), u"test",
                             url);

  auto bookmark_editor_waiter = views::NamedWidgetShownWaiter(
      views::test::AnyWidgetTestPasskey{}, BookmarkEditorView::kViewClassName);

  delegate->ShowBookmarkEditorForCurrentUrl();

  ASSERT_TRUE(bookmark_editor_waiter.WaitIfNeededAndGet());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateUiTest,
                       TestShowBookmarkEditorForCurrentUrl_WithoutBookmark) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr, profile_);
  const GURL url = GURL("https://www.example.com");
  NavigateToURL(url);

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  observer.set_shown_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        ASSERT_FALSE(widget->GetName() == BookmarkEditorView::kViewClassName);
      }));

  delegate->ShowBookmarkEditorForCurrentUrl();
}
