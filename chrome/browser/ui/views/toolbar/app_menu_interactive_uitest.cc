// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_sessions/session_store.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace {

views::MenuItemView* FindMenuItemWithTitle(views::View* root,
                                           std::u16string title) {
  if (auto* const menu_item = views::AsViewClass<views::MenuItemView>(root)) {
    if (menu_item->title() == title) {
      return menu_item;
    }
  }
  for (views::View* child : root->children()) {
    if (auto* const result = FindMenuItemWithTitle(child, title)) {
      return result;
    }
  }
  return nullptr;
}

auto NameMenuItemWithTitle(std::string_view name, std::u16string title) {
  return views::test::InteractiveViewsTestApi::NameView(
      name, base::BindLambdaForTesting([=]() -> views::View* {
        for (views::Widget* widget : views::test::WidgetTest::GetAllWidgets()) {
          if (auto* const result =
                  FindMenuItemWithTitle(widget->GetRootView(), title)) {
            return result;
          }
        }
        return nullptr;
      }));
}

}  // namespace

// TODO(crbug.com/498258602): Drag and drop tests are flaky on MacOS and
// ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

namespace {

class AppMenuDragAndDropInteractiveTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  AppMenuDragAndDropInteractiveTest() {
    scoped_feature_list_.InitWithFeatureState(features::kAppMenuGlowUp,
                                              GetParam());
  }
  ~AppMenuDragAndDropInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        bookmarks::prefs::kShowBookmarkBar, true);
  }

  auto TopCenter() {
    return base::BindLambdaForTesting([this](ui::TrackedElement* el) {
      auto* const view =
          views::test::InteractiveViewsTestApi::AsView<views::View>(el);
      return view->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 5);
    });
  }

  // On Linux, dropping a bookmark closes the app menu, so re-open the
  // bookmarks submenu. On other platforms the menu stays open.
  auto EnsureBookmarksMenuOpenAfterDrop() {
#if BUILDFLAG(IS_LINUX)
    return Steps(PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem));
#else
    return Steps();
#endif
  }

  auto CheckMenuItemBefore(ElementSpecifier item_after_spec,
                           std::u16string item_before_title) {
    return CheckView(item_after_spec, [item_before_title](
                                          views::MenuItemView* a) {
      views::View* parent = a->parent();
      for (views::View* child : parent->children()) {
        if (auto* menu_item = views::AsViewClass<views::MenuItemView>(child)) {
          if (menu_item->title() == item_before_title) {
            return true;
          }
          if (menu_item == a) {
            return false;
          }
        }
      }
      return false;
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppMenuDragAndDropInteractiveTest,
                       BookmarksDragAndDrop) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node, 1, u"b");

  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kANodeMenuId, CenterPoint(), /*release=*/false)
          .SetMustRemainVisible(false),
      MoveMouseTo(kANodeMenuId, TopCenter()), ReleaseMouse(),
      EnsureBookmarksMenuOpenAfterDrop(),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckMenuItemBefore(kANodeMenuId, u"b"));
}

IN_PROC_BROWSER_TEST_P(AppMenuDragAndDropInteractiveTest,
                       BookmarksDragAndDropToNestedFolder) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node, 1, u"b");

  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kANodeMenuId),
      DragMouseTo(kBNodeMenuId, CenterPoint()).SetMustRemainVisible(false),
      EnsureBookmarksMenuOpenAfterDrop(),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      SelectMenuItem(kBNodeMenuId), NameMenuItemWithTitle("a_in_b", u"a"),
      CheckViewProperty("a_in_b", &views::MenuItemView::title, u"a"));
}

