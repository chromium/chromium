// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_element_identifiers.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resources/preinstalled_web_apps/internal/container.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

// Aliases.
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointer;
using ::testing::Property;

// Elements --------------------------------------------------------------------

// Identifiers.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kContainerAppWebContentsElementId);

// Names.
inline char kAppsGridViewElementName[] = "AppsGridView";
inline char kAppListBubbleAppsPageElementName[] = "AppListBubbleAppsPage";
inline char kChromeAppElementName[] = "ChromeApp";
inline char kContainerAppElementName[] = "ContainerApp";
inline char kFilesAppElementName[] = "FilesApp";
inline char kGmailAppElementName[] = "GmailApp";

// Helpers ---------------------------------------------------------------------

// Returns all `descendants` of the specified `parent` matching the given class.
template <typename ViewClass>
void FindDescendantsOfClass(
    const views::View* parent,
    std::vector<raw_ptr<const ViewClass>>& descendants) {
  for (const views::View* child : parent->children()) {
    if (views::IsViewClass<ViewClass>(child)) {
      descendants.emplace_back(views::AsViewClass<ViewClass>(child));
    }
    FindDescendantsOfClass(child, descendants);
  }
}

// Returns the index of `value` in the specified `range`.
template <typename Range, typename Value>
std::optional<size_t> FindIndex(const Range& range, const Value* value) {
  auto it = base::ranges::find(range, value);
  return it != range.end()
             ? std::make_optional<size_t>(std::distance(range.begin(), it))
             : std::make_optional<size_t>();
}

// Returns the `views::MenuItemView`s for the currently showing menu.
std::vector<raw_ptr<const views::MenuItemView>> FindMenuItemViews() {
  if (auto* menu_controller = views::MenuController::GetActiveInstance()) {
    if (auto* menu_item_view = menu_controller->GetSelectedMenuItem()) {
      std::vector<raw_ptr<const views::MenuItemView>> items;
      FindDescendantsOfClass(menu_item_view->parent(), items);
      return items;
    }
  }
  return {};
}

// Returns the `ash::ShelfItem` for the given web app `id`.
const ash::ShelfItem* FindShelfItemForWebApp(std::string_view id) {
  const ash::ShelfItems& items = ash::ShelfModel::Get()->items();
  auto it = base::ranges::find_if(
      items, [id](const ash::ShelfItem& item) { return item.id.app_id == id; });
  return it != items.end() ? &*it : nullptr;
}

// Returns if `view` is the `ash::AppListItemView` for the given web app `id`.
bool IsAppListItemViewForWebApp(std::string_view id, const views::View* view) {
  return views::IsViewClass<ash::AppListItemView>(view) &&
         views::AsViewClass<ash::AppListItemView>(view)->item()->id() == id;
}

// Returns if `browser` is the `Browser` for the given web app `id`.
bool IsBrowserForWebApp(const webapps::AppId& id, const Browser* browser) {
  return web_app::AppBrowserController::IsForWebApp(browser, id);
}

// Returns if the menu is currently showing.
bool IsMenuShowing() {
  auto* menu_controller = views::MenuController::GetActiveInstance();
  return menu_controller && menu_controller->GetSelectedMenuItem();
}

// Returns if `view` is the `ash::ShelfAppButton` for the given web app `id`.
bool IsShelfAppButtonForWebApp(
    std::reference_wrapper<const raw_ptr<ash::ShelfView>> shelf,
    std::string_view id,
    const views::View* view) {
  return views::IsViewClass<ash::ShelfAppButton>(view) &&
         shelf.get()->GetShelfAppButton(FindShelfItemForWebApp(id)->id) == view;
}

}  // namespace

// ContainerAppInteractiveUiTest -----------------------------------------------

