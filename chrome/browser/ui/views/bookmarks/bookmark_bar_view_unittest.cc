// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/browser/ui/views/read_later/read_later_button.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

class BookmarkBarViewBaseTest : public ChromeViewsTestBase {
 public:
  BookmarkBarViewBaseTest() {
    feature_list_.InitAndEnableFeature(reading_list::switches::kReadLater);

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(
            &BookmarkBarViewBaseTest::CreateTemplateURLService));
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    Browser::CreateParams params(profile(), true);
    params.window = &browser_window_;
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
  }

  virtual BookmarkBarView* bookmark_bar_view() = 0;

  TestingProfile* profile() { return profile_.get(); }
  Browser* browser() { return browser_.get(); }

 protected:
  // Returns a string containing the label of each of the *visible* buttons on
  // the bookmark bar. Each label is separated by a space.
  std::string GetStringForVisibleButtons() {
    std::string result;
    for (size_t i = 0; i < test_helper_->GetBookmarkButtonCount() &&
                       test_helper_->GetBookmarkButton(i)->GetVisible();
         ++i) {
      if (i != 0)
        result += " ";
      result +=
          base::UTF16ToASCII(test_helper_->GetBookmarkButton(i)->GetText());
    }
    return result;
  }

  // Continues sizing the bookmark bar until it has |count| buttons that are
  // visible.
  // NOTE: if the model has more than |count| buttons this results in
  // |count| + 1 buttons.
  void SizeUntilButtonsVisible(size_t count) {
    const int start_width = bookmark_bar_view()->width();
    const int height = bookmark_bar_view()->GetPreferredSize().height();
    for (size_t i = 0;
         i < 100 && (test_helper_->GetBookmarkButtonCount() < count ||
                     !test_helper_->GetBookmarkButton(count - 1)->GetVisible());
         ++i) {
      bookmark_bar_view()->SetBounds(0, 0, start_width + i * 10, height);
      bookmark_bar_view()->Layout();
    }
  }

  void WaitForBookmarkModelToLoad() {
    bookmarks::test::WaitForBookmarkModelToLoad(
        BookmarkModelFactory::GetForBrowserContext(profile()));
  }

  // Adds nodes to the bookmark bar node from |string|. See
  // bookmarks::test::AddNodesFromModelString() for details on |string|.
  void AddNodesToBookmarkBarFromModelString(const std::string& string) {
    BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::AddNodesFromModelString(model, model->bookmark_bar_node(),
                                             string);
  }

  // Creates the model, blocking until it loads, then creates the
  // BookmarkBarView.
  std::unique_ptr<BookmarkBarView> CreateBookmarkModelAndBookmarkBarView() {
    WaitForBookmarkModelToLoad();

    auto bookmark_bar_view =
        std::make_unique<BookmarkBarView>(browser(), nullptr);
    test_helper_ =
        std::make_unique<BookmarkBarViewTestHelper>(bookmark_bar_view.get());
    return bookmark_bar_view;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  TestBrowserWindow browser_window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<BookmarkBarViewTestHelper> test_helper_;

 private:
  static std::unique_ptr<KeyedService> CreateTemplateURLService(
      content::BrowserContext* profile) {
    return std::make_unique<TemplateURLService>(
        static_cast<Profile*>(profile)->GetPrefs(),
        std::make_unique<SearchTermsData>(),
        nullptr /* KeywordWebDataService */,
        nullptr /* TemplateURLServiceClient */, base::RepeatingClosure());
  }
};

class BookmarkBarViewTest : public BookmarkBarViewBaseTest {
 public:
  BookmarkBarViewTest() = default;
  BookmarkBarViewTest(const BookmarkBarViewTest&) = delete;
  BookmarkBarViewTest& operator=(const BookmarkBarViewTest&) = delete;
  ~BookmarkBarViewTest() override = default;

  // BookmarkBarViewBaseTest
  void SetUp() override {
    BookmarkBarViewBaseTest::SetUp();

    bookmark_bar_view_ = CreateBookmarkModelAndBookmarkBarView();
  }

  void TearDown() override {
    BookmarkBarViewBaseTest::TearDown();

    bookmark_bar_view_.reset();
  }

  BookmarkBarView* bookmark_bar_view() override {
    return bookmark_bar_view_.get();
  }

 private:
  std::unique_ptr<BookmarkBarView> bookmark_bar_view_;
};

