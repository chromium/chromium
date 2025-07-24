// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/test/tab_strip_interactive_test_mixin.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/data_sharing/public/features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_utils.h"
#include "url/url_constants.h"

namespace {
#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";
#endif  // !BUILDFLAG(IS_CHROMEOS)
}  // anonymous namespace

namespace tab_groups {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

class FaviconFetchObserver : public ui::test::ObservationStateObserver<
                                 bool,
                                 favicon::ContentFaviconDriver,
                                 favicon::FaviconDriverObserver> {
 public:
  FaviconFetchObserver(favicon::ContentFaviconDriver* driver,
                       const GURL& favicon_url)
      : ObservationStateObserver(driver),
        driver_(driver),
        target_favicon_url_(favicon_url) {}

  ~FaviconFetchObserver() override = default;

  bool GetStateObserverInitialState() const override {
    return GetCurrentFaviconURL() == target_favicon_url_;
  }

  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    if (icon_url == target_favicon_url_) {
      OnStateObserverStateChanged(true);
    }
  }

 private:
  GURL GetCurrentFaviconURL() const {
    content::NavigationController& controller =
        driver_->web_contents()->GetController();
    content::NavigationEntry* entry = controller.GetLastCommittedEntry();
    return entry ? entry->GetFavicon().url : GURL();
  }

  raw_ptr<favicon::ContentFaviconDriver> driver_;
  GURL target_favicon_url_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);

class SavedTabGroupInteractiveTestBase
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  SavedTabGroupInteractiveTestBase() = default;
  ~SavedTabGroupInteractiveTestBase() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  GURL GetURL(std::string_view path) {
    return embedded_test_server()->GetURL("example.com", path);
  }

  MultiStep ShowBookmarksBar() {
    return Steps(
        PressButton(kToolbarAppMenuButtonElementId),
    // TODO(https://crbug.com/359252812): On Linux and ChromeOS, sometimes
    // the bookmarks submenu randomly loses focus causing it to close.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
        WithoutDelay(
#endif
            SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
            SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
                )
#endif
            ,
        // On Mac the menu might still be animating closed, so wait for that.
        WaitForHide(AppMenuModel::kBookmarksMenuItem),
        WaitForShow(kBookmarkBarElementId));
  }

  auto WaitForTabCount(Browser* browser, int expected_count) {
    return Steps(
        PollState(kTabCountState,
                  [browser]() { return browser->tab_strip_model()->count(); }),
        WaitForState(kTabCountState, expected_count),
        StopObservingState(kTabCountState));
  }

  MultiStep OpenTabGroupEditorMenu(tab_groups::TabGroupId group_id) {
    return Steps(HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
                 WaitForShow(kTabGroupEditorBubbleId));
  }
};

