// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_bar_utils.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_injector.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_menu_delegate.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/page_navigator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

class BookmarkBarViewBaseTest : public ChromeViewsTestBase {
 public:
  BookmarkBarViewBaseTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        TemplateURLServiceTestUtil::GetTemplateURLServiceTestingFactory());
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        BookmarkMergedSurfaceServiceFactory::GetInstance(),
        BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());
    // The real service calls SavedTabGroupBar::OnInitialized() from a posted
    // task, which needs capabilities() that the mock browser doesn't provide.
    profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  testing::NiceMock<tab_groups::MockTabGroupSyncService>>();
            }));
    profile_ = profile_builder.Build();

    BookmarkMergedSurfaceServiceFactory::GetForProfile(profile_.get())
        ->LoadForTesting({});

    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
  }

  void TearDown() override {
    ChromeViewsTestBase::TearDown();
  }

  virtual BookmarkBarView* bookmark_bar_view() = 0;

  TestingProfile* profile() { return profile_.get(); }
  BrowserWindowInterface* browser() { return &browser_window_interface_; }

 protected:
  // Returns a string containing the label of each of the *visible* buttons on
  // the bookmark bar. Each label is separated by a space.
  std::string GetStringForVisibleButtons() {
    std::string result;
    for (size_t i = 0; i < test_helper_->GetBookmarkButtonCount() &&
                       test_helper_->GetBookmarkButton(i)->GetVisible();
         ++i) {
      if (i != 0) {
        result += " ";
      }
      result +=
          base::UTF16ToASCII(test_helper_->GetBookmarkButton(i)->GetText());
    }
    return result;
  }

  // Continues enlarging the bookmark bar until it has at least `count`
  // buttons that are visible.
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
      views::test::RunScheduledLayout(bookmark_bar_view());
    }
  }

  // Continues shrinking the bookmark bar until it has at most `count` buttons
  // that are visible.
  void SizeDownUntilButtonsVisible(size_t count) {
    const int start_width = bookmark_bar_view()->width();
    const int height = bookmark_bar_view()->GetPreferredSize().height();
    // Keep shrinking the bar view's bounds until either:
    // - There are fewer bookmark buttons than `count`.
    // - The button at index `count` is hidden.
    // - Up to a maximum of 100 times.
    for (size_t i = 0;
         i < 100 && (test_helper_->GetBookmarkButtonCount() >= count &&
                     test_helper_->GetBookmarkButton(count)->GetVisible());
         ++i) {
      bookmark_bar_view()->SetBounds(0, 0, start_width - i * 10, height);
      views::test::RunScheduledLayout(bookmark_bar_view());
    }
  }

  BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile());
  }

  BookmarkMergedSurfaceService* service() {
    return BookmarkMergedSurfaceServiceFactory::GetForProfile(profile());
  }

  void WaitForBookmarkModelToLoad() {
    bookmarks::test::WaitForBookmarkModelToLoad(model());
  }

  // Adds nodes to the bookmark bar node from |string|. See
  // bookmarks::test::AddNodesFromModelString() for details on |string|.
  void AddNodesToBookmarkBarFromModelString(const std::string& string) {
    bookmarks::test::AddNodesFromModelString(
        model(), model()->bookmark_bar_node(), string);
    views::test::RunScheduledLayout(bookmark_bar_view());
  }

  void AddNodesToAccountBookmarkBarFromModelString(const std::string& string) {
    CHECK(model()->account_bookmark_bar_node());
    bookmarks::test::AddNodesFromModelString(
        model(), model()->account_bookmark_bar_node(), string);
    views::test::RunScheduledLayout(bookmark_bar_view());
  }

  // Creates the model, blocking until it loads, then creates the
  // BookmarkBarView.
  std::unique_ptr<BookmarkBarView> CreateBookmarkModelAndBookmarkBarView() {
    WaitForBookmarkModelToLoad();

    auto bookmark_bar_view = std::make_unique<BookmarkBarView>(
        browser(),
        DesktopBookmarkBarInjector::CreateBookmarkBarUIController(browser()),
        nullptr);
    test_helper_ =
        std::make_unique<BookmarkBarViewTestHelper>(bookmark_bar_view.get());
    return bookmark_bar_view;
  }

  base::test::ScopedFeatureList feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<BookmarkBarViewTestHelper> test_helper_;
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
    bookmark_bar_view_.reset();
    BookmarkBarViewBaseTest::TearDown();
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

    widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    bookmark_bar_view_ =
        widget_->SetContentsView(CreateBookmarkModelAndBookmarkBarView());
  }

  void TearDown() override {
    bookmark_bar_view_ = nullptr;
    widget_.reset();

    BookmarkBarViewBaseTest::TearDown();
  }

  BookmarkBarView* bookmark_bar_view() override { return bookmark_bar_view_; }

  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<BookmarkBarView> bookmark_bar_view_ = nullptr;
};

// Verify that in instant extended mode the visibility of the apps shortcut
// button properly follows the pref value.
TEST_F(BookmarkBarViewTest, AppsShortcutVisibility) {
  profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());

  // Try to make the Apps shortcut visible. Its visibility depends on whether
  // the Apps shortcut is enabled.
  profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, true);
  if (chrome::IsAppsShortcutEnabled(profile())) {
    EXPECT_TRUE(test_helper_->apps_page_shortcut()->GetVisible());
  } else {
    EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());
  }

  // Make sure we can also properly transition from true to false.
  profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());
}

TEST_F(BookmarkBarViewTest, TabGroupsBarVisibility) {
  // Pref to show by default. Tab group bar is visible by default.
  EXPECT_TRUE(test_helper_->saved_tab_group_bar()->GetVisible());

  // Pref not to show hides tab group bar.
  profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->saved_tab_group_bar()->GetVisible());

  // Pref to show displays tab group bar.
  profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar, true);
  EXPECT_TRUE(test_helper_->saved_tab_group_bar()->GetVisible());
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
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());
  EXPECT_FALSE(test_helper_->overflow_button()->GetVisible());

  bookmark_bar_view()->SetBounds(0, 0, width_for_one,
                                 bookmark_bar_view()->bounds().height());
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_TRUE(test_helper_->overflow_button()->GetVisible());
}

