// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/echo/echo_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/container.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/session_manager/session_manager_types.h"
#include "components/sync/base/command_line_switches.h"
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
using ::testing::Bool;
using ::testing::Conditional;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointer;
using ::testing::PrintToStringParamName;
using ::testing::Property;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

// Elements --------------------------------------------------------------------

// Identifiers.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserWebContentsElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kContainerAppWebContentsElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsAppWebContentsElementId);

// Names.
inline char kAppsGridViewElementName[] = "AppsGridView";
inline char kAppListBubbleAppsPageElementName[] = "AppListBubbleAppsPage";
inline char kChromeAppElementName[] = "ChromeApp";
inline char kContainerAppElementName[] = "ContainerApp";
inline char kFilesAppElementName[] = "FilesApp";
inline char kGmailAppElementName[] = "GmailApp";
inline char kShowAppInfoMenuItemElementName[] = "ShowAppInfoMenuItem";

// Returns all `descendants` of the specified `parent` matching the given class.
template <typename ViewClass>
void FindDescendantsOfClass(views::View* parent,
                            std::vector<raw_ptr<ViewClass>>& descendants) {
  for (views::View* child : parent->children()) {
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
std::vector<raw_ptr<views::MenuItemView>> FindMenuItemViews() {
  if (auto* menu_controller = views::MenuController::GetActiveInstance()) {
    if (auto* menu_item_view = menu_controller->GetSelectedMenuItem()) {
      std::vector<raw_ptr<views::MenuItemView>> items;
      FindDescendantsOfClass(menu_item_view->parent(), items);
      return items;
    }
  }
  return {};
}

// Returns the `views::MenuItemView` for the currently showing menu associated
// with the specified command `id`.
views::MenuItemView* FindMenuItemViewForCommand(int id) {
  std::vector<raw_ptr<views::MenuItemView>> views = FindMenuItemViews();
  auto it = base::ranges::find(views, id, &views::MenuItemView::GetCommand);
  return it != views.end() ? *it : nullptr;
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
  const ash::ShelfItem* const item = FindShelfItemForWebApp(id);
  return views::IsViewClass<ash::ShelfAppButton>(view) && item &&
         shelf.get()->GetShelfAppButton(item->id) == view;
}

// Waiters ---------------------------------------------------------------------

// Class which waits for `BrowserListObserver::OnBrowserSetLastActive()` events.
class OnBrowserSetLastActiveWaiter : public BrowserListObserver {
 public:
  void Wait() {
    CHECK(!run_loop_);

    base::ScopedObservation<BrowserList, BrowserListObserver> observer(this);
    observer.Observe(BrowserList::GetInstance());

    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);

    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override {
    CHECK(run_loop_);
    run_loop_->Quit();
  }

  // Used to wait for `OnBrowserSetLastActive()` events.
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

// ContainerAppInteractiveUiTestBase -------------------------------------------

// Base class for interactive UI tests of the container app.
class ContainerAppInteractiveUiTestBase
    : public InteractiveBrowserTestT<MixinBasedInProcessBrowserTest> {
 public:
  ContainerAppInteractiveUiTestBase(
      std::optional<ash::LoggedInUserMixin::LogInType> login_type)
      : user_session_mixin_(CreateUserSessionMixin(login_type)) {
    // Enable container app preinstallation.
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kContainerAppPreinstall,
         chromeos::features::kFeatureManagementContainerAppPreinstall},
        {});

    // Use a consistent context for element tracking. Otherwise each widget has
    // its own context, greatly increasing the complexity of tracking
    // cross-widget CUJs as is the case in this test suite.
    views::ElementTrackerViews::SetContextOverrideCallback(
        base::BindRepeating([](views::Widget* widget) {
          return ui::ElementContext(ash::Shell::GetPrimaryRootWindow());
        }));
  }

  // Returns a builder for a step which assigns the last active browser to the
  // specified `ptr_ref`.
  [[nodiscard]] auto AssignLastActiveBrowser(
      std::reference_wrapper<Browser*> ptr_ref) {
    return Do([ptr_ref]() {
      ptr_ref.get() = BrowserList::GetInstance()->GetLastActive();
    });
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
    return container_app_install_info_->start_url().ReplaceComponents(
        components);
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

  // Returns a builder for a step which waits for a
  // `BrowserList::OnBrowserSetLastActive()` event.
  [[nodiscard]] auto WaitForOnBrowserSetLastActive() {
    return Do([]() { OnBrowserSetLastActiveWaiter().Wait(); });
  }

 protected:
  // InteractiveBrowserTestT<MixinBasedInProcessBrowserTest>:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTestT<
        MixinBasedInProcessBrowserTest>::SetUpDefaultCommandLine(command_line);

    // Remove the `switches::kDisableDefaultApps` switch to ensure that default
    // apps are installed. The container app is a default app.
    command_line->RemoveSwitch(switches::kDisableDefaultApps);

    // Disable sync as it would otherwise block updating of shelf pins.
    command_line->AppendSwitch(syncer::kDisableSync);
  }

  void SetUpOnMainThread() override {
    // There's nothing to do if not logging in the user.
    if (!ShouldLogInUser()) {
      InteractiveBrowserTestT<
          MixinBasedInProcessBrowserTest>::SetUpOnMainThread();
      return;
    }

    // For logged-in user sessions, perform login prior to
    // `InteractiveBrowserTestT<>::SetUpOnMainThread()` so that the interactive
    // browser test base class will successfully set the context widget for the
    // test sequence. The context widget will be associated with the browser.
    if (absl::holds_alternative<ash::LoggedInUserMixin>(user_session_mixin_)) {
      absl::get<ash::LoggedInUserMixin>(user_session_mixin_).LogInUser();
    }

    InteractiveBrowserTestT<
        MixinBasedInProcessBrowserTest>::SetUpOnMainThread();

    // Wait for installation of both system and external web apps. The container
    // app is an external app and this test suite will verify its adjacency to
    // system web apps.
    Profile* const profile = browser()->profile();
    ash::SystemWebAppManager::GetForTest(profile)
        ->InstallSystemAppsForTesting();
    web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
        web_app::WebAppProvider::GetForTest(profile));
    AppListClientImpl::GetInstance()->UpdateProfile();

    // Fetch `device_info` from echo.
    base::test::TestFuture<std::optional<base::Time>> oobe_timestamp;
    chromeos::echo_util::GetOobeTimestamp(oobe_timestamp.GetCallback());
    ASSERT_TRUE(oobe_timestamp.Wait());
    ASSERT_TRUE(oobe_timestamp.Get().has_value());
    web_app::DeviceInfo device_info;
    device_info.oobe_timestamp = oobe_timestamp.Get().value();

    // Cache install info for the container app.
    container_app_install_info_ =
        web_app::GetConfigForContainer(device_info).app_info_factory.Run();
  }

 private:
  // Creates the appropriate guest or logged-in user session mixin based on
  // the presence of `login_type`.
  absl::variant<ash::GuestSessionMixin, ash::LoggedInUserMixin>
  CreateUserSessionMixin(
      std::optional<ash::LoggedInUserMixin::LogInType> login_type) {
    if (!login_type) {
      return absl::variant<ash::GuestSessionMixin, ash::LoggedInUserMixin>(
          absl::in_place_type_t<ash::GuestSessionMixin>(), &mixin_host_);
    }

    return absl::variant<ash::GuestSessionMixin, ash::LoggedInUserMixin>(
        absl::in_place_type_t<ash::LoggedInUserMixin>(), &mixin_host_,
        /*test_base=*/this, embedded_test_server(), login_type.value());
  }

  // Returns whether the user should be logged in as part of test setup.
  virtual bool ShouldLogInUser() const { return true; }

  // Used to manage either a guest or logged-in user session based on test
  // parameterization.
  absl::variant<ash::GuestSessionMixin, ash::LoggedInUserMixin>
      user_session_mixin_;

  // Used to enable the container app preinstallation.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Used to retrieve expected title/URL for the container app.
  std::unique_ptr<web_app::WebAppInstallInfo> container_app_install_info_;

  // Used to conditionally ignore the container app preinstallation debug key.
  std::unique_ptr<base::AutoReset<bool>>
      ignore_container_app_preinstall_debug_key_;
};