class SavedTabGroupInteractiveTest
    : public SavedTabGroupInteractiveTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  SavedTabGroupInteractiveTest() = default;
  ~SavedTabGroupInteractiveTest() override = default;

  void SetUp() override {
    if (IsMigrationEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {tab_groups::kTabGroupSyncServiceDesktopMigration,
           data_sharing::features::kDataSharingFeature},
          {data_sharing::features::kDataSharingJoinOnly});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {tab_groups::kTabGroupSyncServiceDesktopMigration,
               data_sharing::features::kDataSharingFeature,
               data_sharing::features::kDataSharingJoinOnly});
    }

    SavedTabGroupInteractiveTestBase::SetUp();
  }

  bool IsMigrationEnabled() const { return GetParam(); }

  MultiStep HoverTabAt(int index) {
    const char kTabToHover[] = "Tab to hover";
    return Steps(NameDescendantViewByType<Tab>(kBrowserViewElementId,
                                               kTabToHover, index),
                 MoveMouseTo(kTabToHover));
  }

  MultiStep WaitToFetchFavicon(int tab_index, const GURL& favicon_url) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(FaviconFetchObserver,
                                        kFaviconFetchObserver);

    favicon::ContentFaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(
            browser()->tab_strip_model()->GetWebContentsAt(tab_index));

    return Steps(ObserveState(kFaviconFetchObserver,
                              std::make_unique<FaviconFetchObserver>(
                                  favicon_driver, favicon_url)),
                 WaitForState(kFaviconFetchObserver, true),
                 StopObservingState(kFaviconFetchObserver));
  }

  MultiStep WaitForTabMenuItemToLoadFavicon() {
    using FaviconLoadObserver =
        views::test::PollingViewObserver<bool, views::MenuItemView>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(FaviconLoadObserver,
                                        kFaviconLoadObserver);

    return Steps(
        PollView(kFaviconLoadObserver, STGTabsMenuModel::kTab,
                 [](const views::MenuItemView* menu_item_view) -> bool {
                   auto actual_image = menu_item_view->GetIcon();
                   auto expected_image = favicon::GetDefaultFaviconModel(
                       kColorTabGroupBookmarkBarBlue);
                   auto* color_provider = menu_item_view->GetColorProvider();
                   CHECK(!actual_image.IsEmpty());
                   return !gfx::BitmapsAreEqual(
                       *actual_image.Rasterize(color_provider).bitmap(),
                       *expected_image.Rasterize(color_provider).bitmap());
                 }),
        WaitForState(kFaviconLoadObserver, true));
  }

  StepBuilder CheckIfSavedGroupIsOpen(const base::Uuid* const saved_guid) {
    return Do([=, this]() {
      const std::optional<SavedTabGroup> group =
          service()->GetGroup(*saved_guid);
      ASSERT_TRUE(group);
      EXPECT_TRUE(group->local_group_id());
      EXPECT_TRUE(browser()->tab_strip_model()->group_model()->ContainsTabGroup(
          group->local_group_id().value()));
    });
  }

  StepBuilder CheckIfSavedGroupIsClosed(const base::Uuid* const saved_guid) {
    return Do([=, this]() {
      EXPECT_EQ(1u, service()->GetAllGroups().size());
      const std::optional<SavedTabGroup> group =
          service()->GetGroup(*saved_guid);
      ASSERT_TRUE(group);
      EXPECT_FALSE(group->local_group_id());
    });
  }

  StepBuilder CheckIfSavedGroupIsPinned(tab_groups::TabGroupId group_id,
                                        bool is_pinned) {
    return Do([=, this]() {
      EXPECT_EQ(is_pinned, service()->GetGroup(group_id)->is_pinned());
    });
  }

  StepBuilder SaveGroupViaModel(const tab_groups::TabGroupId local_group) {
    return Do([=, this]() {
      service()->AddGroup(
          SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(local_group));
      ASSERT_TRUE(service()->GetGroup(local_group).has_value());
    });
  }

  StepBuilder CreateEmptySavedGroup() {
    return Do([=, this]() {
      TabGroupSyncService* service =
          SavedTabGroupUtils::GetServiceForProfile(browser()->profile());
      service->AddGroup({u"Test Test",
                         tab_groups::TabGroupColorId::kBlue,
                         {},
                         /*position=*/std::nullopt,
                         base::Uuid::GenerateRandomV4(),
                         /*local_group_id=*/std::nullopt,
                         /*creator_cache_guid=*/std::nullopt,
                         /*last_updater_cache_guid=*/std::nullopt,
                         /*created_before_syncing_tab_groups=*/false,
                         /*creation_time=*/std::nullopt});
    });
  }

  StepBuilder CreateRemoteSavedGroup() {
    return Do([=, this]() {
      const base::Uuid group_guid = base::Uuid::GenerateRandomV4();
      const base::Uuid tab_guid = base::Uuid::GenerateRandomV4();
      SavedTabGroup group = {/*title=*/u"group_title",
                             /*color=*/tab_groups::TabGroupColorId::kBlue,
                             /*urls=*/
                             {{GetURL("/favicon/page_with_favicon.html"),
                               u"tab_title", group_guid, 0, tab_guid}},
                             /*position=*/std::nullopt,
                             /*saved_guid=*/group_guid,
                             /*local_group_id=*/std::nullopt,
                             /*creator_cache_guid=*/std::nullopt,
                             /*last_updater_cache_guid=*/std::nullopt,
                             /*created_before_syncing_tab_groups=*/false,
                             /*creation_time=*/std::nullopt};

      TabGroupSyncService* service =
          SavedTabGroupUtils::GetServiceForProfile(browser()->profile());

      if (IsMigrationEnabled()) {
        TabGroupSyncServiceImpl* service_impl =
            static_cast<TabGroupSyncServiceImpl*>(service);
        service_impl->GetModel()->AddedFromSync(std::move(group));
      } else {
        TabGroupSyncServiceProxy* service_proxy =
            static_cast<TabGroupSyncServiceProxy*>(service);
        service_proxy->GetModel()->AddedFromSync(std::move(group));
      }
    });
  }

  StepBuilder UnsaveGroupViaModel(const tab_groups::TabGroupId local_group) {
    return Do([=, this]() {
      service()->RemoveGroup(local_group);
      ASSERT_FALSE(service()->GetGroup(local_group).has_value());
    });
  }

  auto CheckEverythingButtonVisibility() {
    return EnsurePresent(kSavedTabGroupOverflowButtonElementId);
  }

  auto CheckActiveTabIndex(int index) {
    return CheckResult(
               [this]() {
                 return browser()->tab_strip_model()->active_index();
               },
               index)
        .SetDescription("CheckActiveTabIndex()");
  }

  // Verifies that the browser containing `group_id` - which may be null -
  // matches `matcher`. Use `testing::Eq(nullptr)` to check for null.
  template <typename M>
  auto CheckBrowserWithGroupId(TabGroupId group_id, M&& matcher) {
    return CheckResult(
        [group_id]() {
          return SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
        },
        std::forward<M>(matcher));
  }

  // Presses the enter/return key on the button to open the context menu.
  auto OpenTabGroupContextMenu() {
    return WithElement(kSavedTabGroupButtonElementId,
                       [](ui::TrackedElement* el) {
                         const ui::KeyEvent event(
                             ui::EventType::kKeyPressed,
                             ui::KeyboardCode::VKEY_RETURN, ui::DomCode::ENTER,
                             ui::EF_NONE, ui::DomKey::ENTER, base::TimeTicks(),
                             /*is_char=*/false);

                         AsView<SavedTabGroupButton>(el)->OnKeyPressed(event);
                       })
        .SetDescription("OpenTabGroupContextMenu()");
  }

  // Helper function to check the number of items in a saved tab group
  // submenu.
  auto CheckTabGroupSubmenuTabCount(ElementSpecifier menu_item,
                                    int expected_count) {
    return WithElement(menu_item, [expected_count](ui::TrackedElement* el) {
      views::MenuItemView* menu_item_view = AsView<views::MenuItemView>(el);
      views::SubmenuView* submenu = menu_item_view->GetSubmenu();
      CHECK(submenu);

      // There are 5 menu items in the menu not including the separator or tabs:
      // Open, move, unpin, delete, and the tabs title
      constexpr int num_non_tab_items_in_menu = 5;
      const int total_items = submenu->GetMenuItems().size();
      const int num_tabs = total_items - num_non_tab_items_in_menu;
      EXPECT_EQ(num_tabs, expected_count);
    });
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));
  }

  TabGroupSyncService* service() {
    return SavedTabGroupUtils::GetServiceForProfile(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest, CreateGroupAndSave) {
  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       UnsaveGroupFromTabGroupHeader) {
  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId),
      OpenTabGroupEditorMenu(group_id),
      // Delete the group and verify the button is no longer present for it.
      PressButton(kTabGroupEditorBubbleDeleteGroupButtonId),
      // Accept the deletion dialog.
      Do([&]() {
        browser()
            ->tab_group_deletion_dialog_controller()
            ->SimulateOkButtonForTesting();
      }),
      EnsureNotPresent(kSavedTabGroupButtonElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       UnsaveGroupFromButtonMenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      OpenTabGroupContextMenu(),
      // Select the delete group menu item.
      SelectMenuItem(STGTabsMenuModel::kDeleteGroupMenuItem),
      // Accept the deletion dialog.
      PressButton(kDeletionDialogOkButtonId),
      // Ensure the button is no longer present.
      EnsureNotPresent(kSavedTabGroupButtonElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       ClosingGroupWithKeyboardShortcutShowsDeletionDialog) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  ui::Accelerator accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_CLOSE_TAB, &accelerator));

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),

      // Close the tab with the keyboard shortcut command, wait for the dialog
      // to show, and accept it.
      SendAccelerator(kBrowserViewElementId, accelerator),
      PressButton(kDeletionDialogOkButtonId, InputType::kKeyboard),

      // Ensure the group was deleted.
      WaitForHide(kSavedTabGroupButtonElementId),
      Check([&]() { return service()->GetAllGroups().empty(); },
            "Check all groups is empty."));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest, UnpinGroupFromButtonMenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // Ensure the tab group is pinned.
      CheckIfSavedGroupIsPinned(group_id, /*is_pinned=*/true),
      FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      OpenTabGroupContextMenu(),
      // Flush events and select the unpin group menu item.
      EnsurePresent(STGTabsMenuModel::kToggleGroupPinStateMenuItem),

      SelectMenuItem(STGTabsMenuModel::kToggleGroupPinStateMenuItem),
      FinishTabstripAnimations(), WaitForHide(kSavedTabGroupButtonElementId),
      // Ensure the tab group is unpinned.
      CheckIfSavedGroupIsPinned(group_id, /*is_pinned=*/false));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       ContextMenuShowForEverythingMenuTabGroupItem) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForShow(STGEverythingMenu::kTabGroup),
      WithElement(
          STGEverythingMenu::kTabGroup,
          [](ui::TrackedElement* el) {
            ui::test::EventGenerator event_generator(
                views::GetRootWindow(AsView<views::View>(el)->GetWidget()));
            event_generator.MoveMouseTo(
                AsView<views::View>(el)->GetBoundsInScreen().CenterPoint());
            event_generator.ClickRightButton();
          }),
      EnsurePresent(STGTabsMenuModel::kMoveGroupToNewWindowMenuItem),
      EnsurePresent(STGTabsMenuModel::kToggleGroupPinStateMenuItem),
      EnsurePresent(STGTabsMenuModel::kDeleteGroupMenuItem),
      EnsurePresent(STGTabsMenuModel::kTabsTitleItem));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       SubmenuShowForAppMenuTabGroups) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      EnsurePresent(STGTabsMenuModel::kOpenGroup),
      EnsurePresent(STGTabsMenuModel::kMoveGroupToNewWindowMenuItem),
      EnsurePresent(STGTabsMenuModel::kToggleGroupPinStateMenuItem),
      EnsurePresent(STGTabsMenuModel::kDeleteGroupMenuItem),
      EnsurePresent(STGTabsMenuModel::kTabsTitleItem),
      EnsurePresent(STGTabsMenuModel::kTab));
}