// Verifies buttons get added correctly when BookmarkBarView is created after
// the model and the model has nodes.
// TODO(crbug.com/375364962): Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ButtonsDynamicallyAddedAfterModelHasNodes \
  DISABLED_ButtonsDynamicallyAddedAfterModelHasNodes
#else
#define MAYBE_ButtonsDynamicallyAddedAfterModelHasNodes \
  ButtonsDynamicallyAddedAfterModelHasNodes
#endif
TEST_F(BookmarkBarViewTest, MAYBE_ButtonsDynamicallyAddedAfterModelHasNodes) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  // Ensure buttons were added in the correct place.
  auto button_iter = bookmark_bar_view()->FindChild(
      test_helper_->saved_tab_groups_separator_view_());
  for (size_t i = 0; i < test_helper_->GetBookmarkButtonCount(); ++i) {
    ++button_iter;
    ASSERT_NE(bookmark_bar_view()->children().cend(), button_iter);
    EXPECT_EQ(test_helper_->GetBookmarkButton(i), *button_iter);
  }
}

// Verifies buttons are added as the model and size change.
TEST_F(BookmarkBarViewTest, ButtonsDynamicallyAdded) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());
  // Ensure buttons were added in the correct place.
  auto button_iter = bookmark_bar_view()->FindChild(
      test_helper_->saved_tab_groups_separator_view_());
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
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());
}

// Various assertions for removing nodes.
TEST_F(BookmarkBarViewTest, RemoveNode) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Remove the 2nd node, should still only have 1 visible.
  model()->Remove(bookmark_bar_node->children()[1].get(),
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ("a", GetStringForVisibleButtons());

  // Remove the first node, should force a new button (for the 'c' node).
  model()->Remove(bookmark_bar_node->children()[0].get(),
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  views::test::RunScheduledLayout(bookmark_bar_view());
  ASSERT_EQ("c", GetStringForVisibleButtons());

  model()->CreateAccountPermanentFolders();
  AddNodesToAccountBookmarkBarFromModelString("1 2 3 ");

  model()->Remove(model()->account_bookmark_bar_node()->children()[1].get(),
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ("1", GetStringForVisibleButtons());

  // Remove first node, should force a new button (for the '3' node).
  model()->Remove(model()->account_bookmark_bar_node()->children()[0].get(),
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  views::test::RunScheduledLayout(bookmark_bar_view());
  ASSERT_EQ("3", GetStringForVisibleButtons());
}

TEST_F(BookmarkBarViewTest, RemoveAccountNodes) {
  model()->CreateAccountPermanentFolders();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  AddNodesToAccountBookmarkBarFromModelString("A1 A2 A3 A4 ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(10);
  EXPECT_EQ(10u, test_helper_->GetBookmarkButtonCount());
  EXPECT_EQ("A1 A2 A3 A4 a b c d e f", GetStringForVisibleButtons());

  // Remove the account nodes, local nodes should still be visible.
  model()->RemoveAccountPermanentFolders();
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());
}

TEST_F(BookmarkBarViewTest, RemoveAccountNodesCustomOrder) {
  model()->CreateAccountPermanentFolders();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  AddNodesToAccountBookmarkBarFromModelString("A1 A2 A3 A4 ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(10);
  EXPECT_EQ("A1 A2 A3 A4 a b c d e f", GetStringForVisibleButtons());

  service()->Move(model()->account_bookmark_bar_node()->children()[1].get(),
                  BookmarkParentFolder::BookmarkBarFolder(), 10u,
                  /*browser=*/nullptr);
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ("A1 A3 A4 a b c d e f A2", GetStringForVisibleButtons());

  // Remove the account nodes, local nodes should still be visible.
  model()->RemoveAccountPermanentFolders();
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());
}

TEST_F(BookmarkBarViewTest, RemoveAccountNodesNotAllAccountNodesVisible) {
  model()->CreateAccountPermanentFolders();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  AddNodesToAccountBookmarkBarFromModelString("1 2 3 4 ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("1 2", GetStringForVisibleButtons());

  // Remove the account nodes, local nodes should still be visible.
  model()->RemoveAccountPermanentFolders();
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ("a b", GetStringForVisibleButtons());
}

// Tests that moving a `BookmarkNode` on the bookmark bar updates button
// ordering and visibility.
TEST_F(BookmarkBarViewTest, MoveNode) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Move 'c' first resulting in 'c a b d e f'.
  model()->Move(bookmark_bar_node->children()[2].get(), bookmark_bar_node, 0);
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Make enough room for 1 node.
  SizeUntilButtonsVisible(1);
  EXPECT_EQ("c", GetStringForVisibleButtons());

  // Move 'f' first, resulting in 'f c a b d e'.
  model()->Move(bookmark_bar_node->children()[5].get(), bookmark_bar_node, 0);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("f c", GetStringForVisibleButtons());

  // Move 'f' to the end, resulting in 'c a b d e f'.
  model()->Move(bookmark_bar_node->children()[0].get(), bookmark_bar_node, 6);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("c a", GetStringForVisibleButtons());

  // Move 'c' after 'a', resulting in 'a c b d e f'.
  model()->Move(bookmark_bar_node->children()[0].get(), bookmark_bar_node, 2);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("a c", GetStringForVisibleButtons());
}

// Ensures that the overflow button's menu responds as bookmark button
// visibility changes.
TEST_F(BookmarkBarViewInWidgetTest, ButtonVisiblityUpdatesOverflowMenu) {
  widget()->Show();
  AddNodesToBookmarkBarFromModelString("a b c d ");
  ASSERT_EQ(4u, test_helper_->GetBookmarkButtonCount());
  SizeDownUntilButtonsVisible(1);

  views::MenuButton* overflow_button = bookmark_bar_view()->overflow_button();
  ASSERT_TRUE(overflow_button);
  overflow_button->Activate(nullptr);
  views::MenuItemView* overflow_menu = bookmark_bar_view()->GetMenu();
  ASSERT_TRUE(overflow_menu && overflow_menu->HasSubmenu());
  EXPECT_EQ(3u, overflow_menu->GetSubmenu()->GetMenuItems().size());

  SizeUntilButtonsVisible(2);
  EXPECT_EQ(2u, overflow_menu->GetSubmenu()->GetMenuItems().size());

  SizeUntilButtonsVisible(3);
  EXPECT_EQ(1u, overflow_menu->GetSubmenu()->GetMenuItems().size());

  SizeDownUntilButtonsVisible(1);
  EXPECT_EQ(3u, overflow_menu->GetSubmenu()->GetMenuItems().size());
}

TEST_F(BookmarkBarViewInWidgetTest, FolderButtonShowsMenu) {
  widget()->Show();
  AddNodesToBookmarkBarFromModelString("F1:[ a b ] ");
  ASSERT_EQ(1u, test_helper_->GetBookmarkButtonCount());
  auto* folder_button =
      views::AsViewClass<views::MenuButton>(test_helper_->GetBookmarkButton(0));
  ASSERT_TRUE(folder_button);

  folder_button->Activate(nullptr);
  views::MenuItemView* menu = bookmark_bar_view()->GetMenu();
  ASSERT_TRUE(menu && menu->HasSubmenu());
  EXPECT_EQ(2u, menu->GetSubmenu()->GetMenuItems().size());

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kCollapse;
  EXPECT_TRUE(folder_button->HandleAccessibleAction(action_data));
  EXPECT_FALSE(bookmark_bar_view()->GetMenu());

  action_data.action = ax::mojom::Action::kExpand;
  EXPECT_TRUE(folder_button->HandleAccessibleAction(action_data));
  EXPECT_TRUE(bookmark_bar_view()->GetMenu());
}

TEST_F(BookmarkBarViewInWidgetTest, AppsShortcutForwardsEventFlags) {
  widget()->Show();
  EXPECT_CALL(browser_window_interface_,
              OpenURL(testing::Field(
                          &content::OpenURLParams::disposition,
                          ui::mojom::WindowOpenDisposition::kNewBackgroundTab),
                      testing::_));

  views::test::ButtonTestApi(test_helper_->apps_page_shortcut())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_MIDDLE_MOUSE_BUTTON,
                                  ui::EF_MIDDLE_MOUSE_BUTTON));
}

// Tests that changing a `BookmarkNode` title updates button text and visibility
// as available space and label widths change.
TEST_F(BookmarkBarViewTest, ChangeTitle) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  model()->SetTitle(bookmark_bar_node->children()[0].get(), u"a1",
                    bookmarks::metrics::BookmarkEditSource::kUser);
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Make enough room for 1 node.
  SizeUntilButtonsVisible(1);
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model()->SetTitle(bookmark_bar_node->children()[1].get(), u"b1",
                    bookmarks::metrics::BookmarkEditSource::kUser);
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model()->SetTitle(bookmark_bar_node->children()[5].get(), u"f1",
                    bookmarks::metrics::BookmarkEditSource::kUser);
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model()->SetTitle(bookmark_bar_node->children()[3].get(), u"d1",
                    bookmarks::metrics::BookmarkEditSource::kUser);

  // Make the second button visible, changes the title of the first to something
  // really long and make sure the second button hides.
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("a1 b1", GetStringForVisibleButtons());
  model()->SetTitle(bookmark_bar_node->children()[0].get(),
                    u"a_really_long_title",
                    bookmarks::metrics::BookmarkEditSource::kUser);
  EXPECT_LE(1u, test_helper_->GetBookmarkButtonCount());

  // Change the title back and make sure the 2nd button is visible again. Don't
  // use GetStringForVisibleButtons() here as more buttons may have been
  // created.
  model()->SetTitle(bookmark_bar_node->children()[0].get(), u"a1",
                    bookmarks::metrics::BookmarkEditSource::kUser);
  ASSERT_LE(2u, test_helper_->GetBookmarkButtonCount());
  EXPECT_TRUE(test_helper_->GetBookmarkButton(0)->GetVisible());
  EXPECT_TRUE(test_helper_->GetBookmarkButton(1)->GetVisible());

  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  views::test::RunScheduledLayout(bookmark_bar_view());
  EXPECT_EQ("a1 b1 c d1 e f1", GetStringForVisibleButtons());
}