// ContainerAppInteractiveUiTest -----------------------------------------------

// Base class for interactive UI tests of the container app, parameterized by
// whether the logged-in user is new or existing. Tests include a PRE_ session,
// where user state is initialized, followed by a subsequent session containing
// test logic. Chrome is restarted between sessions.
class ContainerAppInteractiveUiTest
    : public ContainerAppInteractiveUiTestBase,
      public WithParamInterface</*existing_user=*/bool> {
 public:
  ContainerAppInteractiveUiTest()
      : ContainerAppInteractiveUiTestBase(
            ash::LoggedInUserMixin::LogInType::kConsumer) {
    // Disable the container app during the PRE_ session so that the subsequent
    // session containing test logic is when the app preinstallation occurs.
    if (IsPreSession()) {
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::features::kContainerAppPreinstall);
    }
  }

 protected:
  // ContainerAppInteractiveUiTestBase:
  void SetUpOnMainThread() override {
    ContainerAppInteractiveUiTestBase::SetUpOnMainThread();

    // Check that session state is as expected.
    const auto* session_controller = ash::Shell::Get()->session_controller();
    EXPECT_THAT(session_controller->GetSessionState(),
                Conditional(ShouldLogInUser(),
                            Eq(session_manager::SessionState::ACTIVE),
                            Eq(session_manager::SessionState::LOGIN_PRIMARY)));

    // Check that login state is as expected.
    EXPECT_THAT(
        session_controller->IsUserFirstLogin(),
        Conditional(IsPreSession(), IsExistingUser(), Not(IsExistingUser())));
  }

  bool ShouldLogInUser() const override {
    // Existing users should be logged in for both the PRE_ session and the
    // subsequent session containing test logic. New users should only be logged
    // in for the subsequent session.
    return IsExistingUser() || !IsPreSession();
  }

  // Returns whether the logged-in user is existing given test parameterization.
  bool IsExistingUser() const { return GetParam(); }

  // Returns whether the current session is the PRE_ session. The PRE_ session
  // is the session before the subsequent session containing test logic.
  bool IsPreSession() const {
    return base::StartsWith(
        testing::UnitTest::GetInstance()->current_test_info()->name(), "PRE_");
  }

 private:
  // Used to disable container app preinstallation for the PRE_ session.
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContainerAppInteractiveUiTest,
    /*existing_user=*/Bool(),
    [](const testing::TestParamInfo</*existing_user=*/bool>& info) {
      return info.param ? "ExistingUser" : "NewUser";
    });