// Regression test. See https://crbug.com/416815123.
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       ViewingMultipleGroupSubmenusDoesNotDuplicateTabsList) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->AddToNewGroup({1});

  const char kEverythingMenuRootViewId[] = "EverythingMenuRootView";
  const char kSecondSavedGroupItem[] = "SecondSavedGroupItem";

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),

      // Name the root view of the STGEverythingMenu so we can index into it.
      NameViewRelative(
          AppMenuModel::kTabGroupsMenuItem, kEverythingMenuRootViewId,
          [](views::MenuItemView* item) { return item->GetSubmenu(); }),
      WaitForShow(kEverythingMenuRootViewId),

      // Identify the second saved group. The menu looks as follows:
      // Index 0: "Create new tab group"
      // Index 1: First saved group (aka STGEverythingMenu::kTabGroup)
      // Index 2: Second saved group
      NameDescendantViewByType<views::MenuItemView>(kEverythingMenuRootViewId,
                                                    kSecondSavedGroupItem, 2),

      // Check the number of tabs in the first group.
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      WaitForShow(STGTabsMenuModel::kOpenGroup),
      CheckTabGroupSubmenuTabCount(STGEverythingMenu::kTabGroup, 1),

      // Check the number of tabs in the second group.
      SelectMenuItem(kSecondSavedGroupItem),
      WaitForShow(STGTabsMenuModel::kOpenGroup),
      CheckTabGroupSubmenuTabCount(kSecondSavedGroupItem, 1),

      // Check the number of tabs in the first group again.
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      WaitForShow(STGTabsMenuModel::kOpenGroup),
      CheckTabGroupSubmenuTabCount(STGEverythingMenu::kTabGroup, 1),

      // Close the menu to prevent flakes.
      HoverTabAt(0), ClickMouse(),
      WaitForHide(AppMenuModel::kTabGroupsMenuItem));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       OpenGroupFromAppMenuTabGroupSubmenu) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Add 2 tabs to the group.
  const tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  base::Uuid saved_guid;

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure saved group buttons in the bookmarks bar are present.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // The group we just saved should be the only group in the model.
      CheckResult([&]() { return service()->GetAllGroups().size(); }, 1u),
      // Find the saved guid that is linked to the group we just saved.
      Do([&]() {
        const std::optional<SavedTabGroup> saved_group =
            service()->GetGroup(local_group_id);
        ASSERT_TRUE(saved_group);
        saved_guid = saved_group->saved_guid();
      }),
      // Open the editor bubble, close thegroup and expect the saved group is no
      // longer linked.
      OpenTabGroupEditorMenu(local_group_id),
      PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
      FinishTabstripAnimations(), CheckIfSavedGroupIsClosed(&saved_guid),
      // Reopen the tab group from the app menu tab group's submenu.
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      SelectMenuItem(STGTabsMenuModel::kOpenGroup), FinishTabstripAnimations(),
      WaitForShow(kTabGroupHeaderElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MoveGroupToNewWindowFromAppMenuTabGroupSubmenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      SelectMenuItem(STGTabsMenuModel::kMoveGroupToNewWindowMenuItem),
      WaitForTabCount(browser(), 1),
      // Expect the browser with the tab group is not the original browser.
      CheckBrowserWithGroupId(group_id, testing::Ne(browser())));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       UnpinGroupFromAppMenuTabGroupSubmenu) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved into the bookmarks bar.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // Ensure the tab group is pinned.
      CheckIfSavedGroupIsPinned(group_id, /*is_pinned=*/true),
      FinishTabstripAnimations(), PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      SelectMenuItem(STGTabsMenuModel::kToggleGroupPinStateMenuItem),
      WaitForHide(kSavedTabGroupButtonElementId),
      CheckIfSavedGroupIsPinned(group_id, /*is_pinned=*/false));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       DeleteGroupFromAppMenuTabGroupSubmenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(FinishTabstripAnimations(), ShowBookmarksBar(),
                  PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
                  SelectMenuItem(STGEverythingMenu::kTabGroup),
                  // Selecting the menu item will spanw a dialog instead
                  SelectMenuItem(STGTabsMenuModel::kDeleteGroupMenuItem),
                  PressButton(kDeletionDialogOkButtonId),
                  // Expect the saved tab group button disappears.
                  WaitForHide(kSavedTabGroupButtonElementId),
                  // Expect the original browser has one less tab.
                  WaitForTabCount(browser(), 1),
                  // Expect the browser has no such a tab group.
                  CheckBrowserWithGroupId(group_id, testing::Eq(nullptr)));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       OpenTabFromAppMenuTabGroupSubmenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  GURL test_url(chrome::kChromeUINewTabURL);
  ASSERT_TRUE(AddTabAtIndex(0, test_url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem)
          .SetMustRemainVisible(true),
      SelectMenuItem(STGEverythingMenu::kTabGroup).SetMustRemainVisible(true),
      SelectMenuItem(STGTabsMenuModel::kTab),

      // Expect the original browser has 1 more tab.
      WaitForTabCount(browser(), 3),
      // Expect the active tab is the one opened from submenu.
      CheckResult(
          [&]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetURL();
          },
          test_url),
      // Expect the active tab is at the end of tab strip.
      CheckActiveTabIndex(2));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MoveGroupToNewWindowFromButtonMenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the tab group was saved into the bookmarks bar.
      EnsurePresent(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      OpenTabGroupContextMenu(),
      // Select the move group to new window menu item.
      SelectMenuItem(STGTabsMenuModel::kMoveGroupToNewWindowMenuItem),
      // Ensure the button is no longer present.
      FinishTabstripAnimations(),
      // Expect the original browser has 1 less tab.
      WaitForTabCount(browser(), 1),
      // Expect the browser with the tab group is not the original browser.
      CheckBrowserWithGroupId(group_id, testing::Ne(browser())));
}