// Tests that `BookmarkBarView` supports expected drop formats, including URLs
// and bookmark entries.
TEST_F(BookmarkBarViewTest, GetDropFormats) {
  int formats = 0;
  std::set<ui::ClipboardFormatType> format_types;
  EXPECT_TRUE(bookmark_bar_view()->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::URL, formats);
  EXPECT_NE(format_types.find(ui::ClipboardFormatType::BookmarkEntriesType()),
            format_types.end());
}

// Tests that dropping a URL onto `BookmarkBarView` invokes the drop callback
// and adds the bookmark button.
TEST_F(BookmarkBarViewTest, DropCallbackTest) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  SizeUntilButtonsVisible(7);
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  gfx::Point bar_loc;
  views::View::ConvertPointToScreen(bookmark_bar_view(), &bar_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(bar_loc),
                                   gfx::PointF(bar_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  EXPECT_TRUE(bookmark_bar_view()->CanDrop(drop_data));
  bookmark_bar_view()->OnDragUpdated(target_event);
  auto cb = bookmark_bar_view()->GetDropCallback(target_event);
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());

  ui::mojom::DragOperation output_drag_op;
  std::move(cb).Run(target_event, output_drag_op,
                    /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ("z a b c d e f", GetStringForVisibleButtons());
  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
}

// Tests that mutating the `BookmarkModel` during an active drag updates drop
// state correctly.
TEST_F(BookmarkBarViewTest, MutateModelDuringDrag) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  SizeUntilButtonsVisible(7);
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  gfx::Point drop_loc;
  views::View::ConvertPointToScreen(test_helper_->GetBookmarkButton(5),
                                    &drop_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(drop_loc),
                                   gfx::PointF(drop_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  ASSERT_TRUE(bookmark_bar_view()->CanDrop(drop_data));
  bookmark_bar_view()->OnDragUpdated(target_event);
  EXPECT_NE(-1, test_helper_->GetDropLocationModelIndexForTesting());
  model()->Remove(model()->bookmark_bar_node()->children()[4].get(),
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  EXPECT_EQ(-1, test_helper_->GetDropLocationModelIndexForTesting());
}

TEST_F(BookmarkBarViewTest, DropCallback_InvalidatePtrTest) {
  SizeUntilButtonsVisible(7);
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  gfx::Point bar_loc;
  views::View::ConvertPointToScreen(bookmark_bar_view(), &bar_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(bar_loc),
                                   gfx::PointF(bar_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  EXPECT_TRUE(bookmark_bar_view()->CanDrop(drop_data));
  bookmark_bar_view()->OnDragUpdated(target_event);
  auto cb = bookmark_bar_view()->GetDropCallback(target_event);

  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(target_event, output_drag_op,
                    /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());
  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kNone);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Verifies that the apps shortcut is shown or hidden following the policy
// value. This policy (and the apps shortcut) isn't present on ChromeOS.
TEST_F(BookmarkBarViewTest, ManagedShowAppsShortcutInBookmarksBar) {
  // By default, the pref is not managed and the apps shortcut is not shown.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();
  EXPECT_FALSE(prefs->IsManagedPreference(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());

  // Shows the apps shortcut by policy, via the managed pref.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(true));
  EXPECT_TRUE(test_helper_->apps_page_shortcut()->GetVisible());

  // And try hiding it via policy too.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(false));
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());
}
#endif


TEST_F(BookmarkBarViewTest, GetAvailableWidthForSavedTabGroupsBar) {
  // Saved tab group bar and bookmark buttons can both fit.
  ASSERT_EQ(
      100, BookmarkBarView::GetAvailableWidthForSavedTabGroupsBar(60, 30, 100));

  // Cases of saved tab group bar and bookmark buttons cannot both fit below.
  // Prioritize fitting saved tab group since it's smaller than half of the
  // available width.
  ASSERT_EQ(
      100, BookmarkBarView::GetAvailableWidthForSavedTabGroupsBar(30, 80, 100));

  // Prioritize fitting bookmark buttons since it's smaller than half of the
  // available width.
  ASSERT_EQ(
      70, BookmarkBarView::GetAvailableWidthForSavedTabGroupsBar(80, 30, 100));

  // Split the space evenly since neither can fit half of the availablel width.
  ASSERT_EQ(
      50, BookmarkBarView::GetAvailableWidthForSavedTabGroupsBar(80, 60, 100));
}

TEST_F(BookmarkBarViewTest, AccessibleProperties) {
  ui::AXNodeData data;

  bookmark_bar_view()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kToolbar);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF8(IDS_ACCNAME_BOOKMARKS));
}

TEST_F(BookmarkBarViewTest, BookmarkFolderButtonAccessibleProperties) {
  auto* folder_button = test_helper_->managed_bookmarks_button();
  ui::AXNodeData data;

  folder_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  // Role in set by menu button controller.
  EXPECT_EQ(data.role, ax::mojom::Role::kPopUpButton);
  EXPECT_EQ(
      data.GetStringAttribute(ax::mojom::StringAttribute::kRoleDescription),
      l10n_util::GetStringUTF8(
          IDS_ACCNAME_BOOKMARK_FOLDER_BUTTON_ROLE_DESCRIPTION));
}

TEST_F(BookmarkBarViewTest, BookmarkFolderButtonTooltipText) {
  auto* folder_button = test_helper_->managed_bookmarks_button();
  folder_button->SetText(u"Managed Bookmarks");

  EXPECT_EQ(u"Managed Bookmarks",
            folder_button->GetRenderedTooltipText(gfx::Point()));

  folder_button->SetText(std::u16string());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_UNNAMED_BOOKMARK_FOLDER),
            folder_button->GetRenderedTooltipText(gfx::Point()));
}

TEST_F(BookmarkBarViewTest, ButtonSeparatorViewAccessibleProperties) {
  auto* seperator_view = test_helper_->saved_tab_groups_separator_view_();
  ui::AXNodeData data;

  seperator_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kSplitter);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF8(IDS_ACCNAME_SEPARATOR));
}