// Base class for interactive UI tests of the container app.
class ContainerAppInteractiveUiTest : public InteractiveBrowserTest {
 public:
  ContainerAppInteractiveUiTest() {
    // Enable container app preinstallation.
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kContainerAppPreinstall,
         chromeos::features::kFeatureManagementContainerAppPreinstall},
        {});

    // Do not launch the browser. The only browser expected to launch will be
    // that associated with the container app.
    set_launch_browser_for_testing(nullptr);

    // Use a consistent context for element tracking. Otherwise each widget has
    // its own context, greatly increasing the complexity of tracking
    // cross-widget CUJs as is the case in this test suite.
    views::ElementTrackerViews::SetContextOverrideCallback(
        base::BindRepeating([](views::Widget* widget) {
          return ui::ElementContext(ash::Shell::GetPrimaryRootWindow());
        }));
  }

  // Returns a builder for a step which assigns the view associated with the
  // given `element_specifier` to the given `ptr`.
  template <typename ViewClass>
  [[nodiscard]] static auto AssignView(
      ElementSpecifier element_specifier,
      std::reference_wrapper<raw_ptr<ViewClass>> ptr) {
    return WithView(element_specifier,
                    [ptr](ViewClass* view) { ptr.get() = view; });
  }

  // Returns the expected launch URL for the container app.
  GURL GetContainerAppLaunchUrl() const {
    GURL::Replacements components;
    components.SetQueryStr(*container_app_install_info_->launch_query_params);
    return container_app_install_info_->start_url.ReplaceComponents(components);
  }

  // Returns the expected title for the container app.
  const std::u16string& GetContainerAppTitle() const {
    return container_app_install_info_->title;
  }

  // Returns a builder for a step which presses and releases the given `key`.
  [[nodiscard]] static auto PressAndReleaseKey(ui::KeyboardCode key) {
    return Do([key]() {
      ui::test::EventGenerator(ash::Shell::GetPrimaryRootWindow())
          .PressAndReleaseKeyAndModifierKeys(key, ui::EF_NONE);
    });
  }

  // Returns a builder for a step which resets the specified `ptr`.
  template <typename T>
  [[nodiscard]] static auto Reset(std::reference_wrapper<raw_ptr<T>> ptr) {
    return Do([ptr]() { ptr.get() = nullptr; });
  }

 private:
  // InteractiveBrowserTest:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpDefaultCommandLine(command_line);

    // Remove the `switches::kDisableDefaultApps` switch to ensure that default
    // apps are installed. The container app is a default app.
    command_line->RemoveSwitch(switches::kDisableDefaultApps);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    // Wait for installation of both system and external web apps. The container
    // app is an external app and this test suite will verify its adjacency to
    // system web apps.
    Profile* const profile = ProfileManager::GetActiveUserProfile();
    ash::SystemWebAppManager::GetForTest(profile)
        ->InstallSystemAppsForTesting();
    web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
        web_app::WebAppProvider::GetForTest(profile));
    AppListClientImpl::GetInstance()->UpdateProfile();

    // `InteractiveBrowserTest`s must set a context widget prior to invoking
    // `RunTestSequence()`. This needs to be done explicitly since we suppressed
    // launching the browser. Note that it doesn't matter what this widget is,
    // but convention is to use the `ash::StatusAreaWidget`.
    SetContextWidget(ash::Shell::GetPrimaryRootWindowController()
                         ->shelf()
                         ->GetStatusAreaWidget());

    // Cache install info for the container app.
    container_app_install_info_ =
        web_app::GetConfigForContainer().app_info_factory.Run();
  }

  // Used to ignore the container app preinstallation key.
  const base::AutoReset<bool> ignore_container_app_preinstall_key_{
      chromeos::features::SetIgnoreContainerAppPreinstallKeyForTesting()};

  // Used to enable the container app preinstallation.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Used to retrieve expected title/URL for the container app.
  std::unique_ptr<web_app::WebAppInstallInfo> container_app_install_info_;
};

// Tests -----------------------------------------------------------------------