// Tests -----------------------------------------------------------------------

// Initializes user state and restarts Chrome before `LaunchFromAppList`.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest, PRE_LaunchFromAppList) {}

// Verifies that the container app can be launched from the app list.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest, LaunchFromAppList) {
  // Views.
  raw_ptr<ash::AppsGridView> apps_grid_view = nullptr;
  raw_ptr<ash::AppListItemView> container_app = nullptr;
  raw_ptr<ash::AppListItemView> files_app = nullptr;
  raw_ptr<ash::AppListItemView> gmail_app = nullptr;

  // Test.
  RunTestSequence(
      // Launch app list.
      DoDefaultAction(ash::kHomeButtonElementId),
      WaitForShow(ash::kAppListBubbleViewElementId),

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
        std::vector<raw_ptr<ash::AppListItemView>> apps;
        FindDescendantsOfClass(apps_grid_view, apps);
        const auto container_app_index = FindIndex(apps, container_app.get());
        if (IsExistingUser()) {
          return container_app_index == 0u;
        }
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

// Initializes user state and restarts Chrome before `LaunchFromShelf`.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest, PRE_LaunchFromShelf) {}

// Verifies that the container app can be launched from the shelf.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest, LaunchFromShelf) {
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
        std::vector<raw_ptr<ash::ShelfAppButton>> apps;
        FindDescendantsOfClass(shelf, apps);
        const auto container_app_index = FindIndex(apps, container_app.get());
        if (IsExistingUser()) {
          return container_app_index == 0u;
        }
        const auto chrome_app_index = FindIndex(apps, chrome_app.get());
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

// Initializes user state and restarts Chrome before
// `PreferredAppForSupportedLinks`.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest,
                       PRE_PreferredAppForSupportedLinks) {}