IN_PROC_BROWSER_TEST_P(
    SavedTabGroupInteractiveTest,
    MoveGroupToNewWindowFromButtonMenuDoesNothingIfOnlyGroupInWindow) {
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the tab group was saved.
      EnsurePresent(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      OpenTabGroupContextMenu(),
      // Select the move group to new window menu item.
      SelectMenuItem(STGTabsMenuModel::kMoveGroupToNewWindowMenuItem),
      // Ensure the button is no longer present.
      FinishTabstripAnimations(),
      // Expect the original browser has 1 less tab.
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      // Expect the browser with the tab group is the original browser.
      CheckBrowserWithGroupId(group_id, browser()));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       FirstTabIsFocusedInReopenedSavedGroup) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  base::Uuid saved_guid = service()->GetGroup(local_group_id)->saved_guid();

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure no saved group buttons in the bookmarks bar are present.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // The group we just saved should be the only group in the model.
      CheckResult([&]() { return service()->GetAllGroups().size(); }, 1),
      // Open the editor bubble and close the group.
      FinishTabstripAnimations(), OpenTabGroupEditorMenu(local_group_id),
      // Close the tab group and expect the saved group is no longer linked.
      PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
      FinishTabstripAnimations(), CheckIfSavedGroupIsClosed(&saved_guid),
      // Reopen the tab group and expect the saved group is linked again.
      PressButton(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),
      CheckIfSavedGroupIsOpen(&saved_guid),
      // Verify the first tab in the group is the active tab.
      CheckActiveTabIndex(1));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MoveTabInsideAndOutsideGroup) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Add 2 tabs to the group.
  const tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(), Do([&]() {
        browser()->tab_strip_model()->MoveWebContentsAt(1, 2, false);
      }),
      CheckResult(
          [&]() { return browser()->tab_strip_model()->GetTabGroupForTab(2); },
          std::nullopt),
      Do([&]() {
        std::vector<int> indices = {2};
        browser()->tab_strip_model()->AddToExistingGroup(indices,
                                                         local_group_id);
      }),
      CheckResult(
          [&]() { return browser()->tab_strip_model()->GetTabGroupForTab(1); },
          local_group_id));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       UpdateButtonWhenTabGroupVisualDataChanges) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  const std::u16string new_title = u"New title";
  const tab_groups::TabGroupColorId new_color =
      tab_groups::TabGroupColorId::kPurple;

  TabGroup* const group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
  const tab_groups::TabGroupVisualData* const old_visual_data =
      group->visual_data();

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the button was added to tbe bookmarks bar.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // Verify the button in the bookmarks bar has the same color and title
      // as the tab group.
      CheckViewProperty(kSavedTabGroupButtonElementId,
                        &SavedTabGroupButton::GetText,
                        old_visual_data->title()),
      CheckViewProperty(kSavedTabGroupButtonElementId,
                        &SavedTabGroupButton::tab_group_color_id,
                        old_visual_data->color()),
      // Update the text and color.
      Do([&]() {
        browser()->tab_strip_model()->ChangeTabGroupVisuals(
            group_id, {new_title, new_color});
      }),
      // Verify the button has the same color and title as the tab group.
      CheckViewProperty(kSavedTabGroupButtonElementId,
                        &SavedTabGroupButton::GetText, new_title),
      CheckViewProperty(kSavedTabGroupButtonElementId,
                        &SavedTabGroupButton::tab_group_color_id, new_color),
      // Click the tab group header to close the menu.
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::LEFT),
      FinishTabstripAnimations());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       CreateNewTabGroupFromEverythingMenu) {
  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      CheckEverythingButtonVisibility(),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      EnsureNotPresent(kTabGroupEditorBubbleId),
      PressButton(kSavedTabGroupOverflowButtonElementId),
      EnsurePresent(STGEverythingMenu::kCreateNewTabGroup),
      SelectMenuItem(STGEverythingMenu::kCreateNewTabGroup),
      FinishTabstripAnimations(), WaitForShow(kTabGroupEditorBubbleId),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2),
      // This menu item opens a new tab and the editor bubble.
      CheckActiveTabIndex(1),
      CheckResult(
          [&]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .host_piece();
          },
          chrome::kChromeUINewTabHost));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       OpenSavedGroupFromEverythingMenu) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Add 2 tabs to the group.
  const tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  base::Uuid saved_guid;

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // The group we just saved should be the only group in the model.
      CheckResult([&]() { return service()->GetAllGroups().size(); }, 1),
      // Find the saved guid that is linked to the group we just saved.
      Do([&]() {
        const std::optional<SavedTabGroup> saved_group =
            service()->GetGroup(local_group_id);
        ASSERT_TRUE(saved_group);
        saved_guid = saved_group->saved_guid();
      }),
      // Open the tab group editor bubble.
      OpenTabGroupEditorMenu(local_group_id),
      // Close the tab group and expect the saved group is no longer linked.
      PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
      FinishTabstripAnimations(), CheckIfSavedGroupIsClosed(&saved_guid),
      // Open the saved tab group from the Everything menu item.
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForHide(kTabGroupEditorBubbleId),
      SelectMenuItem(STGEverythingMenu::kTabGroup), FinishTabstripAnimations(),
      WaitForShow(kTabGroupHeaderElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       CreateNewTabGroupFromAppMenuSubmenu) {
  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      CheckEverythingButtonVisibility(),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      EnsureNotPresent(kTabGroupEditorBubbleId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kCreateNewTabGroup),
      FinishTabstripAnimations(), WaitForShow(kTabGroupEditorBubbleId),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2),
      // This menu item opens a new tab and the editor bubble.
      CheckActiveTabIndex(1),
      CheckResult(
          [&]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .host_piece();
          },
          chrome::kChromeUINewTabHost));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       EverythingButtonAlwaysShowsForV2) {
  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      CheckEverythingButtonVisibility(),
      // Press the enter/return key on the button to open the context menu.
      OpenTabGroupContextMenu(),
      // Select the delete group menu item.
      SelectMenuItem(STGTabsMenuModel::kDeleteGroupMenuItem),
      // Accept the deletion dialog.
      PressButton(kDeletionDialogOkButtonId),
      // Ensure the button is no longer present.
      WaitForHide(kSavedTabGroupButtonElementId),
      CheckEverythingButtonVisibility());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       ClosingLastGroupedTabInWindowCreatesNewTab) {
  constexpr char kTabCloseButton[] = "tab_close_button";

  browser()->tab_strip_model()->AddToNewGroup({0});
  RunTestSequence(
      FinishTabstripAnimations(), SelectTab(kTabStripElementId, 0),
      NameViewRelative(kTabStripElementId, kTabCloseButton,
                       [](TabStrip* tab_strip) {
                         return tab_strip->tab_at(0)->close_button().get();
                       }),
      // Close the last tab in the browser which.
      PressButton(kTabCloseButton), PressButton(kDeletionDialogOkButtonId),
      // Ensure the saved group was deleted.
      EnsureNotPresent(kSavedTabGroupButtonElementId),
      // Verify that removing the last grouped tab in the browser keeps the
      // browser open with one tab.
      Do([this]() { EXPECT_EQ(browser()->tab_strip_model()->count(), 1); }));
}