IN_PROC_BROWSER_TEST_P(AppMenuDragAndDropInteractiveTest,
                       BookmarksDragAndDropFromNestedFolder) {
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "System DnD simulation is not supported on Wayland.";
  }
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* const bb_node = model->bookmark_bar_node();
  model->AddFolder(bb_node, 0, u"a");
  model->AddFolder(bb_node->children()[0].get(), 0, u"b");

  static constexpr char kANodeMenuId[] = "a_node";
  static constexpr char kBNodeMenuId[] = "b_node";

  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      SelectMenuItem(kANodeMenuId), NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      MoveMouseTo(kBNodeMenuId),
      DragMouseTo(kANodeMenuId, CenterPoint(), /*release=*/false)
          .SetMustRemainVisible(false),
      MoveMouseTo(kANodeMenuId, TopCenter()), ReleaseMouse(),
      EnsureBookmarksMenuOpenAfterDrop(),
      NameMenuItemWithTitle(kBNodeMenuId, u"b"),
      CheckViewProperty(kBNodeMenuId, &views::MenuItemView::title, u"b"),
      NameMenuItemWithTitle(kANodeMenuId, u"a"),
      CheckViewProperty(kANodeMenuId, &views::MenuItemView::title, u"a"),
      CheckMenuItemBefore(kANodeMenuId, u"b"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppMenuDragAndDropInteractiveTest,
                         testing::Bool());

}  // namespace

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

using AppMenuInteractiveTest = InteractiveBrowserTest;

IN_PROC_BROWSER_TEST_F(AppMenuInteractiveTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(
      PressButton(kToolbarAppMenuButtonElementId),
      Do([this]() { browser()->GetWindow()->Close(); }),
      WaitForHide(kBrowserViewElementId));
}

class AppMenuSearchInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuSearchInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAppMenuGlowUp, features::kChroMenuSearch}, {});
  }
  ~AppMenuSearchInteractiveTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_LINUX)
    // On Linux with GTK4, GtkIMMulticontext re-injects unhandled non-printable
    // keys onto an offscreen dummy GtkWindow when a TextInputClient is focused.
    // Disable the GTK IME module so synthetic X11 key events route through
    // InputMethodAuraLinux synchronously.
    command_line->AppendSwitch(switches::kDisableGtkIme);
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppMenuSearchInteractiveTest, SearchBarKeyEventRouting) {
  RunTestSequence(
      MoveMouseTo(kToolbarBackButtonElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuSearchBarView::kSearchFieldElementId),
      WaitForShow(AppMenuModel::kMoreToolsMenuItem),
      CheckViewProperty(AppMenuSearchBarView::kSearchFieldElementId,
                        &views::View::HasFocus, false),
      // Pressing Down arrow on the active browser window routes through
      // MenuController to focus the search field inside the non-activating
      // MenuHost popup.
      SendKeyPress(kBrowserViewElementId, ui::VKEY_DOWN, ui::EF_NONE),
      CheckViewProperty(AppMenuSearchBarView::kSearchFieldElementId,
                        &views::View::HasFocus, true),
      // Typing characters on the active browser window routes through the
      // shared ui::InputMethod into the focused menu-hosted search field and
      // filters the menu.
      SendKeyPress(kBrowserViewElementId, ui::VKEY_D, ui::EF_NONE),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_O, ui::EF_NONE),
      CheckViewProperty(AppMenuSearchBarView::kSearchFieldElementId,
                        &views::Textfield::GetText, u"do"),
      CheckViewProperty(AppMenuSearchBarView::kSearchFieldElementId,
                        &views::View::HasFocus, true),
      WaitForHide(AppMenuModel::kMoreToolsMenuItem),
      // The leading icon stays the search icon while results are shown.
      CheckViewProperty(AppMenuSearchBarView::kSearchIconElementId,
                        &views::View::GetVisible, true),
      // Pressing Escape clears the search query and restores the normal menu
      // without closing it.
      SendKeyPress(kBrowserViewElementId, ui::VKEY_ESCAPE, ui::EF_NONE),
      CheckViewProperty(AppMenuSearchBarView::kSearchFieldElementId,
                        &views::Textfield::GetText, u""),
      CheckViewProperty(AppMenuSearchBarView::kSearchFieldElementId,
                        &views::View::HasFocus, true),
      WaitForShow(AppMenuModel::kMoreToolsMenuItem),
      CheckViewProperty(AppMenuSearchBarView::kSearchIconElementId,
                        &views::View::GetVisible, true));
}