class BookmarkBarViewInWidgetTest : public BookmarkBarViewBaseTest {
 public:
  BookmarkBarViewInWidgetTest() = default;
  BookmarkBarViewInWidgetTest(const BookmarkBarViewInWidgetTest&) = delete;
  BookmarkBarViewInWidgetTest& operator=(const BookmarkBarViewInWidgetTest&) =
      delete;
  ~BookmarkBarViewInWidgetTest() override = default;

  // BookmarkBarViewBaseTest
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    BookmarkBarViewBaseTest::SetUp();

    widget_ = CreateTestWidget();
    bookmark_bar_view_ =
        widget_->SetContentsView(CreateBookmarkModelAndBookmarkBarView());
  }

  void TearDown() override {
    widget_.reset();

    BookmarkBarViewBaseTest::TearDown();
  }

  BookmarkBarView* bookmark_bar_view() override { return bookmark_bar_view_; }

  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  BookmarkBarView* bookmark_bar_view_ = nullptr;
};

// Verify that in instant extended mode the visibility of the apps shortcut
// button properly follows the pref value.
TEST_F(BookmarkBarViewTest, AppsShortcutVisibility) {
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());

  // Try to make the Apps shortcut visible. Its visibility depends on whether
  // the app launcher is enabled.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, true);
  if (IsAppLauncherEnabled()) {
    EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());
  } else {
    EXPECT_TRUE(test_helper_->apps_page_shortcut()->GetVisible());
  }

  // Make sure we can also properly transition from true to false.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());
}

// Verify that in instant extended mode the visibility of the reading list
// button properly follows the pref value.
TEST_F(BookmarkBarViewTest, ReadingListVisibility) {
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowReadingListInBookmarkBar, false);
  EXPECT_FALSE(bookmark_bar_view()->read_later_button()->GetVisible());

  // Try to make the Apps shortcut visible. Its visibility depends on whether
  // the app launcher is enabled.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowReadingListInBookmarkBar, true);
  EXPECT_TRUE(bookmark_bar_view()->read_later_button()->GetVisible());

  // Make sure we can also properly transition from true to false.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowReadingListInBookmarkBar, false);
  EXPECT_FALSE(bookmark_bar_view()->read_later_button()->GetVisible());
}

// Various assertions around visibility of the overflow_button.
TEST_F(BookmarkBarViewTest, OverflowVisibility) {
  EXPECT_FALSE(test_helper_->overflow_button()->GetVisible());

  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_TRUE(test_helper_->overflow_button()->GetVisible());

  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());
  const int width_for_one = bookmark_bar_view()->bounds().width();
  EXPECT_TRUE(test_helper_->overflow_button()->GetVisible());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());
  EXPECT_FALSE(test_helper_->overflow_button()->GetVisible());

  bookmark_bar_view()->SetBounds(0, 0, width_for_one,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_TRUE(test_helper_->overflow_button()->GetVisible());
}

// Verifies buttons get added correctly when BookmarkBarView is created after
// the model and the model has nodes.
TEST_F(BookmarkBarViewTest, ButtonsDynamicallyAddedAfterModelHasNodes) {
  EXPECT_TRUE(BookmarkModelFactory::GetForBrowserContext(profile())->loaded());
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  // Ensure buttons were added in the correct place.
  auto button_iter =
      bookmark_bar_view()->FindChild(test_helper_->managed_bookmarks_button());
  for (size_t i = 0; i < test_helper_->GetBookmarkButtonCount(); ++i) {
    ++button_iter;
    ASSERT_NE(bookmark_bar_view()->children().cend(), button_iter);
    EXPECT_EQ(test_helper_->GetBookmarkButton(i), *button_iter);
  }
}

// Verifies buttons are added as the model and size change.
TEST_F(BookmarkBarViewTest, ButtonsDynamicallyAdded) {
  EXPECT_TRUE(BookmarkModelFactory::GetForBrowserContext(profile())->loaded());
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());
  // Ensure buttons were added in the correct place.
  auto button_iter =
      bookmark_bar_view()->FindChild(test_helper_->managed_bookmarks_button());
  for (size_t i = 0; i < test_helper_->GetBookmarkButtonCount(); ++i) {
    ++button_iter;
    ASSERT_NE(bookmark_bar_view()->children().cend(), button_iter);
    EXPECT_EQ(test_helper_->GetBookmarkButton(i), *button_iter);
  }
}

TEST_F(BookmarkBarViewTest, AddNodesWhenBarAlreadySized) {
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  bookmark_bar_view()->Layout();
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());
}