// TODO(crbug.com/40264110): Re-enable this test once it doesn't get stuck in
// drag and drop. Maybe related issue - the relative positioning seems to be
// interpreted as an absolute position.
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       DISABLED_DragGroupWithinBar) {
  // Create two tab groups with one tab each.
  const tab_groups::TabGroupId group_id_1 =
      browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->InsertWebContentsAt(1, CreateWebContents(),
                                                    AddTabTypes::ADD_NONE);
  const tab_groups::TabGroupId group_id_2 =
      browser()->tab_strip_model()->AddToNewGroup({1});
  BrowserView::GetBrowserViewForBrowser(browser())->tabstrip()->StopAnimating(
      true);

  const char kSavedTabGroupButton1[] = "SavedTabGroupButton1";
  const char kSavedTabGroupButton2[] = "SavedTabGroupButton2";
  auto right_center =
      base::BindLambdaForTesting([](ui::TrackedElement* element) {
        return element->AsA<views::TrackedElementViews>()
            ->view()
            ->GetLocalBounds()
            .right_center();
      });

  RunTestSequence(
      // This comment fixes the auto formatting, do not remove.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Find the buttons in the saved tab groups bar.
      NameChildViewByType<SavedTabGroupButton>(kSavedTabGroupBarElementId,
                                               kSavedTabGroupButton1, 0),
      NameChildViewByType<SavedTabGroupButton>(kSavedTabGroupBarElementId,
                                               kSavedTabGroupButton2, 1),
      // Drag button 1 to the right of button 2.
      MoveMouseTo(kSavedTabGroupButton1),
      DragMouseTo(kSavedTabGroupButton2, std::move(right_center)));

  EXPECT_EQ(1u, service()->GetGroup(group_id_1)->position());
  EXPECT_EQ(0u, service()->GetGroup(group_id_2)->position());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       EverythingMenuDoesntDisplayEmptyGroups) {
  // The everything menu is only enabled in V2.

  RunTestSequence(
      // Create an empty group
      FinishTabstripAnimations(), ShowBookmarksBar(), CreateEmptySavedGroup(),
      // Open the everything menu and expect the group to not show up.
      CheckEverythingButtonVisibility(),
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForShow(STGEverythingMenu::kCreateNewTabGroup),
      EnsureNotPresent(STGEverythingMenu::kTabGroup));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       AppMenuDoesntDisplayEmptyGroups) {
  // The everything menu is only enabled in V2.

  RunTestSequence(
      // Create an empty group
      FinishTabstripAnimations(), ShowBookmarksBar(), CreateEmptySavedGroup(),
      // Open the app menu tab group menu.
      PressButton(kToolbarAppMenuButtonElementId),

      WaitForShow(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      WaitForShow(STGEverythingMenu::kCreateNewTabGroup),
      // Expect the group to not be displayed.
      EnsureNotPresent(STGEverythingMenu::kTabGroup));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       AppMenuTabGroupsShowsCorrectFavicons) {
  RunTestSequence(
      InstrumentTab(kFirstTab),
      // Navigate to a page with favicon and wait for the favicon
      // to be updated to the local favicon database
      NavigateWebContents(kFirstTab, GetURL("/favicon/page_with_favicon.html")),
      WaitToFetchFavicon(0, GetURL("/favicon/icon.png")),
      CreateRemoteSavedGroup(), PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      WaitForShow(STGEverythingMenu::kTabGroup),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      // Validate if the menu item view loaded a favicon from the database
      WaitForShow(STGTabsMenuModel::kTab), WaitForTabMenuItemToLoadFavicon());
}

#if !BUILDFLAG(IS_CHROMEOS)
class TabGroupShortcutsInteractiveTest
    : public SavedTabGroupInteractiveTestBase {
 public:
  TabGroupShortcutsInteractiveTest() = default;
  ~TabGroupShortcutsInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({tabs::kTabGroupShortcuts}, {});
    SavedTabGroupInteractiveTestBase::SetUp();
  }

  StepBuilder WaitForIndexToBecomeActiveTab(int index) {
    return Do([=, this]() {
      EXPECT_TRUE(base::test::RunUntil([&]() {
        return browser()->tab_strip_model()->active_index() == index;
      }));
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabGroupShortcutsInteractiveTest,
                       ScreenshotAcceleratorsInCreateNewTabInGroupMenuItem) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const TabGroupId group_id = browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), OpenTabGroupEditorMenu(group_id),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      // Verify the tab group editor bubble has the accelerators for New tab in
      // group.
      Screenshot(kTabGroupEditorBubbleNewTabInGroupButtonId,
                 "tab_group_editor_bubble_new_tab_in_group_with_accelerators",
                 "6564307"),
      // Close the tab group editor bubble to prevent flakes on mac.
      HoverTabAt(0), ClickMouse(),
      WaitForHide(kTabGroupEditorBubbleNewTabInGroupButtonId));
}