TEST_F(BookmarkBarViewTest, AllBookmarksButtonVisibilityWithExtensiveChanges) {
  // Initially no "All bookmarks" button should be visible
  EXPECT_FALSE(bookmark_bar_view()->all_bookmarks_button()->GetVisible());

  model()->BeginExtensiveChanges();

  // Add bookmarks to the bookmark bar and other bookmarks folder
  model()->AddFolder(model()->bookmark_bar_node(), 0, u"f1");
  model()->AddURL(model()->other_node(), 0, u"other_bookmark",
                  GURL("https://www.example.com"));

  model()->EndExtensiveChanges();

  // After extensive changes end, the "All bookmarks" button should be visible
  // since there are bookmarks in the "Other Bookmarks" folder
  EXPECT_TRUE(bookmark_bar_view()->all_bookmarks_button()->GetVisible());

  // Now test removing all bookmarks from "Other Bookmarks" folder during
  // extensive changes
  model()->BeginExtensiveChanges();

  // Remove all bookmarks from the "Other Bookmarks" folder
  while (model()->other_node()->children().size() > 0) {
    model()->Remove(model()->other_node()->children()[0].get(),
                    bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }

  model()->EndExtensiveChanges();

  // After extensive changes end, the "All bookmarks" button should no longer be
  // visible since there are no bookmarks in the "Other Bookmarks" folder
  EXPECT_FALSE(bookmark_bar_view()->all_bookmarks_button()->GetVisible());
}

TEST_F(BookmarkBarViewInWidgetTest, UpdateTooltipText) {
  widget()->Show();

  bookmarks::test::AddNodesFromModelString(model(),
                                           model()->bookmark_bar_node(), "a b");
  SizeUntilButtonsVisible(1);
  ASSERT_EQ(1u, test_helper_->GetBookmarkButtonCount());

  views::LabelButton* button = test_helper_->GetBookmarkButton(0);
  ASSERT_TRUE(button);
  gfx::Point p;
  EXPECT_EQ(u"a\na.com", button->GetRenderedTooltipText(p));
  button->SetText(u"new title");
  EXPECT_EQ(u"new title\na.com", button->GetRenderedTooltipText(p));
}

// Regression test for https://crbug.com/385805737. When BookmarkButton receives
// an AddedToWidget call, it should also call the corresponding superclass
// method (specifically, `LabelButton::AddedToWidget()` must be called).
TEST_F(BookmarkBarViewInWidgetTest,
       BookmarkButtonAddedToWidgetCallsSuperclass) {
  widget()->ShowInactive();
  widget()->Hide();

  bookmarks::test::AddNodesFromModelString(model(),
                                           model()->bookmark_bar_node(), "a b");
  SizeUntilButtonsVisible(1);

  // `BookmarkButton::AddedToWidget()` will have been called, so ensure that
  // `LabelButton::AddedToWidget()` has been called as well.
  ASSERT_EQ(1u, test_helper_->GetBookmarkButtonCount());
  views::LabelButton* button = test_helper_->GetBookmarkButton(0);
  ASSERT_TRUE(button);
  // The `LabelButton::AddedToWidget()` call only has an effect for bookmark
  // buttons on certain platforms, so gate the check.
  if constexpr (views::PlatformStyle::kInactiveWidgetControlsAppearDisabled) {
    EXPECT_TRUE(button->has_paint_as_active_subscription_for_testing());
  }
}

// TODO(crbug.com/375364962): Flaky on Windows & Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#define MAYBE_AccessibleRoleDescription DISABLED_AccessibleRoleDescription
#else
#define MAYBE_AccessibleRoleDescription AccessibleRoleDescription
#endif
TEST_F(BookmarkBarViewTest, MAYBE_AccessibleRoleDescription) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  SizeUntilButtonsVisible(1);
  views::LabelButton* button = test_helper_->GetBookmarkButton(0);

  ui::AXNodeData data;
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      data.GetStringAttribute(ax::mojom::StringAttribute::kRoleDescription),
      l10n_util::GetStringUTF8(IDS_ACCNAME_BOOKMARK_BUTTON_ROLE_DESCRIPTION));
}