// Classic app menu; `AppMenuOtherDeviceTabsGlowUpInteractiveTest` covers
// kAppMenuGlowUp.
class AppMenuOtherDeviceTabsInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuOtherDeviceTabsInteractiveTest()
      : AppMenuOtherDeviceTabsInteractiveTest(/*glow_up=*/false) {}

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    sync_sessions::SessionSyncService* session_sync_service =
        SessionSyncServiceFactory::GetForProfile(browser()->GetProfile());
    syncer::DataTypeActivationRequest activation_request;
    activation_request.cache_guid = "test_cache_guid";
    activation_request.error_handler = base::DoNothing();
    base::test::TestFuture<std::unique_ptr<syncer::DataTypeActivationResponse>>
        activation_response;
    session_sync_service->GetControllerDelegate()->OnSyncStarting(
        activation_request, activation_response.GetCallback());
    sync_processor_ = std::move(activation_response.Take()->type_processor);
    sync_processor_->ConnectSync(
        std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());
  }

  // Syncs the first `num_devices` of the devices below, most recent first.
  // The first one always overflows; with 4 devices, the others are just above,
  // at and below the cap of 3.
  void SyncDevices(int num_devices) {
    struct Device {
      std::string_view name;
      syncer::DeviceInfo::FormFactor form_factor;
      int num_tabs;
    };
    static constexpr auto kDevices = std::to_array<Device>({
        {"Laptop", syncer::DeviceInfo::FormFactor::kDesktop, kNumLaptopTabs},
        {"Phone", syncer::DeviceInfo::FormFactor::kPhone, 4},
        {"Tablet", syncer::DeviceInfo::FormFactor::kTablet, 3},
        {"Desktop", syncer::DeviceInfo::FormFactor::kDesktop, 1},
    });
    const base::Time now = base::Time::Now();
    RecentTabsBuilderTestHelper helper;
    for (int d = 0; d < num_devices; ++d) {
      helper.AddSession();
      // A fixed name keeps screenshots stable.
      helper.SetSessionName(d, std::string(kDevices[d].name));
      helper.SetSessionFormFactor(d, kDevices[d].form_factor);
      helper.AddWindow(d);
      for (int i = 0; i < kDevices[d].num_tabs; ++i) {
        helper.AddTabWithInfo(d, 0, now - base::Minutes(100 * d + i),
                              u"Tab " + base::NumberToString16(i));
      }
    }
    helper.ExportToSessionSync(sync_processor_.get());
  }

  // Syncs Laptop (12 tabs) followed by an older Tablet with 0 open tabs.
  void SyncLaptopAndEmptyTablet() {
    SyncDevices(1);
    const base::Time tablet_time = base::Time::Now() - base::Minutes(100);
    sync_pb::SessionSpecifics specifics;
    specifics.set_session_tag("tablet_session_tag");
    sync_pb::SessionHeader* header = specifics.mutable_header();
    header->set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
    header->set_device_form_factor(
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET);
    header->set_client_name("Tablet");

    syncer::EntityData entity;
    *entity.specifics.mutable_session() = specifics;
    entity.creation_time = tablet_time;
    entity.modification_time = tablet_time;
    entity.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::SESSIONS, sync_sessions::SessionStore::GetClientTag(specifics));
    entity.id = entity.client_tag_hash.value();

    syncer::UpdateResponseData update;
    update.entity = std::move(entity);
    update.response_version = 1;
    syncer::UpdateResponseDataList updates;
    updates.push_back(std::move(update));

    sync_pb::DataTypeState data_type_state;
    data_type_state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    sync_processor_->OnUpdateReceived(data_type_state, std::move(updates),
                                      /*gc_directive=*/std::nullopt);
    base::RunLoop().RunUntilIdle();
  }

  // Opens the submenu, where the laptop shows `max_inline_tabs` of its tabs
  // inline above its flyout row.
  auto ShowSubmenu(int max_inline_tabs) {
    return Steps(
        // Keeps a cursor left over by an earlier test from hovering the menus.
        MoveMouseTo(kToolbarAppMenuButtonElementId),
        PressButton(kToolbarAppMenuButtonElementId),
        SelectMenuItem(AppMenuModel::kOtherDeviceTabsMenuItem, kMouse),
        NameMenuItemWithTitle(kFlyoutRow,
                              l10n_util::GetPluralStringFUTF16(
                                  IDS_OTHER_DEVICE_TABS_SEE_MORE_TABS,
                                  kNumLaptopTabs - max_inline_tabs)));
  }

  // Like ShowSubmenu(), then takes a screenshot of the submenu.
  auto ShowSubmenuAndScreenshot(int max_inline_tabs,
                                std::string_view baseline_cl = kBaselineCl) {
    return Steps(
        ShowSubmenu(max_inline_tabs),
        SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                kSkipPixelTestsReason),
        ScreenshotSurface(kFlyoutRow, "other_device_tabs_menu",
                          std::string(baseline_cl)));
  }

  // Opens the submenu for Laptop (5 inline tabs + flyout) + empty Tablet,
  // screenshots the submenu, and verifies the empty-state title row.
  auto ShowSubmenuWithEmptyDeviceAndScreenshot() {
    return Steps(
        ShowSubmenuAndScreenshot(/*max_inline_tabs=*/5, "8796148"),
        NameMenuItemWithTitle(
            kEmptyStateRow, l10n_util::GetStringFUTF16(
                                IDS_OTHER_DEVICE_TABS_NO_OPEN_TABS, u"Tablet")),
        CheckView(kEmptyStateRow, [](views::MenuItemView* item) {
          if (item->GetType() != views::MenuItemView::Type::kTitle ||
              item->GetEnabled()) {
            return false;
          }
          const std::u16string flyout_title = l10n_util::GetPluralStringFUTF16(
              IDS_OTHER_DEVICE_TABS_SEE_MORE_TABS, kNumLaptopTabs - 5);
          for (views::View* child : item->parent()->children()) {
            if (auto* menu_item =
                    views::AsViewClass<views::MenuItemView>(child)) {
              if (menu_item->title() == flyout_title) {
                return true;
              }
              if (menu_item == item) {
                return false;
              }
            }
          }
          return false;
        }));
  }

  // From the open submenu, opens the last tab of the laptop's flyout after
  // taking a screenshot of the flyout.
  auto OpenFlyoutTab() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kRestoredTabId);
    const std::vector<RecentTabItem> items =
        RecentTabsBuilder::BuildOtherDeviceTabs(browser()->GetProfile());
    const auto it = std::ranges::find(items, RecentTabItem::Type::kMoreTabs,
                                      &RecentTabItem::type);
    CHECK(it != items.end());
    const std::string expected_query(it->children().back().url().query());
    return Steps(
        SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                kSkipPixelTestsReason),
        SelectMenuItem(kFlyoutRow, kMouse),
        NameMenuItemWithTitle(kFlyoutTab, kLastTab),
        ScreenshotSurface(kFlyoutTab, "more_tabs_flyout", kBaselineCl),
        InstrumentNextTab(kRestoredTabId), SelectMenuItem(kFlyoutTab),
        WaitForShow(kRestoredTabId),
        CheckResult([this]() { return browser()->tab_strip_model()->count(); },
                    2),
        // Compares the query, which identifies the tab, as the scheme may be
        // upgraded to HTTPS by now.
        CheckResult(
            [this]() {
              return std::string(browser()
                                     ->tab_strip_model()
                                     ->GetActiveWebContents()
                                     ->GetVisibleURL()
                                     .query());
            },
            expected_query));
  }

 protected:
  static constexpr int kNumLaptopTabs = 12;
  static constexpr char kEmptyStateRow[] = "empty_state_row";
  static constexpr char kFlyoutRow[] = "flyout_row";
  static constexpr char kFlyoutTab[] = "flyout_tab";
  static constexpr char16_t kLastTab[] = u"Tab 11";
  static constexpr char kSkipPixelTestsReason[] =
      "Should only run in pixel_tests.";
  static constexpr char kBaselineCl[] = "8712703";
  // Unlike keyboard input, opens submenus without later selecting their first
  // item, which keeps screenshots stable.
  static constexpr auto kMouse =
      ui::test::InteractionTestUtil::InputType::kMouse;

  explicit AppMenuOtherDeviceTabsInteractiveTest(bool glow_up) {
    feature_list_.InitWithFeatureStates(
        {{syncer::kCrossDeviceTabsInAppMenu, true},
         {features::kAppMenuGlowUp, glow_up}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<syncer::DataTypeProcessor> sync_processor_;
};

class AppMenuOtherDeviceTabsGlowUpInteractiveTest
    : public AppMenuOtherDeviceTabsInteractiveTest {
 public:
  AppMenuOtherDeviceTabsGlowUpInteractiveTest()
      : AppMenuOtherDeviceTabsInteractiveTest(/*glow_up=*/true) {}
};

IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsInteractiveTest, OpenFlyoutTab) {
  SyncDevices(1);
  RunTestSequence(ShowSubmenuAndScreenshot(/*max_inline_tabs=*/10),
                  OpenFlyoutTab());
}

IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsInteractiveTest,
                       ShowSubmenuWithTwoDevices) {
  SyncDevices(2);
  RunTestSequence(ShowSubmenuAndScreenshot(/*max_inline_tabs=*/5));
}

IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsInteractiveTest,
                       ShowSubmenuWithFourDevices) {
  SyncDevices(4);
  RunTestSequence(ShowSubmenuAndScreenshot(/*max_inline_tabs=*/3));
}

IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsInteractiveTest,
                       ShowSubmenuWithEmptyDevice) {
  SyncLaptopAndEmptyTablet();
  RunTestSequence(ShowSubmenuWithEmptyDeviceAndScreenshot());
}

// Like inline tabs, a Ctrl/Cmd-clicked flyout tab opens without closing the
// menu. Replays what the menu does on such a click, as synthetic modifier
// clicks are unreliable on Mac and lose the modifier on ChromeOS.
IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsInteractiveTest,
                       CtrlClickFlyoutTabKeepsMenuOpen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kRestoredTabId);
#if BUILDFLAG(IS_MAC)
  constexpr int kFlags = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN;
#else
  constexpr int kFlags = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN;
#endif
  const ui::MouseEvent click(ui::EventType::kMouseReleased, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(), kFlags,
                             ui::EF_LEFT_MOUSE_BUTTON);
  SyncDevices(1);
  RunTestSequence(
      ShowSubmenu(/*max_inline_tabs=*/10), SelectMenuItem(kFlyoutRow),
      NameMenuItemWithTitle(kFlyoutTab, kLastTab),
      InstrumentNextTab(kRestoredTabId),
      CheckView(
          kFlyoutTab,
          [&click](views::MenuItemView* item) {
            return item->GetDelegate()->ShouldExecuteCommandWithoutClosingMenu(
                item->GetCommand(), click);
          }),
      WithView(kFlyoutTab,
               [](views::MenuItemView* item) {
                 item->GetDelegate()->ExecuteCommand(item->GetCommand(),
                                                     kFlags);
               }),
      WaitForShow(kRestoredTabId), EnsurePresent(kFlyoutTab));
}

IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsGlowUpInteractiveTest,
                       OpenFlyoutTab) {
  SyncDevices(1);
  RunTestSequence(ShowSubmenuAndScreenshot(/*max_inline_tabs=*/10),
                  OpenFlyoutTab());
}

IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsGlowUpInteractiveTest,
                       ShowSubmenuWithTwoDevices) {
  SyncDevices(2);
  RunTestSequence(ShowSubmenuAndScreenshot(/*max_inline_tabs=*/5));
}

IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsGlowUpInteractiveTest,
                       ShowSubmenuWithFourDevices) {
  SyncDevices(4);
  RunTestSequence(ShowSubmenuAndScreenshot(/*max_inline_tabs=*/3));
}

IN_PROC_BROWSER_TEST_F(AppMenuOtherDeviceTabsGlowUpInteractiveTest,
                       ShowSubmenuWithEmptyDevice) {
  SyncLaptopAndEmptyTablet();
  RunTestSequence(ShowSubmenuWithEmptyDeviceAndScreenshot());
}
