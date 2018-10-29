// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

class BookmarkBarViewTest : public BrowserWithTestWindowTest {
 public:
  BookmarkBarViewTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
  }

  void TearDown() override {
    test_helper_.reset();
    bookmark_bar_view_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Returns a string containing the label of each of the *visible* buttons on
  // the bookmark bar. Each label is separated by a space.
  std::string GetStringForVisibleButtons() {
    std::string result;
    for (int i = 0; i < test_helper_->GetBookmarkButtonCount() &&
                        test_helper_->GetBookmarkButton(i)->visible();
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
  void SizeUntilButtonsVisible(int count) {
    const int start_width = bookmark_bar_view_->width();
    const int height = bookmark_bar_view_->GetPreferredSize().height();
    for (int i = 0;
         i < 100 && (test_helper_->GetBookmarkButtonCount() < count ||
                     !test_helper_->GetBookmarkButton(count - 1)->visible());
         ++i) {
      bookmark_bar_view_->SetBounds(0, 0, start_width + i * 10, height);
      bookmark_bar_view_->Layout();
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
  // Creates the BookmarkBarView and BookmarkBarViewTestHelper. Generally you'll
  // want to use CreateBookmarkModelAndBookmarkBarView(), but use this if
  // need to create the BookmarkBarView after the model has populated.
  void CreateBookmarkBarView() {
    bookmark_bar_view_.reset(new BookmarkBarView(browser(), nullptr));
    bookmark_bar_view_->set_owned_by_client();
    test_helper_.reset(new BookmarkBarViewTestHelper(bookmark_bar_view_.get()));
  }

  // Creates the model, blocking until it loads, then creates the
  // BookmarkBarView.
  void CreateBookmarkModelAndBookmarkBarView() {
    profile()->CreateBookmarkModel(true);
    WaitForBookmarkModelToLoad();
    CreateBookmarkBarView();
  }

  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
    // TemplateURLService is normally NULL during testing. Instant extended
    // needs this service so set a custom factory function.
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindRepeating(&BookmarkBarViewTest::CreateTemplateURLService));
    return profile;
  }

  std::unique_ptr<BookmarkBarViewTestHelper> test_helper_;
  std::unique_ptr<BookmarkBarView> bookmark_bar_view_;

 private:
  static std::unique_ptr<KeyedService> CreateTemplateURLService(
      content::BrowserContext* profile) {
    return base::WrapUnique(
        new TemplateURLService(static_cast<Profile*>(profile)->GetPrefs(),
                               base::WrapUnique(new SearchTermsData), NULL,
                               std::unique_ptr<TemplateURLServiceClient>(),
                               NULL, NULL, base::Closure()));
  }

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewTest);
};

// Verify that in instant extended mode the visibility of the apps shortcut
// button properly follows the pref value.
TEST_F(BookmarkBarViewTest, AppsShortcutVisibility) {
  CreateBookmarkModelAndBookmarkBarView();
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->visible());

  // Try to make the Apps shortcut visible. Its visibility depends on whether
  // the app launcher is enabled.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, true);
  if (IsAppLauncherEnabled()) {
    EXPECT_FALSE(test_helper_->apps_page_shortcut()->visible());
  } else {
    EXPECT_TRUE(test_helper_->apps_page_shortcut()->visible());
  }

  // Make sure we can also properly transition from true to false.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->visible());
}

// Various assertions around visibility of the overflow_button.
TEST_F(BookmarkBarViewTest, OverflowVisibility) {
  profile()->CreateBookmarkModel(true);
  CreateBookmarkBarView();
  EXPECT_FALSE(test_helper_->overflow_button()->visible());

  WaitForBookmarkModelToLoad();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_TRUE(test_helper_->overflow_button()->visible());

  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2, test_helper_->GetBookmarkButtonCount());
  const int width_for_one = bookmark_bar_view_->bounds().width();
  EXPECT_TRUE(test_helper_->overflow_button()->visible());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view_->SetBounds(
      0, 0, 5000, bookmark_bar_view_->bounds().height());
  bookmark_bar_view_->Layout();
  EXPECT_EQ(6, test_helper_->GetBookmarkButtonCount());
  EXPECT_FALSE(test_helper_->overflow_button()->visible());

  bookmark_bar_view_->SetBounds(
      0, 0, width_for_one, bookmark_bar_view_->bounds().height());
  bookmark_bar_view_->Layout();
  EXPECT_TRUE(test_helper_->overflow_button()->visible());
}