// This mock is used for method call counting. It redirects the call to the real
// implementation.
class BookmarkBarViewWithCounter : public BookmarkBarView {
 public:
  explicit BookmarkBarViewWithCounter(BrowserWindowInterface* browser)
      : BookmarkBarView(
            browser,
            DesktopBookmarkBarInjector::CreateBookmarkBarUIController(browser),
            nullptr) {}

  size_t GetSchedulePaintCount() const { return schedule_paint_count_; }

 protected:
  void OnDidSchedulePaint(const gfx::Rect& r) override {
    BookmarkBarView::OnDidSchedulePaint(r);
    ++schedule_paint_count_;
  }

  size_t schedule_paint_count_ = 0;
};

// Test implementation using `BookmarkBarViewWithCounter`.
class BookmarkBarViewWithCounterTest : public BookmarkBarViewBaseTest {
 public:
  // BookmarkBarViewBaseTest
  void SetUp() override {
    BookmarkBarViewBaseTest::SetUp();

    WaitForBookmarkModelToLoad();
    bookmark_bar_view_with_counter_ =
        std::make_unique<BookmarkBarViewWithCounter>(browser());
  }

  void TearDown() override {
    bookmark_bar_view_with_counter_.reset();
    BookmarkBarViewBaseTest::TearDown();
  }

  BookmarkBarView* bookmark_bar_view() override {
    return bookmark_bar_view_with_counter_.get();
  }
  BookmarkBarViewWithCounter* bookmark_bar_view_with_counter() {
    return static_cast<BookmarkBarViewWithCounter*>(bookmark_bar_view());
  }

 private:
  std::unique_ptr<BookmarkBarViewWithCounter> bookmark_bar_view_with_counter_;
};

TEST_F(BookmarkBarViewWithCounterTest, PaintCountWithIndividualOperations) {
  ASSERT_EQ(bookmark_bar_view_with_counter()->GetSchedulePaintCount(), 0u);

  const bookmarks::BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  model()->AddFolder(bookmark_bar, 0, u"f1");
  ASSERT_EQ(bookmark_bar_view_with_counter()->GetSchedulePaintCount(), 1u);

  model()->AddFolder(bookmark_bar, 0, u"f2");
  ASSERT_EQ(bookmark_bar_view_with_counter()->GetSchedulePaintCount(), 2u);

  const bookmarks::BookmarkNode* f3 =
      model()->AddFolder(bookmark_bar, 0, u"f3");
  ASSERT_EQ(bookmark_bar_view_with_counter()->GetSchedulePaintCount(), 3u);

  const bookmarks::BookmarkNode* ff3 = model()->AddFolder(f3, 0, u"ff3");
  model()->Move(ff3, bookmark_bar, 0);
  ASSERT_EQ(bookmark_bar_view_with_counter()->GetSchedulePaintCount(), 4u);
}

TEST_F(BookmarkBarViewWithCounterTest,
       PaintCountWithExtensiveChangesOperations) {
  ASSERT_EQ(bookmark_bar_view_with_counter()->GetSchedulePaintCount(), 0u);

  model()->BeginExtensiveChanges();
  const bookmarks::BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  model()->AddFolder(bookmark_bar, 0, u"f1");
  model()->AddFolder(bookmark_bar, 0, u"f2");
  const bookmarks::BookmarkNode* f3 =
      model()->AddFolder(bookmark_bar, 0, u"f3");
  const bookmarks::BookmarkNode* ff3 = model()->AddFolder(f3, 0, u"ff3");
  model()->Move(ff3, bookmark_bar, 0);
  EXPECT_EQ(bookmark_bar_view_with_counter()->GetSchedulePaintCount(), 0u);

  model()->EndExtensiveChanges();
  EXPECT_EQ(bookmark_bar_view_with_counter()->GetSchedulePaintCount(), 1u);
}