// Verifies that the container app can be launched from the app list.
IN_PROC_BROWSER_TEST_F(ContainerAppInteractiveUiTest, LaunchFromAppList) {
  // Views.
  raw_ptr<ash::AppsGridView> apps_grid_view = nullptr;
  raw_ptr<ash::AppListItemView> container_app = nullptr;
  raw_ptr<ash::AppListItemView> files_app = nullptr;
  raw_ptr<ash::AppListItemView> gmail_app = nullptr;

  // Test.
  RunTestSequence(
      // Launch app list.
      DoDefaultAction(ash::kHomeButtonElementId),

      // Find apps page.
      NameDescendantViewByType<ash::AppListBubbleAppsPage>(
          ash::kAppListBubbleViewElementId, kAppListBubbleAppsPageElementName),

      // Find apps grid.
      NameDescendantViewByType<ash::AppsGridView>(
          kAppListBubbleAppsPageElementName, kAppsGridViewElementName),

      // Cache apps grid.
      AssignView(kAppsGridViewElementName, std::ref(apps_grid_view)),

      // Find container app.
      NameDescendantView(kAppsGridViewElementName, kContainerAppElementName,
                         base::BindRepeating(&IsAppListItemViewForWebApp,
                                             web_app::kContainerAppId)),

      // Cache container app.
      AssignView(kContainerAppElementName, std::ref(container_app)),

      // Find Files app.
      NameDescendantView(
          kAppsGridViewElementName, kFilesAppElementName,
          base::BindRepeating(&IsAppListItemViewForWebApp,
                              file_manager::kFileManagerSwaAppId)),

      // Cache Files app.
      AssignView(kFilesAppElementName, std::ref(files_app)),

      // Find Gmail app.
      NameDescendantView(kAppsGridViewElementName, kGmailAppElementName,
                         base::BindRepeating(&IsAppListItemViewForWebApp,
                                             web_app::kGmailAppId)),

      // Cache Gmail app.
      AssignView(kGmailAppElementName, std::ref(gmail_app)),

      // Check container app title.
      CheckView(kContainerAppElementName,
                base::BindOnce(&ash::AppListItemView::title),
                Property(&views::Label::GetText, Eq(GetContainerAppTitle()))),

      // Check container app position.
      Check([&]() {
        std::vector<raw_ptr<const ash::AppListItemView>> apps;
        FindDescendantsOfClass(apps_grid_view, apps);
        const auto container_app_index = FindIndex(apps, container_app.get());
        const auto files_app_index = FindIndex(apps, files_app.get());
        const auto gmail_app_index = FindIndex(apps, gmail_app.get());
        return (files_app_index == container_app_index.value() - 1u) &&
               (gmail_app_index == container_app_index.value() + 1u);
      }),

      // Reset cached pointers which might dangle when the app list closes.
      Reset(std::ref(apps_grid_view)), Reset(std::ref(container_app)),
      Reset(std::ref(files_app)), Reset(std::ref(gmail_app)),

      // Launch container app.
      InstrumentNextTab(kContainerAppWebContentsElementId, AnyBrowser()),
      DoDefaultAction(kContainerAppElementName),

      // Check container app browser.
      CheckElement(kContainerAppWebContentsElementId,
                   base::BindOnce(&AsInstrumentedWebContents)
                       .Then(base::BindOnce(
                           &WebContentsInteractionTestUtil::web_contents))
                       .Then(base::BindOnce(&chrome::FindBrowserWithTab))
                       .Then(base::BindOnce(&IsBrowserForWebApp,
                                            web_app::kContainerAppId))),

      // Check container app launch URL.
      WaitForWebContentsReady(kContainerAppWebContentsElementId,
                              GetContainerAppLaunchUrl()));
}