// Verifies buttons get added correctly when BookmarkBarView is created after
// the model and the model has nodes.
TEST_F(BookmarkBarViewTest, ButtonsDynamicallyAddedAfterModelHasNodes) {
  profile()->CreateBookmarkModel(true);
  WaitForBookmarkModelToLoad();
  EXPECT_TRUE(BookmarkModelFactory::GetForBrowserContext(profile())->loaded());
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  CreateBookmarkBarView();
  EXPECT_EQ(0, test_helper_->GetBookmarkButtonCount());

  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2, test_helper_->GetBookmarkButtonCount());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view_->SetBounds(
      0, 0, 5000, bookmark_bar_view_->bounds().height());
  bookmark_bar_view_->Layout();
  EXPECT_EQ(6, test_helper_->GetBookmarkButtonCount());

  // Ensure buttons were added in the correct place.
  int managed_button_index =
      bookmark_bar_view_->GetIndexOf(test_helper_->managed_bookmarks_button());
  for (int i = 0; i < test_helper_->GetBookmarkButtonCount(); ++i) {
    views::View* button = test_helper_->GetBookmarkButton(i);
    EXPECT_EQ(bookmark_bar_view_->GetIndexOf(button),
              managed_button_index + 1 + i);
  }
}

// Verifies buttons are added as the model and size change.
TEST_F(BookmarkBarViewTest, ButtonsDynamicallyAdded) {
  CreateBookmarkModelAndBookmarkBarView();
  EXPECT_TRUE(BookmarkModelFactory::GetForBrowserContext(profile())->loaded());
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2, test_helper_->GetBookmarkButtonCount());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view_->SetBounds(
      0, 0, 5000, bookmark_bar_view_->bounds().height());
  bookmark_bar_view_->Layout();
  EXPECT_EQ(6, test_helper_->GetBookmarkButtonCount());
  // Ensure buttons were added in the correct place.
  int managed_button_index =
      bookmark_bar_view_->GetIndexOf(test_helper_->managed_bookmarks_button());
  for (int i = 0; i < test_helper_->GetBookmarkButtonCount(); ++i) {
    views::View* button = test_helper_->GetBookmarkButton(i);
    EXPECT_EQ(bookmark_bar_view_->GetIndexOf(button),
              managed_button_index + 1 + i);
  }
}

TEST_F(BookmarkBarViewTest, AddNodesWhenBarAlreadySized) {
  CreateBookmarkModelAndBookmarkBarView();
  bookmark_bar_view_->SetBounds(
      0, 0, 5000, bookmark_bar_view_->bounds().height());
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  bookmark_bar_view_->Layout();
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());
}

// Various assertions for removing nodes.
TEST_F(BookmarkBarViewTest, RemoveNode) {
  CreateBookmarkModelAndBookmarkBarView();
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  const BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2, test_helper_->GetBookmarkButtonCount());

  // Remove the 2nd node, should still only have 1 visible.
  model->Remove(bookmark_bar_node->GetChild(1));
  EXPECT_EQ("a", GetStringForVisibleButtons());

  // Remove the first node, should force a new button (for the 'c' node).
  model->Remove(bookmark_bar_node->GetChild(0));
  ASSERT_EQ("c", GetStringForVisibleButtons());
}

// Assertions for moving a node on the bookmark bar.
TEST_F(BookmarkBarViewTest, MoveNode) {
  CreateBookmarkModelAndBookmarkBarView();
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  const BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0, test_helper_->GetBookmarkButtonCount());

  // Move 'c' first resulting in 'c a b d e f'.
  model->Move(bookmark_bar_node->GetChild(2), bookmark_bar_node, 0);
  EXPECT_EQ(0, test_helper_->GetBookmarkButtonCount());

  // Make enough room for 1 node.
  SizeUntilButtonsVisible(1);
  EXPECT_EQ("c", GetStringForVisibleButtons());

  // Move 'f' first, resulting in 'f c a b d e'.
  model->Move(bookmark_bar_node->GetChild(5), bookmark_bar_node, 0);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("f c", GetStringForVisibleButtons());

  // Move 'f' to the end, resulting in 'c a b d e f'.
  model->Move(bookmark_bar_node->GetChild(0), bookmark_bar_node, 6);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("c a", GetStringForVisibleButtons());

  // Move 'c' after 'a', resulting in 'a c b d e f'.
  model->Move(bookmark_bar_node->GetChild(0), bookmark_bar_node, 2);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("a c", GetStringForVisibleButtons());
}