class BookmarkBarViewFaviconTest : public BookmarkBarViewTest {
 public:
  void SetUp() override {
    BookmarkBarViewTest::SetUp();
    favicon_service_ = static_cast<favicon::MockFaviconService*>(
        FaviconServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      testing::NiceMock<favicon::MockFaviconService>>();
                })));
    ON_CALL(*favicon_service_,
            GetFaviconImageForPageURL(testing::_, testing::_, testing::_))
        .WillByDefault([this](const GURL& page_url,
                              favicon_base::FaviconImageCallback callback,
                              base::CancelableTaskTracker* tracker) {
          favicon_base::FaviconImageResult result;
          result.image = current_favicon_;
          result.icon_url = page_url.Resolve("/favicon.ico");
          return tracker->PostTask(
              base::SingleThreadTaskRunner::GetCurrentDefault().get(),
              FROM_HERE,
              base::BindOnce(std::move(callback), std::move(result)));
        });
  }

  void TearDown() override {
    menu_item_ = nullptr;
    bar_bookmark_ = nullptr;
    other_bookmark_ = nullptr;
    menu_delegate_.reset();
    root_menu_.reset();
    favicon_service_ = nullptr;
    BookmarkBarViewTest::TearDown();
  }

  void InitBookmarksAndMenu() {
    bar_bookmark_ =
        model()->AddURL(model()->bookmark_bar_node(), 0, u"example", url_);
    other_bookmark_ =
        model()->AddURL(model()->other_node(), 0, u"other example", url_);
    SizeUntilButtonsVisible(1);
    ASSERT_GE(test_helper_->GetBookmarkButtonCount(), 1u);

    menu_delegate_ = std::make_unique<BookmarkBarMenuDelegate>(
        browser(), nullptr, &dummy_menu_delegate_,
        BookmarkLaunchLocation::kSubfolder);
    menu_delegate_->SetActiveMenu(BookmarkParentFolder::OtherFolder(), 0);
    root_menu_.reset(menu_delegate_->menu());
    ASSERT_GE(root_menu_->GetSubmenu()->GetMenuItems().size(), 2u);
    menu_item_ = root_menu_->GetSubmenu()->GetMenuItemAt(1);
    ASSERT_EQ(u"other example", menu_item_->title());

    task_environment()->FastForwardBy(base::TimeDelta());
    ASSERT_EQ(bar_button()->GetImageModel(views::Button::STATE_NORMAL),
              ui::ImageModel::FromImage(initial_favicon_));
    ASSERT_EQ(menu_item_->GetIcon(),
              ui::ImageModel::FromImage(initial_favicon_));
    testing::Mock::VerifyAndClearExpectations(favicon_service_);
  }

  views::LabelButton* bar_button() {
    return test_helper_->GetBookmarkButton(0);
  }

 protected:
  const GURL url_{"https://www.example.com"};
  const gfx::Image initial_favicon_ = gfx::test::CreateImage(16, SK_ColorRED);
  const gfx::Image updated_favicon_ = gfx::test::CreateImage(16, SK_ColorBLUE);
  gfx::Image current_favicon_ = initial_favicon_;
  raw_ptr<favicon::MockFaviconService> favicon_service_ = nullptr;

  views::MenuDelegate dummy_menu_delegate_;
  std::unique_ptr<views::MenuItemView> root_menu_;
  std::unique_ptr<BookmarkBarMenuDelegate> menu_delegate_;
  raw_ptr<const BookmarkNode> bar_bookmark_ = nullptr;
  raw_ptr<const BookmarkNode> other_bookmark_ = nullptr;
  raw_ptr<views::MenuItemView> menu_item_ = nullptr;
};

TEST_F(BookmarkBarViewFaviconTest, ThrottlesFrequentFaviconChanges) {
  ASSERT_NO_FATAL_FAILURE(InitBookmarksAndMenu());

  current_favicon_ = updated_favicon_;
  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(url_, testing::_, testing::_))
      .Times(0);
  const base::TimeDelta step = base::Milliseconds(200);
  for (int i = 0; i < 3; ++i) {
    model()->OnFaviconsChanged({url_}, GURL());
    EXPECT_EQ(bar_button()->GetImageModel(views::Button::STATE_NORMAL),
              ui::ImageModel::FromImage(initial_favicon_));
    EXPECT_EQ(menu_item_->GetIcon(),
              ui::ImageModel::FromImage(initial_favicon_));
    task_environment()->FastForwardBy(step);
    EXPECT_EQ(bar_button()->GetImageModel(views::Button::STATE_NORMAL),
              ui::ImageModel::FromImage(initial_favicon_));
    EXPECT_EQ(menu_item_->GetIcon(),
              ui::ImageModel::FromImage(initial_favicon_));
  }
  testing::Mock::VerifyAndClearExpectations(favicon_service_);

  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(url_, testing::_, testing::_))
      .Times(2);
  task_environment()->FastForwardBy(BookmarkBarView::kFaviconUpdateDelay -
                                    step);
  EXPECT_EQ(bar_button()->GetImageModel(views::Button::STATE_NORMAL),
            ui::ImageModel::FromImage(updated_favicon_));
  EXPECT_EQ(menu_item_->GetIcon(), ui::ImageModel::FromImage(updated_favicon_));
}

TEST_F(BookmarkBarViewFaviconTest, DefersFaviconUpdateLoadedDuringTimer) {
  ASSERT_NO_FATAL_FAILURE(InitBookmarksAndMenu());

  current_favicon_ = updated_favicon_;
  const base::TimeDelta step = base::Milliseconds(200);
  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(url_, testing::_, testing::_))
      .Times(2);
  model()->OnFaviconsChanged({url_}, GURL());
  model()->GetFavicon(bar_bookmark_);
  model()->GetFavicon(other_bookmark_);
  task_environment()->FastForwardBy(step);
  ASSERT_TRUE(bar_bookmark_->is_favicon_loaded());
  ASSERT_TRUE(other_bookmark_->is_favicon_loaded());
  EXPECT_EQ(bar_button()->GetImageModel(views::Button::STATE_NORMAL),
            ui::ImageModel::FromImage(initial_favicon_));
  EXPECT_EQ(menu_item_->GetIcon(), ui::ImageModel::FromImage(initial_favicon_));
  testing::Mock::VerifyAndClearExpectations(favicon_service_);

  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(testing::_, testing::_, testing::_))
      .Times(0);
  task_environment()->FastForwardBy(BookmarkBarView::kFaviconUpdateDelay -
                                    step);
  EXPECT_EQ(bar_button()->GetImageModel(views::Button::STATE_NORMAL),
            ui::ImageModel::FromImage(updated_favicon_));
  EXPECT_EQ(menu_item_->GetIcon(), ui::ImageModel::FromImage(updated_favicon_));
}