// Verifies that the container app can be launched from the shelf.
IN_PROC_BROWSER_TEST_F(ContainerAppInteractiveUiTest, LaunchFromShelf) {
  // Views.
  raw_ptr<ash::ShelfAppButton> chrome_app = nullptr;
  raw_ptr<ash::ShelfAppButton> container_app = nullptr;
  raw_ptr<ash::ShelfAppButton> gmail_app = nullptr;
  raw_ptr<ash::ShelfView> shelf = nullptr;

  // Test.
  RunTestSequence(
      // Cache shelf.
      AssignView(ash::kShelfViewElementId, std::ref(shelf)),

      // Find container app.
      NameDescendantView(
          ash::kShelfViewElementId, kContainerAppElementName,
          base::BindRepeating(&IsShelfAppButtonForWebApp, std::cref(shelf),
                              web_app::kContainerAppId)),

      // Cache container app.
      AssignView(kContainerAppElementName, std::ref(container_app)),

      // Find Chrome app.
      NameDescendantView(
          ash::kShelfViewElementId, kChromeAppElementName,
          base::BindRepeating(&IsShelfAppButtonForWebApp, std::cref(shelf),
                              app_constants::kChromeAppId)),

      // Cache Chrome app.
      AssignView(kChromeAppElementName, std::ref(chrome_app)),

      // Find Gmail app.
      NameDescendantView(
          ash::kShelfViewElementId, kGmailAppElementName,
          base::BindRepeating(&IsShelfAppButtonForWebApp, std::cref(shelf),
                              web_app::kGmailAppId)),

      // Cache Gmail app.
      AssignView(kGmailAppElementName, std::ref(gmail_app)),

      // Check container app position.
      Check([&]() {
        std::vector<raw_ptr<const ash::ShelfAppButton>> apps;
        FindDescendantsOfClass(shelf, apps);
        const auto chrome_app_index = FindIndex(apps, chrome_app.get());
        const auto container_app_index = FindIndex(apps, container_app.get());
        const auto gmail_app_index = FindIndex(apps, gmail_app.get());
        return (chrome_app_index == container_app_index.value() - 1u) &&
               (gmail_app_index == container_app_index.value() + 1u);
      }),

      // Launch container app.
      InstrumentNextTab(kContainerAppWebContentsElementId, AnyBrowser()),
      DoDefaultAction(kContainerAppElementName),

      // Check container app browser.
      CheckElement(kContainerAppWebContentsElementId,
                   base::BindOnce(&AsInstrumentedWebContents)
                       .Then(base::BindOnce(
                           &WebContentsInteractionTestUtil::web_contents))
                       .Then(base::BindOnce(&chrome::FindBrowserWithTab))
                       .Then(base::BindOnce(&IsBrowserForWebApp,
                                            web_app::kContainerAppId))),

      // Check container app launch URL.
      WaitForWebContentsReady(kContainerAppWebContentsElementId,
                              GetContainerAppLaunchUrl()));
}

// Verifies that the container app cannot be uninstalled from the app list.
IN_PROC_BROWSER_TEST_F(ContainerAppInteractiveUiTest, UninstallFromAppList) {
  RunTestSequence(
      // Launch app list.
      DoDefaultAction(ash::kHomeButtonElementId),

      // Find apps page.
      NameDescendantViewByType<ash::AppListBubbleAppsPage>(
          ash::kAppListBubbleViewElementId, kAppListBubbleAppsPageElementName),

      // Find apps grid.
      NameDescendantViewByType<ash::AppsGridView>(
          kAppListBubbleAppsPageElementName, kAppsGridViewElementName),

      // Find container app.
      NameDescendantView(kAppsGridViewElementName, kContainerAppElementName,
                         base::BindRepeating(&IsAppListItemViewForWebApp,
                                             web_app::kContainerAppId)),

      // Open menu.
      MoveMouseTo(kContainerAppElementName), ClickMouse(ui_controls::RIGHT),
      Check(&IsMenuShowing),

      // Activate menu.
      PressAndReleaseKey(ui::VKEY_DOWN),

      // Check container app cannot be uninstalled.
      CheckResult(
          &FindMenuItemViews,
          AllOf(Not(IsEmpty()),
                Not(Contains(Pointer(Property(&views::MenuItemView::GetCommand,
                                              Eq(ash::UNINSTALL))))))));
}

// Verifies that the container app cannot be uninstalled from the shelf.
IN_PROC_BROWSER_TEST_F(ContainerAppInteractiveUiTest, UninstallFromShelf) {
  // Views.
  raw_ptr<ash::ShelfView> shelf = nullptr;

  // Test.
  RunTestSequence(
      // Cache shelf.
      AssignView(ash::kShelfViewElementId, std::ref(shelf)),

      // Find container app.
      NameDescendantView(
          ash::kShelfViewElementId, kContainerAppElementName,
          base::BindRepeating(&IsShelfAppButtonForWebApp, std::cref(shelf),
                              web_app::kContainerAppId)),

      // Open menu.
      MoveMouseTo(kContainerAppElementName), ClickMouse(ui_controls::RIGHT),
      Check(&IsMenuShowing),

      // Activate menu.
      PressAndReleaseKey(ui::VKEY_DOWN),

      // Check container app cannot be uninstalled.
      CheckResult(
          &FindMenuItemViews,
          AllOf(Not(IsEmpty()),
                Not(Contains(Pointer(Property(&views::MenuItemView::GetCommand,
                                              Eq(ash::UNINSTALL))))))));
}

// TODO(http://b/331668699): Test container app position for existing users.
// TODO(http://b/331668699): Test container app preinstall ineligibility.
// TODO(http://b/331668699): Test container app uninstallability from Settings.