// Verifies that the container app is the preferred app for supported links.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest,
                       PreferredAppForSupportedLinks) {
  // Browser.
  Browser* container_app_browser = nullptr;

  // Test.
  RunTestSequence(
      // Navigate browser to page with supported link.
      AddInstrumentedTab(
          kBrowserWebContentsElementId,
          GURL(base::StrCat({"data:text/html;base64,",
                             base::Base64Encode(base::ReplaceStringPlaceholders(
                                 R"(<DOCTYPE html>
                                    <html>
                                      <head>
                                        <style>
                                          html, body, a {
                                            display: block;
                                            height: 100%;
                                            width: 100%;
                                          }
                                        </style>
                                      </head>
                                      <body>
                                        <a href="$1" target="_blank"></a>
                                      </body>
                                    </html>)",
                                 /*subst=*/{GetContainerAppLaunchUrl().spec()},
                                 /*offsets=*/nullptr))}))),

      // Launch container app via supported link.
      MoveMouseTo(kBrowserViewElementId), ClickMouse(),

      // Instrument container app browser.
      WaitForOnBrowserSetLastActive(),
      AssignLastActiveBrowser(std::ref(container_app_browser)),
      InstrumentTab(kContainerAppWebContentsElementId,
                    /*tab_index=*/std::nullopt,
                    std::ref(container_app_browser)),

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

// Initializes user state and restarts Chrome before `UninstallFromAppList`.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest,
                       PRE_UninstallFromAppList) {}

// Verifies that the container app cannot be uninstalled from the app list.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest, UninstallFromAppList) {
  RunTestSequence(
      // Launch app list.
      DoDefaultAction(ash::kHomeButtonElementId),
      WaitForShow(ash::kAppListBubbleViewElementId),

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

// Initializes user state and restarts Chrome before `UninstallFromSettings`.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest,
                       PRE_UninstallFromSettings) {}

// Verifies that the container app cannot be uninstalled from the Settings app.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest, UninstallFromSettings) {
  // Views.
  raw_ptr<ash::ShelfView> shelf = nullptr;

  // Queries.
  auto get_settings_app_subpage_query = [](std::string_view query,
                                           bool shadow_dom = false) {
    constexpr char kOsSettingsAppsPage[] = "os-settings-apps-page";
    constexpr char kOsSettingsMain[] = "os-settings-main";
    constexpr char kOsSettingsMainPageContainer[] = "#mainPageContainer";
    constexpr char kOsSettingsSubpage[] = "os-settings-subpage";
    constexpr char kOsSettingsUi[] = "os-settings-ui";

    const DeepQuery deep_query({kOsSettingsUi, kOsSettingsMain,
                                kOsSettingsMainPageContainer,
                                kOsSettingsAppsPage});

    return shadow_dom
               ? (deep_query + kOsSettingsSubpage) + std::string(query)
               : (deep_query + base::StrCat({kOsSettingsSubpage, " ", query}));
  };

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

      // Find menu item.
      NameView(kShowAppInfoMenuItemElementName,
               base::BindOnce(FindMenuItemViewForCommand, ash::SHOW_APP_INFO)
                   .Then(base::BindOnce<views::View*(views::View*)>(
                       views::AsViewClass<views::View>))),

      // Launch Settings app.
      InstrumentNextTab(kSettingsAppWebContentsElementId, AnyBrowser()),
      DoDefaultAction(kShowAppInfoMenuItemElementName),

      // Check Settings app browser.
      CheckElement(kSettingsAppWebContentsElementId,
                   base::BindOnce(&AsInstrumentedWebContents)
                       .Then(base::BindOnce(
                           &WebContentsInteractionTestUtil::web_contents))
                       .Then(base::BindOnce(&chrome::FindBrowserWithTab))
                       .Then(base::BindOnce(&IsBrowserForWebApp,
                                            web_app::kOsSettingsAppId))),

      // Check Settings app launch URL.
      WaitForWebContentsReady(
          kSettingsAppWebContentsElementId,
          chrome::GetOSSettingsUrl(
              base::StrCat({chromeos::settings::mojom::kAppDetailsSubpagePath,
                            "?id=", web_app::kContainerAppId}))),

      // Check container app title.
      CheckJsResultAt(
          kSettingsAppWebContentsElementId,
          get_settings_app_subpage_query("#subpageTitle", /*shadow_dom=*/true),
          base::StrCat({"subpageTitle => subpageTitle.innerText === '",
                        base::UTF16ToUTF8(GetContainerAppTitle()), "'"})),

      // Check container app cannot be uninstalled.
      CheckJsResultAt(
          kSettingsAppWebContentsElementId,
          get_settings_app_subpage_query("app-management-uninstall-button"),
          "appManagementUninstallButton => "
          "!appManagementUninstallButton.shadowRoot.querySelector('*[role="
          "button]')"));
}