TEST_F(BookmarkBarViewFaviconTest, UpdatesFaviconOnNodeChangedDuringTimer) {
  ASSERT_NO_FATAL_FAILURE(InitBookmarksAndMenu());

  const GURL new_url("https://www.new-example.com");
  const gfx::Image final_favicon = gfx::test::CreateImage(16, SK_ColorGREEN);

  current_favicon_ = updated_favicon_;
  model()->OnFaviconsChanged({url_}, GURL());
  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(url_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(new_url, testing::_, testing::_))
      .Times(2);
  model()->SetURL(bar_bookmark_, new_url,
                  bookmarks::metrics::BookmarkEditSource::kOther);
  model()->SetURL(other_bookmark_, new_url,
                  bookmarks::metrics::BookmarkEditSource::kOther);
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(bar_button()->GetImageModel(views::Button::STATE_NORMAL),
            ui::ImageModel::FromImage(updated_favicon_));
  EXPECT_EQ(menu_item_->GetIcon(), ui::ImageModel::FromImage(updated_favicon_));
  testing::Mock::VerifyAndClearExpectations(favicon_service_);

  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(testing::_, testing::_, testing::_))
      .Times(0);
  task_environment()->FastForwardBy(BookmarkBarView::kFaviconUpdateDelay);
  testing::Mock::VerifyAndClearExpectations(favicon_service_);

  current_favicon_ = final_favicon;
  model()->OnFaviconsChanged({new_url}, GURL());
  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(new_url, testing::_, testing::_))
      .Times(2);
  model()->SetTitle(bar_bookmark_, u"renamed",
                    bookmarks::metrics::BookmarkEditSource::kOther);
  model()->SetTitle(other_bookmark_, u"renamed",
                    bookmarks::metrics::BookmarkEditSource::kOther);
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(bar_button()->GetImageModel(views::Button::STATE_NORMAL),
            ui::ImageModel::FromImage(final_favicon));
  EXPECT_EQ(menu_item_->GetIcon(), ui::ImageModel::FromImage(final_favicon));
  testing::Mock::VerifyAndClearExpectations(favicon_service_);

  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(testing::_, testing::_, testing::_))
      .Times(0);
  task_environment()->FastForwardBy(BookmarkBarView::kFaviconUpdateDelay);
}

TEST_F(BookmarkBarViewFaviconTest, CancelsFaviconTimerOnNodeRemoval) {
  const GURL folder_url("https://www.folder-example.com");
  const BookmarkNode* folder =
      model()->AddFolder(model()->other_node(), 0, u"folder");
  model()->AddURL(folder, 0, u"folder example", folder_url);

  ASSERT_NO_FATAL_FAILURE(InitBookmarksAndMenu());
  ASSERT_EQ(3u, root_menu_->GetSubmenu()->GetMenuItems().size());
  views::MenuItemView* folder_menu_item =
      root_menu_->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_EQ(u"folder", folder_menu_item->title());
  menu_delegate_->WillShowMenu(folder_menu_item);
  ASSERT_EQ(1u, folder_menu_item->GetSubmenu()->GetMenuItems().size());
  views::MenuItemView* folder_child_item =
      folder_menu_item->GetSubmenu()->GetMenuItemAt(0);
  task_environment()->FastForwardBy(base::TimeDelta());
  ASSERT_EQ(folder_child_item->GetIcon(),
            ui::ImageModel::FromImage(initial_favicon_));
  testing::Mock::VerifyAndClearExpectations(favicon_service_);

  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(testing::_, testing::_, testing::_))
      .Times(0);
  model()->OnFaviconsChanged({folder_url}, GURL());
  model()->Remove(folder, bookmarks::metrics::BookmarkEditSource::kOther,
                  FROM_HERE);
  task_environment()->FastForwardBy(BookmarkBarView::kFaviconUpdateDelay);

  model()->OnFaviconsChanged({url_}, GURL());
  model()->RemoveAllUserBookmarks(FROM_HERE);
  task_environment()->FastForwardBy(BookmarkBarView::kFaviconUpdateDelay);
}

TEST_F(BookmarkBarViewFaviconTest,
       SkipsFaviconUpdatesForHiddenOverflowButtons) {
  const GURL overflow_url("https://www.overflow-example.com");
  const BookmarkNode* visible_bookmark =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"visible", url_);
  const BookmarkNode* overflow_bookmark = model()->AddURL(
      model()->bookmark_bar_node(), 1, u"overflow", overflow_url);
  SizeUntilButtonsVisible(1);
  ASSERT_EQ(2u, test_helper_->GetBookmarkButtonCount());
  views::LabelButton* visible_button = test_helper_->GetBookmarkButton(0);
  views::LabelButton* overflow_button = test_helper_->GetBookmarkButton(1);
  ASSERT_TRUE(visible_button->GetVisible());
  ASSERT_FALSE(overflow_button->GetVisible());

  task_environment()->FastForwardBy(base::TimeDelta());
  ASSERT_TRUE(visible_bookmark->is_favicon_loaded());
  ASSERT_TRUE(overflow_bookmark->is_favicon_loaded());
  testing::Mock::VerifyAndClearExpectations(favicon_service_);

  current_favicon_ = updated_favicon_;
  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(testing::_, testing::_, testing::_))
      .Times(0);
  model()->OnFaviconsChanged({overflow_url}, GURL());
  task_environment()->FastForwardBy(BookmarkBarView::kFaviconUpdateDelay);

  model()->OnFaviconsChanged({url_}, GURL());
  SizeDownUntilButtonsVisible(0);
  ASSERT_FALSE(visible_button->GetVisible());
  task_environment()->FastForwardBy(BookmarkBarView::kFaviconUpdateDelay);
  testing::Mock::VerifyAndClearExpectations(favicon_service_);

  EXPECT_CALL(*favicon_service_,
              GetFaviconImageForPageURL(testing::_, testing::_, testing::_))
      .Times(2);
  SizeUntilButtonsVisible(2);
  ASSERT_TRUE(visible_button->GetVisible());
  ASSERT_TRUE(overflow_button->GetVisible());
  EXPECT_EQ(visible_button->GetImageModel(views::Button::STATE_NORMAL),
            ui::ImageModel::FromImage(initial_favicon_));
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(visible_button->GetImageModel(views::Button::STATE_NORMAL),
            ui::ImageModel::FromImage(updated_favicon_));
  EXPECT_EQ(overflow_button->GetImageModel(views::Button::STATE_NORMAL),
            ui::ImageModel::FromImage(updated_favicon_));
}