IN_PROC_BROWSER_TEST_F(TabGroupShortcutsInteractiveTest,
                       ScreenshotAcceleratorsInCloseGroupMenuItem) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const TabGroupId group_id = browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), OpenTabGroupEditorMenu(group_id),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      // Verify the tab group editor bubble has the accelerators for Close
      // group.
      Screenshot(kTabGroupEditorBubbleCloseGroupButtonId,
                 "tab_group_editor_bubble_close_group_with_accelerators",
                 "6564307"),
      // Close the tab group editor bubble to prevent flakes on mac.
      HoverTabAt(0), ClickMouse(),
      WaitForHide(kTabGroupEditorBubbleCloseGroupButtonId));
}

IN_PROC_BROWSER_TEST_F(TabGroupShortcutsInteractiveTest,
                       ScreenshotAcceleratorsInTabGroupSubmenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  const char kEverythingMenuRootViewId[] = "EverythingMenuRootView";

  RunTestSequence(
      FinishTabstripAnimations(), PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      NameViewRelative(
          AppMenuModel::kTabGroupsMenuItem, kEverythingMenuRootViewId,
          [](views::MenuItemView* item) { return item->GetSubmenu(); }),
      WaitForShow(kEverythingMenuRootViewId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      // Verify the app menu has the accelerator for Create new tab group.
      Screenshot(kEverythingMenuRootViewId,
                 "tab_group_app_menu_with_accelerators", "6564307"),
      // Close the app menu to prevent flakes on mac.
      HoverTabAt(0), ClickMouse(),
      WaitForHide(AppMenuModel::kTabGroupsMenuItem));
}

IN_PROC_BROWSER_TEST_F(TabGroupShortcutsInteractiveTest,
                       NewTabAddedToEndOfActiveTabsGroupWithKeyboardShortcut) {
  ui::Accelerator create_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_CREATE_NEW_TAB_GROUP, &create_accelerator));

  ui::Accelerator add_new_tab_to_group_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_ADD_NEW_TAB_TO_GROUP, &add_new_tab_to_group_accelerator));

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Use the keyboard shortcut command to create a new tab group.
      SendAccelerator(kBrowserViewElementId, create_accelerator),
      EnsurePresent(kTabGroupHeaderElementId),
      // Refocus the first tab in order to close the tab group editor bubble.
      PressButton(kSavedTabGroupButtonElementId),
      // Use the keyboard shortcut command to add a new tab at the end of the
      // group.
      SendAccelerator(kBrowserViewElementId, add_new_tab_to_group_accelerator),

      // Verify the tab was added to the group.
      CheckResult(
          [&]() {
            int active_index = browser()->tab_strip_model()->active_index();
            std::optional<TabGroupId> group_id =
                browser()->tab_strip_model()->GetTabGroupForTab(active_index);
            EXPECT_TRUE(group_id);

            return browser()
                ->tab_strip_model()
                ->group_model()
                ->GetTabGroup(group_id.value())
                ->ListTabs()
                .length();
          },
          2));
}