// Initializes user state and restarts Chrome before `UninstallFromShelf`.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest, PRE_UninstallFromShelf) {}

// Verifies that the container app cannot be uninstalled from the shelf.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiTest, UninstallFromShelf) {
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

// ContainerAppInteractiveUiIneligibilityTest ----------------------------------

// Reasons why the user may be ineligible for container app preinstallation.
enum class IneligibilityReason {
  kMinValue = 0,
  kFeatureDebugAndManagementFlagsDisabled = kMinValue,
  kFeatureFlagDisabled,
  kUserManaged,
  kUserTypeChild,
  kUserTypeGuest,
  kMaxValue = kUserTypeGuest,
};

#define INELIGIBILITY_REASON_CASE(reason) \
  case IneligibilityReason::reason:       \
    return os << std::string(#reason)

inline std::ostream& operator<<(std::ostream& os, IneligibilityReason reason) {
  switch (reason) {
    INELIGIBILITY_REASON_CASE(kFeatureDebugAndManagementFlagsDisabled);
    INELIGIBILITY_REASON_CASE(kFeatureFlagDisabled);
    INELIGIBILITY_REASON_CASE(kUserManaged);
    INELIGIBILITY_REASON_CASE(kUserTypeChild);
    INELIGIBILITY_REASON_CASE(kUserTypeGuest);
  }
}

// Base class for interactive UI tests of container app ineligibility.
class ContainerAppInteractiveUiIneligibilityTest
    : public ContainerAppInteractiveUiTestBase,
      public WithParamInterface<IneligibilityReason> {
 public:
  ContainerAppInteractiveUiIneligibilityTest()
      : ContainerAppInteractiveUiTestBase(GetLoginType()) {
    scoped_feature_list_.InitWithFeatureStates(
        {{chromeos::features::kContainerAppPreinstall, IsFeatureFlagEnabled()},
         {chromeos::features::kContainerAppPreinstallDebug,
          IsFeatureDebugFlagEnabled()},
         {chromeos::features::kFeatureManagementContainerAppPreinstall,
          IsFeatureManagementFlagEnabled()}});
  }

 private:
  // ContainerAppInteractiveUiTestBase:
  void SetUpOnMainThread() override {
    // Web app preinstallation times out for child user types due to failure to
    // install some default web apps. Since this test suite only cares about the
    // preinstallation of the container app, circumvent timeouts by disabling
    // other default web apps.
    std::unique_ptr<web_app::ScopedTestingPreinstalledAppData> app_data;
    if (GetLoginType() == ash::LoggedInUserMixin::LogInType::kChild) {
      app_data = std::make_unique<web_app::ScopedTestingPreinstalledAppData>();
      app_data->apps.emplace_back(
          web_app::GetConfigForContainer(/*device_info=*/std::nullopt));
    }

    ContainerAppInteractiveUiTestBase::SetUpOnMainThread();
  }

  // Returns the login type for the user given test parameterization.
  std::optional<ash::LoggedInUserMixin::LogInType> GetLoginType() const {
    switch (GetParam()) {
      case IneligibilityReason::kUserTypeChild:
        return ash::LoggedInUserMixin::LogInType::kChild;
      case IneligibilityReason::kUserTypeGuest:
        return std::nullopt;
      case IneligibilityReason::kUserManaged:
        return ash::LoggedInUserMixin::LogInType::kManaged;
      default:
        return ash::LoggedInUserMixin::LogInType::kConsumer;
    }
  }

  // Returns whether the feature debug flag is enabled given test
  // parameterization.
  bool IsFeatureDebugFlagEnabled() const {
    return GetParam() !=
           IneligibilityReason::kFeatureDebugAndManagementFlagsDisabled;
  }

  // Returns whether the feature flag is enabled given test parameterization.
  bool IsFeatureFlagEnabled() const {
    return GetParam() != IneligibilityReason::kFeatureFlagDisabled;
  }

  // Returns whether the feature management flag is enabled given test
  // parameterization.
  bool IsFeatureManagementFlagEnabled() const {
    return GetParam() !=
           IneligibilityReason::kFeatureDebugAndManagementFlagsDisabled;
  }

  // Used to enable/disable the container app preinstallation based on test
  // parameterization.
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContainerAppInteractiveUiIneligibilityTest,
    ValuesIn(([]() {
      std::vector<IneligibilityReason> reasons(
          static_cast<int>(IneligibilityReason::kMaxValue) -
          static_cast<int>(IneligibilityReason::kMinValue) + 1);
      base::ranges::generate(
          reasons.begin(), reasons.end(),
          [i = static_cast<int>(IneligibilityReason::kMinValue)]() mutable {
            return static_cast<IneligibilityReason>(i++);
          });
      return reasons;
    })()),
    PrintToStringParamName());

// Tests -----------------------------------------------------------------------

// Verifies the container app is absent from the app list for ineligible users.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiIneligibilityTest,
                       AbsentFromAppList) {
  RunTestSequence(
      // Launch app list.
      DoDefaultAction(ash::kHomeButtonElementId),
      WaitForShow(ash::kAppListBubbleViewElementId),

      // Find apps page.
      NameDescendantViewByType<ash::AppListBubbleAppsPage>(
          ash::kAppListBubbleViewElementId, kAppListBubbleAppsPageElementName),

      // Find apps grid.
      NameDescendantViewByType<ash::AppsGridView>(
          kAppListBubbleAppsPageElementName, kAppsGridViewElementName),

      // Check container app absent.
      CheckView(
          kAppsGridViewElementName, [](ash::AppsGridView* apps_grid_view) {
            std::vector<raw_ptr<ash::AppListItemView>> apps;
            FindDescendantsOfClass(apps_grid_view, apps);
            return apps.size() &&
                   base::ranges::none_of(apps, [&](ash::AppListItemView* app) {
                     return IsAppListItemViewForWebApp(web_app::kContainerAppId,
                                                       app);
                   });
          }));
}

// Verifies the container app is absent from the shelf for ineligible users.
IN_PROC_BROWSER_TEST_P(ContainerAppInteractiveUiIneligibilityTest,
                       AbsentFromShelf) {
  RunTestSequence(
      // Check container app absent.
      CheckView(ash::kShelfViewElementId, [](ash::ShelfView* shelf) {
        std::vector<raw_ptr<ash::ShelfAppButton>> apps;
        FindDescendantsOfClass(shelf, apps);
        return apps.size() &&
               base::ranges::none_of(
                   apps, [&, shelf = raw_ptr(shelf)](ash::ShelfAppButton* app) {
                     return IsShelfAppButtonForWebApp(
                         std::cref(shelf), web_app::kContainerAppId, app);
                   });
      }));
}