class BookmarkBarViewAreaKeyboardNavigationTest
    : public BookmarkBarViewInWidgetTest,
      public testing::WithParamInterface<bool> {
 public:
  BookmarkBarViewAreaKeyboardNavigationTest() {
    area_nav_feature_list_.InitWithFeatureState(
        features::kBookmarkBarAreaKeyboardNavigation, GetParam());
  }

  // False on macOS even when the feature is enabled.
  bool IsAreaNavigationActive() const {
    return GetParam() && views::PlatformStyle::kDefaultFocusBehavior ==
                             views::View::FocusBehavior::ALWAYS;
  }

  void ShowWithBookmarks(const std::string& model_string) {
    widget()->Show();
    AddNodesToBookmarkBarFromModelString(model_string);
    bookmark_bar_view()->SetBounds(
        0, 0, 5000, bookmark_bar_view()->GetPreferredSize().height());
    views::test::RunScheduledLayout(bookmark_bar_view());
  }

  void PressKey(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::EventType::kKeyPressed, key_code, ui::EF_NONE);
    widget()->OnKeyEvent(&event);
  }

  views::View* GetFocusedView() {
    return widget()->GetFocusManager()->GetFocusedView();
  }

 protected:
  // ViewsTestBase:
  std::unique_ptr<views::Widget> AllocateTestWidget() override {
    return std::make_unique<FocusWithoutActivationWidget>();
  }

 private:
  // Some platforms (e.g. Wayland on Weston) never activate test windows, and
  // FocusManager defers focus requests on inactive widgets.
  class FocusWithoutActivationWidget : public views::Widget {
   public:
    bool CanFocusViewsWithoutActivation() const override { return true; }
  };

  base::test::ScopedFeatureList area_nav_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         BookmarkBarViewAreaKeyboardNavigationTest,
                         testing::Bool());

TEST_P(BookmarkBarViewAreaKeyboardNavigationTest, BookmarkAreaFocusBehavior) {
  ShowWithBookmarks("a b c ");
  ASSERT_EQ(3u, test_helper_->GetBookmarkButtonCount());

  EXPECT_EQ(IsAreaNavigationActive()
                ? views::PlatformStyle::kDefaultFocusBehavior
                : views::View::FocusBehavior::ACCESSIBLE_ONLY,
            test_helper_->GetBookmarkButton(0)->GetFocusBehavior());
  EXPECT_EQ(views::View::FocusBehavior::ACCESSIBLE_ONLY,
            test_helper_->GetBookmarkButton(1)->GetFocusBehavior());
  EXPECT_EQ(views::View::FocusBehavior::ACCESSIBLE_ONLY,
            test_helper_->GetBookmarkButton(2)->GetFocusBehavior());
}

TEST_P(BookmarkBarViewAreaKeyboardNavigationTest,
       BookmarkAreaFocusBehaviorFollowsFirstVisible) {
  if (!IsAreaNavigationActive()) {
    GTEST_SKIP() << "Area keyboard navigation is not active.";
  }
  ShowWithBookmarks("a b c ");
  ASSERT_EQ(3u, test_helper_->GetBookmarkButtonCount());

  model()->Remove(model()->bookmark_bar_node()->children()[0].get(),
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  views::test::RunScheduledLayout(bookmark_bar_view());
  ASSERT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  EXPECT_EQ(views::PlatformStyle::kDefaultFocusBehavior,
            test_helper_->GetBookmarkButton(0)->GetFocusBehavior());
  EXPECT_EQ(views::View::FocusBehavior::ACCESSIBLE_ONLY,
            test_helper_->GetBookmarkButton(1)->GetFocusBehavior());
}

// The Tab Groups area has at most one Tab-focusable button.
TEST_P(BookmarkBarViewAreaKeyboardNavigationTest, TabGroupAreaFocusBehavior) {
  if (!IsAreaNavigationActive()) {
    GTEST_SKIP() << "Area keyboard navigation is not active.";
  }
  ShowWithBookmarks("a ");
  tab_groups::SavedTabGroupBar* stg_bar = test_helper_->saved_tab_group_bar();
  ASSERT_TRUE(stg_bar);
  views::test::RunScheduledLayout(stg_bar);

  const std::vector<views::View*> buttons =
      stg_bar->GetVisibleKeyboardNavigationButtons();
  for (size_t i = 0; i < buttons.size(); ++i) {
    EXPECT_EQ(i == 0 ? views::PlatformStyle::kDefaultFocusBehavior
                     : views::View::FocusBehavior::ACCESSIBLE_ONLY,
              buttons[i]->GetFocusBehavior());
  }
}

// Left/Right arrows move focus within the Bookmarks area and wrap around.
TEST_P(BookmarkBarViewAreaKeyboardNavigationTest, ArrowKeysWrapWithinArea) {
  if (!IsAreaNavigationActive()) {
    GTEST_SKIP() << "Area keyboard navigation is not active.";
  }
  ShowWithBookmarks("a b c ");
  ASSERT_EQ(3u, test_helper_->GetBookmarkButtonCount());

  views::View* a = test_helper_->GetBookmarkButton(0);
  views::View* b = test_helper_->GetBookmarkButton(1);
  views::View* c = test_helper_->GetBookmarkButton(2);
  ASSERT_FALSE(bookmark_bar_view()->overflow_button()->GetVisible());
  // The "All Bookmarks" button, if visible, is the last item of the area.
  views::View* last = bookmark_bar_view()->all_bookmarks_button()->GetVisible()
                          ? bookmark_bar_view()->all_bookmarks_button()
                          : c;

  a->RequestFocus();
  ASSERT_EQ(a, GetFocusedView());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(b, GetFocusedView());
  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(c, GetFocusedView());
  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(b, GetFocusedView());
  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(a, GetFocusedView());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(last, GetFocusedView());
  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(a, GetFocusedView());
}

}  // namespace