// Various assertions for removing nodes.
TEST_F(BookmarkBarViewTest, RemoveNode) {
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  const BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Remove the 2nd node, should still only have 1 visible.
  model->Remove(bookmark_bar_node->children()[1].get());
  EXPECT_EQ("a", GetStringForVisibleButtons());

  // Remove the first node, should force a new button (for the 'c' node).
  model->Remove(bookmark_bar_node->children()[0].get());
  ASSERT_EQ("c", GetStringForVisibleButtons());
}

// Assertions for moving a node on the bookmark bar.
TEST_F(BookmarkBarViewTest, MoveNode) {
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  const BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Move 'c' first resulting in 'c a b d e f'.
  model->Move(bookmark_bar_node->children()[2].get(), bookmark_bar_node, 0);
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Make enough room for 1 node.
  SizeUntilButtonsVisible(1);
  EXPECT_EQ("c", GetStringForVisibleButtons());

  // Move 'f' first, resulting in 'f c a b d e'.
  model->Move(bookmark_bar_node->children()[5].get(), bookmark_bar_node, 0);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("f c", GetStringForVisibleButtons());

  // Move 'f' to the end, resulting in 'c a b d e f'.
  model->Move(bookmark_bar_node->children()[0].get(), bookmark_bar_node, 6);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("c a", GetStringForVisibleButtons());

  // Move 'c' after 'a', resulting in 'a c b d e f'.
  model->Move(bookmark_bar_node->children()[0].get(), bookmark_bar_node, 2);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("a c", GetStringForVisibleButtons());
}

// Assertions for changing the title of a node.
TEST_F(BookmarkBarViewTest, ChangeTitle) {
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  const BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  model->SetTitle(bookmark_bar_node->children()[0].get(), u"a1");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Make enough room for 1 node.
  SizeUntilButtonsVisible(1);
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model->SetTitle(bookmark_bar_node->children()[1].get(), u"b1");
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model->SetTitle(bookmark_bar_node->children()[5].get(), u"f1");
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model->SetTitle(bookmark_bar_node->children()[3].get(), u"d1");

  // Make the second button visible, changes the title of the first to something
  // really long and make sure the second button hides.
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("a1 b1", GetStringForVisibleButtons());
  model->SetTitle(bookmark_bar_node->children()[0].get(),
                  u"a_really_long_title");
  EXPECT_LE(1u, test_helper_->GetBookmarkButtonCount());

  // Change the title back and make sure the 2nd button is visible again. Don't
  // use GetStringForVisibleButtons() here as more buttons may have been
  // created.
  model->SetTitle(bookmark_bar_node->children()[0].get(), u"a1");
  ASSERT_LE(2u, test_helper_->GetBookmarkButtonCount());
  EXPECT_TRUE(test_helper_->GetBookmarkButton(0)->GetVisible());
  EXPECT_TRUE(test_helper_->GetBookmarkButton(1)->GetVisible());

  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_EQ("a1 b1 c d1 e f1", GetStringForVisibleButtons());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Verifies that the apps shortcut is shown or hidden following the policy
// value. This policy (and the apps shortcut) isn't present on ChromeOS.
TEST_F(BookmarkBarViewTest, ManagedShowAppsShortcutInBookmarksBar) {
  // By default, the pref is not managed and the apps shortcut is shown.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();
  EXPECT_FALSE(prefs->IsManagedPreference(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
  EXPECT_TRUE(test_helper_->apps_page_shortcut()->GetVisible());

  // Hide the apps shortcut by policy, via the managed pref.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(false));
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());

  // And try showing it via policy too.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(true));
  EXPECT_TRUE(test_helper_->apps_page_shortcut()->GetVisible());
}
#endif

TEST_F(BookmarkBarViewInWidgetTest, UpdateTooltipText) {
  widget()->Show();

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::AddNodesFromModelString(model, model->bookmark_bar_node(),
                                           "a b");
  SizeUntilButtonsVisible(1);
  ASSERT_EQ(1u, test_helper_->GetBookmarkButtonCount());

  views::LabelButton* button = test_helper_->GetBookmarkButton(0);
  ASSERT_TRUE(button);
  gfx::Point p;
  EXPECT_EQ(u"a\na.com", button->GetTooltipText(p));
  button->SetText(u"new title");
  EXPECT_EQ(u"new title\na.com", button->GetTooltipText(p));
}

}  // namespace