IN_PROC_BROWSER_TEST_F(TabGroupShortcutsInteractiveTest,
                       CreateNewGroupAndCloseItWithKeyboardShortcuts) {
  ui::Accelerator create_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_CREATE_NEW_TAB_GROUP, &create_accelerator));

  ui::Accelerator close_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_CLOSE_TAB_GROUP, &close_accelerator));

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Use the keyboard shortcut command to create a new tab group.
      SendAccelerator(kBrowserViewElementId, create_accelerator),
      EnsurePresent(kTabGroupHeaderElementId),
      // Refocus the first tab in order to close the tab group editor bubble.
      PressButton(kSavedTabGroupButtonElementId),
      // Use the keyboard shortcut command to close the tab group.
      SendAccelerator(kBrowserViewElementId, close_accelerator),
      WaitForHide(kTabGroupHeaderElementId),
      // Verify the group is still saved.
      EnsurePresent(kSavedTabGroupButtonElementId));
}

IN_PROC_BROWSER_TEST_F(TabGroupShortcutsInteractiveTest,
                       FocusNextAndPrevTabGroupWithKeyboardShortcuts) {
  ui::Accelerator create_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_CREATE_NEW_TAB_GROUP, &create_accelerator));

  ui::Accelerator focus_next_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_TAB_GROUP, &focus_next_accelerator));

  ui::Accelerator focus_prev_accelerator;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_PREV_TAB_GROUP, &focus_prev_accelerator));

  RunTestSequence(
      FinishTabstripAnimations(),
      // Use the keyboard shortcut command to create 3 new tab groups.
      //
      // NOTE: Ensure the tab group editor bubble that is opened when creating a
      // new group is closed before running subsequent SendAccelerator calls.
      // Otherwise the commands will get eaten by that view and the tests will
      // fail.
      SendAccelerator(kBrowserViewElementId, create_accelerator),
      FinishTabstripAnimations(), WaitForIndexToBecomeActiveTab(1),
      SendAccelerator(kTabGroupEditorBubbleId,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
      WaitForHide(kTabGroupEditorBubbleId), FinishTabstripAnimations(),

      SendAccelerator(kBrowserViewElementId, create_accelerator),
      FinishTabstripAnimations(), WaitForIndexToBecomeActiveTab(2),
      SendAccelerator(kTabGroupEditorBubbleId,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
      WaitForHide(kTabGroupEditorBubbleId), FinishTabstripAnimations(),

      SendAccelerator(kBrowserViewElementId, create_accelerator),
      FinishTabstripAnimations(), WaitForIndexToBecomeActiveTab(3),
      SendAccelerator(kTabGroupEditorBubbleId,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
      WaitForHide(kTabGroupEditorBubbleId), FinishTabstripAnimations(),

      CheckResult(
          [&]() {
            return browser()
                ->tab_strip_model()
                ->group_model()
                ->ListTabGroups()
                .size();
          },
          3u),

      // Cycle through the first tabs of the next tab groups until we land on
      // the third group again using the next keyboard shortcut.
      SendAccelerator(kBrowserViewElementId, focus_next_accelerator),
      WaitForIndexToBecomeActiveTab(1),
      SendAccelerator(kBrowserViewElementId, focus_next_accelerator),
      WaitForIndexToBecomeActiveTab(2),
      SendAccelerator(kBrowserViewElementId, focus_next_accelerator),
      WaitForIndexToBecomeActiveTab(3),

      // Cycle through the first tabs of the previous tab groups until we land
      // on the third group again using the prev keyboard shortcut.
      SendAccelerator(kBrowserViewElementId, focus_prev_accelerator),
      WaitForIndexToBecomeActiveTab(2),
      SendAccelerator(kBrowserViewElementId, focus_prev_accelerator),
      WaitForIndexToBecomeActiveTab(1),
      SendAccelerator(kBrowserViewElementId, focus_prev_accelerator),
      WaitForIndexToBecomeActiveTab(3));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(SavedTabGroupBar,
                         SavedTabGroupInteractiveTest,
                         testing::Bool());

}  // namespace tab_groups