// Assertions for changing the title of a node.
TEST_F(BookmarkBarViewTest, ChangeTitle) {
  CreateBookmarkModelAndBookmarkBarView();
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  const BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0, test_helper_->GetBookmarkButtonCount());

  model->SetTitle(bookmark_bar_node->GetChild(0), base::ASCIIToUTF16("a1"));
  EXPECT_EQ(0, test_helper_->GetBookmarkButtonCount());

  // Make enough room for 1 node.
  SizeUntilButtonsVisible(1);
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model->SetTitle(bookmark_bar_node->GetChild(1), base::ASCIIToUTF16("b1"));
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model->SetTitle(bookmark_bar_node->GetChild(5), base::ASCIIToUTF16("f1"));
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model->SetTitle(bookmark_bar_node->GetChild(3), base::ASCIIToUTF16("d1"));

  // Make the second button visible, changes the title of the first to something
  // really long and make sure the second button hides.
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("a1 b1", GetStringForVisibleButtons());
  model->SetTitle(bookmark_bar_node->GetChild(0),
                  base::ASCIIToUTF16("a_really_long_title"));
  EXPECT_LE(1, test_helper_->GetBookmarkButtonCount());

  // Change the title back and make sure the 2nd button is visible again. Don't
  // use GetStringForVisibleButtons() here as more buttons may have been
  // created.
  model->SetTitle(bookmark_bar_node->GetChild(0), base::ASCIIToUTF16("a1"));
  ASSERT_LE(2, test_helper_->GetBookmarkButtonCount());
  EXPECT_TRUE(test_helper_->GetBookmarkButton(0)->visible());
  EXPECT_TRUE(test_helper_->GetBookmarkButton(1)->visible());

  bookmark_bar_view_->SetBounds(
      0, 0, 5000, bookmark_bar_view_->bounds().height());
  bookmark_bar_view_->Layout();
  EXPECT_EQ("a1 b1 c d1 e f1", GetStringForVisibleButtons());
}

#if !defined(OS_CHROMEOS)
// Verifies that the apps shortcut is shown or hidden following the policy
// value. This policy (and the apps shortcut) isn't present on ChromeOS.
TEST_F(BookmarkBarViewTest, ManagedShowAppsShortcutInBookmarksBar) {
  CreateBookmarkModelAndBookmarkBarView();
  // By default, the pref is not managed and the apps shortcut is shown.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();
  EXPECT_FALSE(prefs->IsManagedPreference(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
  EXPECT_TRUE(test_helper_->apps_page_shortcut()->visible());

  // Hide the apps shortcut by policy, via the managed pref.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(false));
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->visible());

  // And try showing it via policy too.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(true));
  EXPECT_TRUE(test_helper_->apps_page_shortcut()->visible());
}
#endif

TEST_F(BookmarkBarViewTest, UpdateTooltipText) {
  CreateBookmarkModelAndBookmarkBarView();
  // Create a widget who creates and owns a views::ToolipManager.
  views::Widget widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.native_widget = views::test::CreatePlatformDesktopNativeWidgetImpl(
      params, &widget, nullptr);
  widget.Init(params);
  widget.Show();
  widget.GetRootView()->AddChildView(bookmark_bar_view_.get());

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::AddNodesFromModelString(model, model->bookmark_bar_node(),
                                           "a b");
  SizeUntilButtonsVisible(1);
  ASSERT_EQ(1, test_helper_->GetBookmarkButtonCount());

  views::LabelButton* button = test_helper_->GetBookmarkButton(0);
  ASSERT_TRUE(button);
  gfx::Point p;
  base::string16 text;
  button->GetTooltipText(p, &text);
  EXPECT_EQ(base::ASCIIToUTF16("a\na.com"), text);
  button->SetText(base::ASCIIToUTF16("new title"));
  button->GetTooltipText(p, &text);
  EXPECT_EQ(base::ASCIIToUTF16("new title\na.com"), text);

  widget.CloseNow();
}

}  // namespace
