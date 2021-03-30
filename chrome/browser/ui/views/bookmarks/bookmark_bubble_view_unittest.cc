// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

using bookmarks::BookmarkModel;

namespace {
const char kTestBookmarkURL[] = "http://www.google.com";
} // namespace

class BookmarkBubbleViewTest : public BrowserWithTestWindowTest {
 public:
  // The test executes the UI code for displaying a window that should be
  // executed on the UI thread. The test also hits the networking code that
  // fails without the IO thread. We pass the REAL_IO_THREAD option to run UI
  // and IO tasks on separate threads.
  BookmarkBubbleViewTest()
      : BrowserWithTestWindowTest(
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    anchor_widget_ =
        views::UniqueWidgetPtr(std::make_unique<ChromeTestWidget>());
    views::Widget::InitParams widget_params;
    widget_params.context = GetContext();
    anchor_widget_->Init(std::move(widget_params));

    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    bookmarks::AddIfNotBookmarked(bookmark_model, GURL(kTestBookmarkURL),
                                  std::u16string());
  }

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        BookmarkBubbleView::bookmark_bubble()->GetWidget());
    BookmarkBubbleView::bookmark_bubble()->GetWidget()->Close();
    destroyed_waiter.Wait();

    anchor_widget_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{BookmarkModelFactory::GetInstance(),
             BookmarkModelFactory::GetDefaultFactory()}};
  }

 protected:
  // Creates a bookmark bubble view.
  void CreateBubbleView() {
    // Create a fake anchor view for the bubble.
    BookmarkBubbleView::ShowBubble(anchor_widget_->GetContentsView(), nullptr,
                                   nullptr, nullptr, profile(),
                                   GURL(kTestBookmarkURL), true);
  }

 private:
  views::UniqueWidgetPtr anchor_widget_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleViewTest);
};

// Verifies that the sync promo is not displayed for a signed in user.
TEST_F(BookmarkBubbleViewTest, SyncPromoSignedIn) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()),
      "fake_username@gmail.com");
  CreateBubbleView();
  EXPECT_FALSE(
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting());
}

// Verifies that the sync promo is displayed for a user that is not signed in.
TEST_F(BookmarkBubbleViewTest, SyncPromoNotSignedIn) {
  CreateBubbleView();
  views::View* footnote =
      BookmarkBubbleView::bookmark_bubble()->GetFootnoteViewForTesting();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(footnote);
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(footnote);
#endif
}
