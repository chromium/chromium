// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks_templates/desks_templates_client.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/templates/desks_templates_test_util.h"
#include "ash/wm/desks/templates/desks_templates_util.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_test_helper.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks_templates/desks_templates_app_launch_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/policy_constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ElementsAre;

namespace {

constexpr int32_t kSettingsWindowId = 100;
constexpr int32_t kHelpWindowId = 200;
constexpr int32_t kLaunchedWindowIdBase = 1000000000;

constexpr char kExampleUrl1[] = "https://examples1.com";
constexpr char kExampleUrl2[] = "https://examples2.com";
constexpr char kExampleUrl3[] = "https://examples3.com";
constexpr char kAboutBlankUrl[] = "about:blank";
constexpr char kNewTabPageUrl[] = "chrome://newtab/";
constexpr char kYoutubeUrl[] = "https://www.youtube.com/";
constexpr char kTestAdminTemplateUuid[] =
    "1f4ec992-0fa9-415d-a136-4b7c292c39dc";
constexpr char kTestAdminTemplateFormat[] =
    "[{\"version\":1,\"uuid\":\"%s\",\"name\": \"test admin template\","
    "\"created_time_usec\": \"1633535632\",\"updated_time_usec\": "
    "\"1633535632\",\"desk\":{}}]";

Browser* FindBrowser(int32_t window_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* window = browser->window()->GetNativeWindow();
    if (window->GetProperty(app_restore::kRestoreWindowIdKey) == window_id)
      return browser;
  }
  return nullptr;
}

aura::Window* FindBrowserWindow(int32_t window_id) {
  Browser* browser = FindBrowser(window_id);
  return browser ? browser->window()->GetNativeWindow() : nullptr;
}

std::vector<GURL> GetURLsForBrowserWindow(Browser* browser) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  std::vector<GURL> urls;
  for (int i = 0; i < tab_strip_model->count(); ++i)
    urls.push_back(tab_strip_model->GetWebContentsAt(i)->GetVisibleURL());
  return urls;
}

// Locate a browser launched from a template whose URLs match `urls`.
Browser* FindLaunchedBrowserByURLs(const std::vector<GURL>& urls) {
  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* window = browser->window()->GetNativeWindow();
    if (window->GetProperty(app_restore::kRestoreWindowIdKey) >=
            kLaunchedWindowIdBase &&
        GetURLsForBrowserWindow(browser) == urls) {
      return browser;
    }
  }
  return nullptr;
}

// TODO(crbug.com/1286515): Remove this. Tests should navigate to overview and
// click the button using an event generator.
std::unique_ptr<ash::DeskTemplate> CaptureActiveDeskAndSaveTemplate() {
  base::RunLoop run_loop;
  std::unique_ptr<ash::DeskTemplate> desk_template;
  DesksTemplatesClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ash::DeskTemplate> captured_desk_template,
              std::string error_string) {
            run_loop.Quit();
            ASSERT_TRUE(captured_desk_template);
            desk_template = std::move(captured_desk_template);
          }));
  run_loop.Run();
  return desk_template;
}

std::vector<ash::DeskTemplate*> GetDeskTemplates() {
  base::RunLoop run_loop;
  std::vector<ash::DeskTemplate*> templates;

  DesksTemplatesClient::Get()->GetDeskTemplates(base::BindLambdaForTesting(
      [&](const std::vector<ash::DeskTemplate*>& desk_templates,
          std::string error_string) {
        templates = desk_templates;
        run_loop.Quit();
      }));
  run_loop.Run();

  return templates;
}

// Search `desk_templates` for a template with `uuid` and returns true if found,
// false if not.
bool ContainUuidInTemplates(
    const std::string& uuid,
    const std::vector<ash::DeskTemplate*>& desk_templates) {
  base::GUID guid = base::GUID::ParseCaseInsensitive(uuid);
  DCHECK(guid.is_valid());

  for (auto* desk_template : desk_templates) {
    if (desk_template->uuid() == guid)
      return true;
  }

  return false;
}

std::string GetTemplateJson(const std::string& uuid, Profile* profile) {
  base::RunLoop run_loop;
  std::string template_json_result;
  DesksTemplatesClient::Get()->GetTemplateJson(
      uuid, profile,
      base::BindLambdaForTesting(
          [&](const std::string& template_json, std::string error_string) {
            run_loop.Quit();
            ASSERT_TRUE(error_string.empty());
            template_json_result = template_json;
          }));
  run_loop.Run();
  return template_json_result;
}

void DeleteDeskTemplate(const base::GUID uuid) {
  base::RunLoop run_loop;
  DesksTemplatesClient::Get()->DeleteDeskTemplate(
      uuid.AsLowercaseString(),
      base::BindLambdaForTesting(
          [&](std::string error_string) { run_loop.Quit(); }));
  run_loop.Run();
}

web_app::AppId CreateSystemWebApp(Profile* profile,
                                  web_app::SystemAppType app_type) {
  DCHECK(app_type == web_app::SystemAppType::SETTINGS ||
         app_type == web_app::SystemAppType::HELP);
  web_app::AppId app_id = *web_app::GetAppIdForSystemWebApp(profile, app_type);
  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, apps::mojom::LaunchSource::kFromTest);
  params.restore_id = app_type == web_app::SystemAppType::SETTINGS
                          ? kSettingsWindowId
                          : kHelpWindowId;
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithParams(
      std::move(params));
  web_app::FlushSystemWebAppLaunchesForTesting(profile);
  return app_id;
}

web_app::AppId CreateSettingsSystemWebApp(Profile* profile) {
  return CreateSystemWebApp(profile, web_app::SystemAppType::SETTINGS);
}

web_app::AppId CreateHelpSystemWebApp(Profile* profile) {
  return CreateSystemWebApp(profile, web_app::SystemAppType::HELP);
}

void ClickButton(const views::Button* button) {
  DCHECK(button);
  DCHECK(button->GetVisible());
  aura::Window* root_window =
      button->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

// If `wait_for_ui` is true, wait for the callback from the model to update the
// UI.
void ClickSaveDeskAsTemplateButton(bool wait_for_ui) {
  views::Button* save_desk_as_template_button =
      ash::GetSaveDeskAsTemplateButton();
  DCHECK(save_desk_as_template_button);
  ClickButton(save_desk_as_template_button);
  if (wait_for_ui)
    ash::WaitForDesksTemplatesUI();
  // Clicking the save template button selects the newly created template's name
  // field. We can press enter or escape or click to select out of it.
  ash::SendKey(ui::VKEY_RETURN);
}

void ClickSaveDeskAsTemplateButton() {
  ClickSaveDeskAsTemplateButton(/*wait_for_ui=*/true);
}

void ClickZeroStateTemplatesButton() {
  views::Button* zero_state_templates_button =
      ash::GetZeroStateDesksTemplatesButton();
  ASSERT_TRUE(zero_state_templates_button);
  ClickButton(zero_state_templates_button);
  ash::WaitForDesksTemplatesUI();
}

void ClickExpandedStateTemplatesButton() {
  views::Button* expanded_state_templates_button =
      ash::GetExpandedStateDesksTemplatesButton();
  ASSERT_TRUE(expanded_state_templates_button);
  ClickButton(expanded_state_templates_button);
}

void ClickFirstTemplateItem() {
  views::Button* template_item = ash::GetTemplateItemButton(/*index=*/0);
  DCHECK(template_item);
  ClickButton(template_item);

  // We need to wait for the template to be fetched from the model.
  ash::WaitForDesksTemplatesUI();
}

const std::vector<ash::DeskTemplate*> GetAllEntries() {
  std::vector<ash::DeskTemplate*> templates;
  base::RunLoop loop;
  DesksTemplatesClient::Get()->GetDeskModel()->GetAllEntries(
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::GetAllEntriesStatus status,
              const std::vector<ash::DeskTemplate*>& entries) {
            DCHECK_EQ(desks_storage::DeskModel::GetAllEntriesStatus::kOk,
                      status);
            templates = entries;
            loop.Quit();
          }));
  loop.Run();
  return templates;
}

class MockDesksTemplatesAppLaunchHandler
    : public DesksTemplatesAppLaunchHandler {
 public:
  explicit MockDesksTemplatesAppLaunchHandler(Profile* profile)
      : DesksTemplatesAppLaunchHandler(profile) {}
  MockDesksTemplatesAppLaunchHandler(
      const MockDesksTemplatesAppLaunchHandler&) = delete;
  MockDesksTemplatesAppLaunchHandler& operator=(
      const MockDesksTemplatesAppLaunchHandler&) = delete;
  ~MockDesksTemplatesAppLaunchHandler() override = default;

  MOCK_METHOD(void,
              LaunchSystemWebAppOrChromeApp,
              (apps::AppType,
               const std::string&,
               const app_restore::RestoreData::LaunchList&),
              (override));
};

}  // namespace

// Scoped class that temporarily sets a new app launch handler for testing
// purposes.
class ScopedDesksTemplatesAppLaunchHandlerSetter {
 public:
  ScopedDesksTemplatesAppLaunchHandlerSetter(
      std::unique_ptr<DesksTemplatesAppLaunchHandler> launch_handler,
      int launch_id)
      : launch_id_(launch_id) {
    DCHECK(!instance_active_);
    instance_active_ = true;

    DesksTemplatesClient* desks_client = DesksTemplatesClient::Get();
    DCHECK(desks_client);
    desks_client->app_launch_handlers_[launch_id_] = std::move(launch_handler);
  }
  ScopedDesksTemplatesAppLaunchHandlerSetter(
      const ScopedDesksTemplatesAppLaunchHandlerSetter&) = delete;
  ScopedDesksTemplatesAppLaunchHandlerSetter& operator=(
      const ScopedDesksTemplatesAppLaunchHandlerSetter&) = delete;
  ~ScopedDesksTemplatesAppLaunchHandlerSetter() {
    DCHECK(instance_active_);
    instance_active_ = false;

    DesksTemplatesClient* desks_client = DesksTemplatesClient::Get();
    DCHECK(desks_client);
    desks_client->app_launch_handlers_[launch_id_] = nullptr;
  }

 private:
  // Variable to ensure we never have more than one instance of this object.
  inline static bool instance_active_ = false;

  // The id of the launch used for testing.
  int launch_id_;
};

class DesksTemplatesClientTest : public extensions::PlatformAppBrowserTest {
 public:
  DesksTemplatesClientTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kDesksTemplates},
        /*disabled_features=*/{ash::features::kDeskTemplateSync});
  }
  DesksTemplatesClientTest(const DesksTemplatesClientTest&) = delete;
  DesksTemplatesClientTest& operator=(const DesksTemplatesClientTest&) = delete;
  ~DesksTemplatesClientTest() override = default;

  // TODO(crbug.com/1286515): These functions will be removed with the
  // extension. Avoid further uses of this method and create or launch templates
  // by mocking clicks on the system UI.
  void SetTemplate(std::unique_ptr<ash::DeskTemplate> launch_template) {
    if (launch_template->launch_id() == 0)
      launch_template->set_launch_id(1);
    DesksTemplatesClient::Get()->launch_template_for_test_ =
        std::move(launch_template);
  }

  void LaunchTemplate(const base::GUID& uuid) {
    DesksTemplatesClient::Get()->LaunchDeskTemplate(uuid.AsLowercaseString(),
                                                    base::DoNothing());
  }

  void SetAndLaunchTemplate(std::unique_ptr<ash::DeskTemplate> desk_template) {
    ash::DeskTemplate* desk_template_ptr = desk_template.get();
    SetTemplate(std::move(desk_template));
    LaunchTemplate(desk_template_ptr->uuid());
  }

  Browser* CreateBrowser(
      const std::vector<GURL>& urls,
      absl::optional<size_t> active_url_index = absl::nullopt) {
    Browser::CreateParams params(Browser::TYPE_NORMAL, profile(),
                                 /*user_gesture=*/false);
    Browser* browser = Browser::Create(params);
    // Create a new tab and make sure the urls have loaded.
    for (size_t i = 0; i < urls.size(); i++) {
      content::TestNavigationObserver navigation_observer(urls[i]);
      navigation_observer.StartWatchingNewWebContents();
      chrome::AddTabAt(
          browser, urls[i], /*index=*/-1,
          /*foreground=*/!active_url_index || active_url_index.value() == i);
      navigation_observer.Wait();
    }
    browser->window()->Show();
    return browser;
  }

  Browser* InstallAndLaunchPWA(const GURL& start_url, bool launch_in_browser) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    if (!launch_in_browser)
      web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    const web_app::AppId app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));

    // Wait for app service to see the newly installed app.
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->FlushMojoCallsForTesting();

    return launch_in_browser
               ? web_app::LaunchBrowserForWebAppInTab(profile(), app_id)
               : web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  }

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    ::full_restore::SetActiveProfilePath(profile()->GetPath());
    web_app::WebAppProvider::GetForTest(profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a browser's urls can be captured correctly in the desk template.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, CaptureBrowserUrlsTest) {
  // Create a new browser and add a few tabs to it.
  Browser* browser = CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  // Find |browser| window's app restore data.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Check the urls are captured correctly in the |desk_template|.
  EXPECT_EQ(data->urls.value(), urls);
}

// Tests that incognito browser windows will NOT be captured in the desk
// template.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, CaptureIncognitoBrowserTest) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  chrome::AddTabAt(incognito_browser, GURL(kExampleUrl1), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(incognito_browser, GURL(kExampleUrl2), /*index=*/-1,
                   /*foreground=*/true);
  incognito_browser->window()->Show();
  aura::Window* window = incognito_browser->window()->GetNativeWindow();

  const int32_t incognito_browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  // Find |browser| window's app restore data.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(incognito_browser_window_id);
  // Created incognito window is NOT in restore list
  ASSERT_TRUE(app_restore_data_iter == iter->second.end());
}

// Tests that browsers and chrome apps can be captured correctly in the desk
// template.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       CaptureActiveDeskAsTemplateTest) {
  // Test that Singleton was properly initialized.
  ASSERT_TRUE(DesksTemplatesClient::Get());

  // Change |browser|'s bounds.
  const gfx::Rect browser_bounds = gfx::Rect(0, 0, 800, 200);
  aura::Window* window = browser()->window()->GetNativeWindow();
  window->SetBounds(browser_bounds);
  // Make window visible on all desks.
  window->SetProperty(aura::client::kWindowWorkspaceKey,
                      aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);

  // Create the settings app, which is a system web app.
  web_app::AppId settings_app_id =
      CreateSettingsSystemWebApp(browser()->profile());

  // Change the Settings app's bounds too.
  const gfx::Rect settings_app_bounds = gfx::Rect(100, 100, 800, 300);
  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  const int32_t settings_window_id =
      settings_window->GetProperty(app_restore::kWindowIdKey);
  ASSERT_TRUE(settings_window);
  settings_window->SetBounds(settings_app_bounds);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  // Test the default template's name is the current desk's name.
  auto* desks_controller = ash::DesksController::Get();
  EXPECT_EQ(
      desk_template->template_name(),
      desks_controller->GetDeskName(desks_controller->GetActiveDeskIndex()));

  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 2u);

  // Find |browser| window's app restore data.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Verify window info are correctly captured.
  EXPECT_EQ(browser_bounds, data->current_bounds.value());
  // `visible_on_all_workspaces` should have been reset even though
  // the captured window is visible on all workspaces.
  EXPECT_FALSE(data->desk_id.has_value());
  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  // We don't capture the window's desk_id as a template will always
  // create in a new desk.
  EXPECT_FALSE(data->desk_id.has_value());

  // Find Setting app's app restore data.
  auto iter2 = app_id_to_launch_list.find(settings_app_id);
  ASSERT_TRUE(iter2 != app_id_to_launch_list.end());
  auto app_restore_data_iter2 = iter2->second.find(settings_window_id);
  ASSERT_TRUE(app_restore_data_iter2 != iter2->second.end());
  const auto& data2 = app_restore_data_iter2->second;
  EXPECT_EQ(
      static_cast<int>(apps::mojom::LaunchContainer::kLaunchContainerWindow),
      data2->container.value());
  EXPECT_EQ(static_cast<int>(WindowOpenDisposition::NEW_WINDOW),
            data2->disposition.value());
  // Verify window info are correctly captured.
  EXPECT_EQ(settings_app_bounds, data2->current_bounds.value());
  EXPECT_FALSE(data2->desk_id.has_value());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_FALSE(data2->desk_id.has_value());
}

// Tests that launching the same desk template multiple times creates desks with
// different/incremented names.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, LaunchMultipleDeskTemplates) {
  const base::GUID kDeskUuid = base::GUID::GenerateRandomV4();
  const std::u16string kDeskName(u"Test Desk Name");

  auto* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // TODO(crbug.com/1273532): Note that `SetTemplate` allows setting an empty
  // desk template which shouldn't be possible in a real workflow. Make sure a
  // non empty desks are launched when this test is updated to use the real
  // workflow.
  auto desk_template = std::make_unique<ash::DeskTemplate>(
      kDeskUuid.AsLowercaseString(), ash::DeskTemplateSource::kUser,
      base::UTF16ToUTF8(kDeskName), base::Time::Now());
  SetTemplate(std::move(desk_template));

  auto check_launch_template_desk_name =
      [kDeskUuid, desks_controller, this](const std::u16string& desk_name) {
        LaunchTemplate(kDeskUuid);

        EXPECT_EQ(desk_name, desks_controller->GetDeskName(
                                 desks_controller->GetActiveDeskIndex()));
      };

  // Launching a desk from the template creates a desk with the same name as the
  // template.
  check_launch_template_desk_name(kDeskName);

  // Launch more desks from the template and verify that the newly created desks
  // have unique names.
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (1)"));
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (2)"));

  // Remove "Test Desk Name (1)", which means the next created desk from
  // template will have that name. Then it will skip (2) since it already
  // exists, and create the next desk with (3).
  RemoveDesk(desks_controller->desks()[2].get());
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (1)"));
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (3)"));

  // Same as above, but make sure that deleting the desk with the exact template
  // name still functions the same by only filling in whatever name is
  // available.
  RemoveDesk(desks_controller->desks()[1].get());
  check_launch_template_desk_name(kDeskName);
  check_launch_template_desk_name(std::u16string(kDeskName).append(u" (4)"));
}

// Tests that launching a template that contains a system web app works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, LaunchTemplateWithSystemApp) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(browser()->profile());

  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  ASSERT_TRUE(settings_window);
  const std::u16string settings_title = settings_window->GetTitle();

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  // Close the settings window. We'll need to verify if it reopens later.
  views::Widget* settings_widget =
      views::Widget::GetWidgetForNativeWindow(settings_window);
  settings_widget->CloseNow();
  ASSERT_FALSE(FindBrowserWindow(kSettingsWindowId));
  settings_window = nullptr;

  auto* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the settings window has been launched on the new desk (desk B).
  // TODO(sammiequon): Right now the app just launches, so verify the title
  // matches. We should verify the restore id and use
  // `FindBrowserWindow(kSettingsWindowId)` once things are wired up properly.
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* window = browser->window()->GetNativeWindow();
    if (window->GetTitle() == settings_title) {
      settings_window = window;
      break;
    }
  }
  ASSERT_TRUE(settings_window);
  EXPECT_EQ(ash::Shell::GetContainer(settings_window->GetRootWindow(),
                                     ash::kShellWindowId_DeskContainerB),
            settings_window->parent());
}

// Tests that launching a template that contains a system web app will move the
// existing instance of the system web app to the current desk.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       LaunchTemplateWithSystemAppExisting) {
  ASSERT_TRUE(DesksTemplatesClient::Get());
  Profile* profile = browser()->profile();

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(profile);

  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  ASSERT_TRUE(settings_window);
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  // Give the settings app a known position.
  const gfx::Rect settings_bounds(100, 100, 600, 400);
  settings_window->SetBounds(settings_bounds);
  // Focus the browser so that the settings window is stacked at the bottom.
  browser()->window()->GetNativeWindow()->Focus();
  ASSERT_THAT(settings_window->parent()->children(),
              ElementsAre(settings_window, _));

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  // Move the settings window to a new place and stack it on top so that we can
  // later verify that it has been placed and stacked correctly.
  settings_window->SetBounds(gfx::Rect(150, 150, 650, 500));
  settings_window->Focus();

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));

  // We launch a new browser window, but not a new settings app. Verify that the
  // window has been moved to the right place and stacked at the bottom.
  EXPECT_EQ(3u, BrowserList::GetInstance()->size());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(settings_window));
  EXPECT_EQ(settings_bounds, settings_window->bounds());
  ASSERT_THAT(settings_window->parent()->children(),
              ElementsAre(settings_window, _));
}

// Tests that launching a template that contains a chrome app works as expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, LaunchTemplateWithChromeApp) {
  DesksTemplatesClient* desks_client = DesksTemplatesClient::Get();
  ASSERT_TRUE(desks_client);

  // Create a chrome app.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);

  const std::string extension_id = extension->id();
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<app_restore::AppLaunchInfo>(
          extension_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  extensions::AppWindow* app_window = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window);
  ASSERT_TRUE(GetFirstAppWindowForApp(extension_id));

  // Capture the active desk, which contains the chrome app.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);
  desk_template->set_launch_id(1);

  // Close the chrome app window. We'll need to verify if it reopens later.
  views::Widget* app_widget =
      views::Widget::GetWidgetForNativeWindow(app_window->GetNativeWindow());
  app_widget->CloseNow();
  ASSERT_FALSE(GetFirstAppWindowForApp(extension_id));

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // `BrowserAppLauncher::LaunchAppWithParams()` does not launch the chrome app
  // in tests, so here we set up a mock app launch handler and just verify a
  // `LaunchSystemWebAppOrChromeApp()` call with the associated extension is
  // seen.
  auto mock_app_launch_handler =
      std::make_unique<MockDesksTemplatesAppLaunchHandler>(profile());
  MockDesksTemplatesAppLaunchHandler* mock_app_launch_handler_ptr =
      mock_app_launch_handler.get();
  ScopedDesksTemplatesAppLaunchHandlerSetter scoped_launch_handler(
      std::move(mock_app_launch_handler), desk_template->launch_id());

  EXPECT_CALL(*mock_app_launch_handler_ptr,
              LaunchSystemWebAppOrChromeApp(_, extension_id, _));

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));
}

// Tests that launching a template that contains a browser window works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       LaunchTemplateWithBrowserWindow) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  // Create a new browser and add a few tabs to it, and specify the active tab
  // index.
  const size_t browser_active_index = 1;
  Browser* browser = CreateBrowser(
      {GURL(kExampleUrl1), GURL(kExampleUrl2), GURL(kExampleUrl3)},
      /*active_url_index=*/browser_active_index);

  // Verify that the active tab is correct.
  EXPECT_EQ(static_cast<int>(browser_active_index),
            browser->tab_strip_model()->active_index());

  // Get current tabs from browser.
  const std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));

  // Wait for the tabs to load.
  content::RunAllTasksUntilIdle();

  // Verify that the browser was launched with the correct urls and active tab.
  Browser* new_browser = FindLaunchedBrowserByURLs(urls);
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(static_cast<int>(browser_active_index),
            new_browser->tab_strip_model()->active_index());

  // Verify that the browser window has been launched on the new desk (desk B).
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
  aura::Window* browser_window = new_browser->window()->GetNativeWindow();
  ASSERT_TRUE(browser_window);
  EXPECT_EQ(ash::Shell::GetContainer(browser_window->GetRootWindow(),
                                     ash::kShellWindowId_DeskContainerB),
            browser_window->parent());
}

// Tests that browser session restore isn't triggered when we launch a template
// that contains a browser window.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       PreventBrowserSessionRestoreTest) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  // Do not exit from test or delete the Profile* when last browser is closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      browser()->profile(), ProfileKeepAliveOrigin::kBrowserWindow);

  // Enable session service.
  SessionStartupPref pref(SessionStartupPref::LAST);
  Profile* profile = browser()->profile();
  SessionStartupPref::SetStartupPref(profile, pref);

  const int expected_tab_count = 2;
  chrome::AddTabAt(browser(), GURL(kExampleUrl2), /*index=*/-1,
                   /*foreground=*/true);
  EXPECT_EQ(expected_tab_count, browser()->tab_strip_model()->count());

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  // Close the browser and verify that all browser windows are closed.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // Set the template we created and launch the template.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the browser was launched with the correct number of tabs, and
  // that browser session restore did not restore any windows/tabs.
  Browser* new_browser =
      FindLaunchedBrowserByURLs({GURL(kAboutBlankUrl), GURL(kNewTabPageUrl)});
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

// Tests that the windows and tabs count histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       DeskTemplateWindowAndTabCountHistogram) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  base::HistogramTester histogram_tester;

  Profile* profile = browser()->profile();

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(profile);

  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2), GURL(kExampleUrl3)});

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 2u);

  constexpr char kWindowCountHistogramName[] = "Ash.DeskTemplate.WindowCount";
  constexpr char kTabCountHistogramName[] = "Ash.DeskTemplate.TabCount";
  constexpr char kWindowAndTabCountHistogramName[] =
      "Ash.DeskTemplate.WindowAndTabCount";
  // NOTE: there is an existing browser with 1 tab created by BrowserMain().
  histogram_tester.ExpectBucketCount(kWindowCountHistogramName, 4, 1);
  histogram_tester.ExpectBucketCount(kTabCountHistogramName, 6, 1);
  histogram_tester.ExpectBucketCount(kWindowAndTabCountHistogramName, 7, 1);
}

// Tests that the launch from template histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       DeskTemplateLaunchFromTemplateHistogram) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  base::HistogramTester histogram_tester;

  // Create a new browser.
  CreateBrowser({});

  // Save the template.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  ASSERT_TRUE(desk_template);

  const int launches = 5;
  for (int i = 0; i < launches; i++) {
    auto launch_template = desk_template->Clone();
    launch_template->set_launch_id(i + 1);
    SetAndLaunchTemplate(std::move(launch_template));
  }

  constexpr char kLaunchFromTemplateHistogramName[] =
      "Ash.DeskTemplate.LaunchFromTemplate";
  histogram_tester.ExpectTotalCount(kLaunchFromTemplateHistogramName, launches);
}

// Tests that the template count histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       DeskTemplateUserTemplateCountHistogram) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  base::HistogramTester histogram_tester;

  // Verify that all template saves and deletes are captured by the histogram.
  CaptureActiveDeskAndSaveTemplate();
  CaptureActiveDeskAndSaveTemplate();
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  DeleteDeskTemplate(desk_template->uuid());
  CaptureActiveDeskAndSaveTemplate();

  constexpr char kUserTemplateCountHistogramName[] =
      "Ash.DeskTemplate.UserTemplateCount";
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 1, 1);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 2, 2);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 3, 2);
}

// Tests that browser windows created from a template have the correct bounds
// and window state.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, BrowserWindowRestorationTest) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  // Create a new browser and set its bounds.
  std::vector<GURL> browser_urls_1 = {GURL(kExampleUrl1), GURL(kExampleUrl2)};
  Browser* browser_1 = CreateBrowser(browser_urls_1);
  const gfx::Rect browser_bounds_1 = gfx::Rect(100, 100, 600, 200);
  aura::Window* window_1 = browser_1->window()->GetNativeWindow();
  window_1->SetBounds(browser_bounds_1);

  // Create a new minimized browser.
  std::vector<GURL> browser_urls_2 = {GURL(kExampleUrl1)};
  Browser* browser_2 = CreateBrowser(browser_urls_2);
  const gfx::Rect browser_bounds_2 = gfx::Rect(150, 150, 500, 300);
  aura::Window* window_2 = browser_2->window()->GetNativeWindow();
  window_2->SetBounds(browser_bounds_2);
  EXPECT_EQ(browser_bounds_2, window_2->bounds());
  browser_2->window()->Minimize();

  // Create a new maximized browser.
  std::vector<GURL> browser_urls_3 = {GURL(kExampleUrl2)};
  Browser* browser_3 = CreateBrowser(browser_urls_3);
  browser_3->window()->Maximize();

  EXPECT_EQ(browser_bounds_1, window_1->bounds());
  EXPECT_EQ(browser_bounds_2, window_2->bounds());
  ASSERT_TRUE(browser_2->window()->IsMinimized());
  ASSERT_TRUE(browser_3->window()->IsMaximized());

  // Capture the active desk, which contains the two browser windows.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  // Set the template and launch it.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the browser was launched with the correct bounds.
  Browser* new_browser_1 = FindLaunchedBrowserByURLs(browser_urls_1);
  ASSERT_TRUE(new_browser_1);
  EXPECT_EQ(browser_bounds_1,
            new_browser_1->window()->GetNativeWindow()->bounds());

  // Verify that the browser was launched and minimized.
  Browser* new_browser_2 = FindLaunchedBrowserByURLs(browser_urls_2);
  ASSERT_TRUE(new_browser_2);
  ASSERT_TRUE(new_browser_2->window()->IsMinimized());
  EXPECT_EQ(browser_bounds_2,
            new_browser_2->window()->GetNativeWindow()->bounds());

  // Verify that the browser was launched and maximized.
  Browser* new_browser_3 = FindLaunchedBrowserByURLs(browser_urls_3);
  ASSERT_TRUE(new_browser_3);
  ASSERT_TRUE(new_browser_3->window()->IsMaximized());
}

// Tests that saving and launching a template that contains a PWA works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, LaunchTemplateWithPWA) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  Browser* pwa_browser =
      InstallAndLaunchPWA(GURL(kExampleUrl1), /*launch_in_browser=*/false);
  ASSERT_TRUE(pwa_browser->is_type_app());
  aura::Window* pwa_window = pwa_browser->window()->GetNativeWindow();
  const gfx::Rect pwa_bounds(50, 50, 500, 500);
  pwa_window->SetBounds(pwa_bounds);
  const int32_t pwa_window_id =
      pwa_window->GetProperty(app_restore::kWindowIdKey);
  const std::string* app_name =
      pwa_window->GetProperty(app_restore::kBrowserAppNameKey);
  ASSERT_TRUE(app_name);

  // Capture the active desk, which contains the PWA.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  // Find |pwa_browser| window's app restore data.
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);
  ASSERT_TRUE(restore_data->HasAppTypeBrowser());
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(pwa_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Verify window info are correctly captured.
  EXPECT_EQ(pwa_bounds, data->current_bounds.value());
  ASSERT_TRUE(data->app_type_browser.has_value() &&
              data->app_type_browser.value());
  EXPECT_EQ(*app_name, *data->app_name);

  // Set the template and launch it.
  SetAndLaunchTemplate(std::move(desk_template));

  // Verify that the PWA was launched correctly.
  Browser* new_pwa_browser = FindLaunchedBrowserByURLs({GURL(kExampleUrl1)});
  ASSERT_TRUE(new_pwa_browser);
  ASSERT_TRUE(new_pwa_browser->is_type_app());
  aura::Window* new_browser_window =
      new_pwa_browser->window()->GetNativeWindow();
  EXPECT_NE(new_browser_window, pwa_window);
  EXPECT_EQ(pwa_bounds, new_browser_window->bounds());
  const std::string* new_app_name =
      new_browser_window->GetProperty(app_restore::kBrowserAppNameKey);
  ASSERT_TRUE(new_app_name);
  EXPECT_EQ(*app_name, *new_app_name);
}

// Tests that saving and launching a template that contains a PWA in a browser
// window works as expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       LaunchTemplateWithPWAInBrowser) {
  ASSERT_TRUE(DesksTemplatesClient::Get());

  Browser* pwa_browser =
      InstallAndLaunchPWA(GURL(kYoutubeUrl), /*launch_in_browser=*/true);
  aura::Window* pwa_window = pwa_browser->window()->GetNativeWindow();
  const int32_t pwa_window_id =
      pwa_window->GetProperty(app_restore::kWindowIdKey);

  // Capture the active desk, which contains the PWA.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  // Test that |pwa_browser| restore data can be found.
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  // Test that |pwa_browser|'s restore data is saved under the Chrome browser
  // app id app_constants::kChromeAppId, not Youtube app id
  // extension_misc::kYoutubeAppId.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(pwa_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());

  iter = app_id_to_launch_list.find(extension_misc::kYoutubeAppId);
  EXPECT_FALSE(iter != app_id_to_launch_list.end());
}

// Tests that captured desk templates can be recalled as a JSON string.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, GetDeskTemplateJson) {
  // Test that Singleton was properly initialized.
  ASSERT_TRUE(DesksTemplatesClient::Get());

  // Change |browser|'s bounds.
  const gfx::Rect browser_bounds = gfx::Rect(0, 0, 800, 200);
  aura::Window* window = browser()->window()->GetNativeWindow();
  window->SetBounds(browser_bounds);
  // Make window visible on all desks.
  window->SetProperty(aura::client::kWindowWorkspaceKey,
                      aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);

  // Create the settings app, which is a system web app.
  web_app::AppId settings_app_id =
      CreateSettingsSystemWebApp(browser()->profile());

  // Change the Settings app's bounds too.
  const gfx::Rect settings_app_bounds = gfx::Rect(100, 100, 800, 300);
  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  ASSERT_TRUE(settings_window);
  settings_window->SetBounds(settings_app_bounds);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();

  std::string template_json = GetTemplateJson(
      desk_template->uuid().AsLowercaseString(), browser()->profile());

  // content of the conversion is tested in:
  // components/desks_storage/core/desk_template_conversion_unittests.cc in this
  // case we're simply interested in whether or not we got content back.
  ASSERT_TRUE(!template_json.empty());
}

// Tests that basic operations using the native UI work as expected.
// TODO(crbug.com/1286515): Remove the SystemUI prefix from these tests. Remove
// the tests that do not have the SystemUI prefix other than GetDeskTemplateJson
// once the extension is deprecated.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, SystemUIBasic) {
  auto* desk_model = DesksTemplatesClient::Get()->GetDeskModel();
  ASSERT_EQ(0u, desk_model->GetEntryCount());

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  // Tests that since we have no templates right now, so the desks templates
  // button is hidden.
  views::Button* zero_state_templates_button =
      ash::GetZeroStateDesksTemplatesButton();
  ASSERT_TRUE(zero_state_templates_button);
  EXPECT_FALSE(zero_state_templates_button->GetVisible());

  // Note that this button needs at least one window to show up. Browser tests
  // have an existing browser window, so no new window needs to be created.
  views::Button* save_desk_as_template_button =
      ash::GetSaveDeskAsTemplateButton();
  ASSERT_TRUE(save_desk_as_template_button);
  ClickButton(save_desk_as_template_button);

  ash::WaitForDesksTemplatesUI();

  EXPECT_EQ(1u, desk_model->GetEntryCount());

  // Tests that since we have one template right now, so that the expanded state
  // desk button is shown, and the desk templates grid has one item.
  auto* expanded_state_templates_button =
      ash::GetExpandedStateDesksTemplatesButton();
  ASSERT_TRUE(expanded_state_templates_button);
  EXPECT_TRUE(expanded_state_templates_button->GetVisible());

  views::Button* template_item = ash::GetTemplateItemButton(/*index=*/0);
  EXPECT_TRUE(template_item);
}

// Tests launching a template with a browser window.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest, SystemUILaunchBrowser) {
  // Create a new browser and add a few tabs to it, and specify the active tab
  // index.
  const size_t browser_active_index = 1;
  Browser* browser = CreateBrowser(
      {GURL(kExampleUrl1), GURL(kExampleUrl2), GURL(kExampleUrl3)},
      /*active_url_index=*/browser_active_index);

  // Verify that the active tab is correct.
  EXPECT_EQ(static_cast<int>(browser_active_index),
            browser->tab_strip_model()->active_index());

  // Get current tabs from browser.
  const std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  // There are two browser windows currently, the default one and the one we
  // just created.
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());

  // Enter overview and save the current desk as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  ClickFirstTemplateItem();

  // Wait for the tabs to load.
  content::RunAllTasksUntilIdle();

  // There are a total of four browser windows now. The two initial ones and the
  // two created from our template.
  EXPECT_EQ(4u, BrowserList::GetInstance()->size());

  // Test that the created browser has the same tabs and the same active tab.
  Browser* new_browser = FindLaunchedBrowserByURLs(urls);
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(static_cast<int>(browser_active_index),
            new_browser->tab_strip_model()->active_index());

  // Verify that the browser window has been launched on the new desk (desk B).
  EXPECT_EQ(1, ash::DesksController::Get()->GetActiveDeskIndex());
  aura::Window* browser_window = new_browser->window()->GetNativeWindow();
  ASSERT_TRUE(browser_window);
  EXPECT_EQ(ash::Shell::GetContainer(browser_window->GetRootWindow(),
                                     ash::kShellWindowId_DeskContainerB),
            browser_window->parent());
}

// Tests that a browser's urls can be captured correctly in the desk template.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUICaptureBrowserUrlsTest) {
  // Create a new browser and add a few tabs to it.
  Browser* browser = CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  std::vector<ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  ash::DeskTemplate* desk_template = templates.front();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(app_id_to_launch_list.size(), 1u);

  // Find `browser` window's app restore data.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Check the urls are captured correctly in the `desk_template`.
  EXPECT_EQ(data->urls.value(), urls);
}

// Tests that incognito browser windows will NOT be captured in the desk
// template.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUICaptureIncognitoBrowserTest) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  chrome::AddTabAt(incognito_browser, GURL(kExampleUrl1), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(incognito_browser, GURL(kExampleUrl2), /*index=*/-1,
                   /*foreground=*/true);
  incognito_browser->window()->Show();
  aura::Window* window = incognito_browser->window()->GetNativeWindow();

  const int32_t incognito_browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  // Incognito browsers are unsupported so a dialog will popup asking users if
  // they are sure. Use a key press to accept the dialog instead of a click as
  // dialog buttons think a click generated by the event generator is an
  // accidentally click and therefore ignores it.
  ClickSaveDeskAsTemplateButton(/*wait_for_ui=*/false);
  views::Button* dialog_accept_button =
      ash::GetDesksTemplatesDialogAcceptButton();
  ASSERT_TRUE(dialog_accept_button);
  aura::Window* root_window =
      dialog_accept_button->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressAndReleaseKey(ui::VKEY_RETURN);

  std::vector<ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  ash::DeskTemplate* desk_template = templates.front();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, app_id_to_launch_list.size());

  // Find `browser` window's app restore data.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_FALSE(iter == app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(incognito_browser_window_id);
  // Created incognito window is NOT in restore list.
  EXPECT_TRUE(iter->second.end() == app_restore_data_iter);
}

// Tests that launching a template that contains a system web app works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUILaunchTemplateWithSystemWebApp) {
  // Create the settings and help apps, which are system web apps.
  CreateSettingsSystemWebApp(browser()->profile());
  CreateHelpSystemWebApp(browser()->profile());

  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  aura::Window* help_window = FindBrowserWindow(kHelpWindowId);
  ASSERT_TRUE(settings_window);
  ASSERT_TRUE(help_window);
  const std::u16string settings_title = settings_window->GetTitle();
  const std::u16string help_title = help_window->GetTitle();

  // Maximize the help app.
  ash::WindowState::Get(help_window)->Maximize();

  // Enter overview and save the current desk as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  // Exit overview and close the settings window. We'll need to verify if it
  // reopens later.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();

  // Close both apps.
  views::Widget::GetWidgetForNativeWindow(settings_window)->CloseNow();
  views::Widget::GetWidgetForNativeWindow(help_window)->CloseNow();
  ASSERT_FALSE(FindBrowserWindow(kSettingsWindowId));
  ASSERT_FALSE(FindBrowserWindow(kHelpWindowId));

  // Enter overview, head over to the desks templates grid and launch the
  // template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickZeroStateTemplatesButton();
  ClickFirstTemplateItem();

  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* window = browser->window()->GetNativeWindow();
    const std::u16string title = window->GetTitle();
    if (title == settings_title)
      settings_window = window;
    if (title == help_title)
      help_window = window;
  }
  ASSERT_TRUE(settings_window);
  ASSERT_TRUE(help_window);
  EXPECT_EQ(ash::Shell::GetContainer(settings_window->GetRootWindow(),
                                     ash::kShellWindowId_DeskContainerB),
            settings_window->parent());
  EXPECT_EQ(ash::Shell::GetContainer(help_window->GetRootWindow(),
                                     ash::kShellWindowId_DeskContainerB),
            help_window->parent());
  EXPECT_TRUE(ash::WindowState::Get(help_window)->IsMaximized());
}

// Tests that launching a template that contains a system web app will move the
// existing instance of the system web app to the current desk.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUILaunchTemplateWithSWAExisting) {
  Profile* profile = browser()->profile();

  // Create the settings and help apps, which are system web apps.
  CreateSettingsSystemWebApp(profile);
  CreateHelpSystemWebApp(profile);

  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  aura::Window* help_window = FindBrowserWindow(kHelpWindowId);
  ASSERT_TRUE(settings_window);
  ASSERT_TRUE(help_window);
  EXPECT_EQ(3u, BrowserList::GetInstance()->size());

  // Give the settings app a known position, and maximize the help app.
  const gfx::Rect settings_bounds(100, 100, 600, 400);
  settings_window->SetBounds(settings_bounds);
  ash::WindowState::Get(help_window)->Maximize();
  // Focus the browser so that the settings window is stacked at the bottom.
  browser()->window()->GetNativeWindow()->Focus();
  ASSERT_THAT(settings_window->parent()->children(),
              ElementsAre(settings_window, help_window, _));

  // Enter overview and save the current desk as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  // Exit overview and move the settings window to a new place and stack it on
  // top so that we can later verify that it has been placed and stacked
  // correctly.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();
  settings_window->SetBounds(gfx::Rect(150, 150, 650, 500));
  settings_window->Focus();

  // Restore the help window so we can later verify that it remaximizes.
  ash::WindowState::Get(help_window)->Restore();

  // Enter overview, head over to the desks templates grid and launch the
  // template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickZeroStateTemplatesButton();
  ClickFirstTemplateItem();

  // Wait for the tabs to load.
  content::RunAllTasksUntilIdle();

  // Exit overview.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(1, desks_controller->GetActiveDeskIndex());

  // We launch a new browser window, but not a new settings or help app. Verify
  // that the settings window has been moved to the right place and stacked at
  // the bottom. Verify that the help window is maximized.
  EXPECT_EQ(4u, BrowserList::GetInstance()->size());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(settings_window));
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(help_window));
  EXPECT_EQ(settings_bounds, settings_window->bounds());
  EXPECT_TRUE(ash::WindowState::Get(help_window)->IsMaximized());

  // TODO(crbug.com/1281393): Verify that the element order is correct.

  // Tests that there is no clipping on the either window.
  EXPECT_EQ(gfx::Rect(), settings_window->layer()->clip_rect());
  EXPECT_EQ(gfx::Rect(), help_window->layer()->clip_rect());
}

// Tests that browser windows created from a template have the correct bounds
// and window state.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUIBrowserWindowRestorationTest) {
  // Create a new browser and set its bounds.
  std::vector<GURL> browser_urls_1 = {GURL(kExampleUrl1), GURL(kExampleUrl2)};
  Browser* browser_1 = CreateBrowser(browser_urls_1);
  const gfx::Rect browser_bounds_1 = gfx::Rect(100, 100, 600, 200);
  aura::Window* window_1 = browser_1->window()->GetNativeWindow();
  window_1->SetBounds(browser_bounds_1);

  // Create a new minimized browser.
  std::vector<GURL> browser_urls_2 = {GURL(kExampleUrl1)};
  Browser* browser_2 = CreateBrowser(browser_urls_2);
  const gfx::Rect browser_bounds_2 = gfx::Rect(150, 150, 500, 300);
  aura::Window* window_2 = browser_2->window()->GetNativeWindow();
  window_2->SetBounds(browser_bounds_2);
  EXPECT_EQ(browser_bounds_2, window_2->bounds());
  browser_2->window()->Minimize();

  // Create a new maximized browser.
  std::vector<GURL> browser_urls_3 = {GURL(kExampleUrl2)};
  Browser* browser_3 = CreateBrowser(browser_urls_3);
  browser_3->window()->Maximize();

  EXPECT_EQ(browser_bounds_1, window_1->bounds());
  EXPECT_EQ(browser_bounds_2, window_2->bounds());
  ASSERT_TRUE(browser_2->window()->IsMinimized());
  ASSERT_TRUE(browser_3->window()->IsMaximized());

  // Capture the active desk, which contains the three browser windows.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  ClickFirstTemplateItem();

  // Wait for the tabs to load.
  content::RunAllTasksUntilIdle();

  // Verify that the browser was launched with the correct bounds.
  Browser* new_browser_1 = FindLaunchedBrowserByURLs(browser_urls_1);
  ASSERT_TRUE(new_browser_1);
  EXPECT_EQ(browser_bounds_1,
            new_browser_1->window()->GetNativeWindow()->bounds());

  // Verify that the browser was launched and minimized.
  Browser* new_browser_2 = FindLaunchedBrowserByURLs(browser_urls_2);
  ASSERT_TRUE(new_browser_2);
  ASSERT_TRUE(new_browser_2->window()->IsMinimized());
  EXPECT_EQ(browser_bounds_2,
            new_browser_2->window()->GetNativeWindow()->bounds());

  // Verify that the browser was launched and maximized.
  Browser* new_browser_3 = FindLaunchedBrowserByURLs(browser_urls_3);
  ASSERT_TRUE(new_browser_3);
  ASSERT_TRUE(new_browser_3->window()->IsMaximized());
}

// Tests that saving and launching a template that contains a PWA works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUILaunchTemplateWithPWA) {
  Browser* pwa_browser =
      InstallAndLaunchPWA(GURL(kExampleUrl1), /*launch_in_browser=*/false);
  ASSERT_TRUE(pwa_browser->is_type_app());
  aura::Window* pwa_window = pwa_browser->window()->GetNativeWindow();
  const gfx::Rect pwa_bounds(50, 50, 500, 500);
  pwa_window->SetBounds(pwa_bounds);
  const int32_t pwa_window_id =
      pwa_window->GetProperty(app_restore::kWindowIdKey);
  const std::string* app_name =
      pwa_window->GetProperty(app_restore::kBrowserAppNameKey);
  ASSERT_TRUE(app_name);

  // Capture the active desk, which contains the PWA.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  std::vector<ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  // Find `pwa_browser` window's app restore data.
  ash::DeskTemplate* desk_template = templates.front();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, app_id_to_launch_list.size());
  ASSERT_TRUE(restore_data->HasAppTypeBrowser());
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(pwa_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Verify window info are correctly captured.
  EXPECT_EQ(pwa_bounds, data->current_bounds.value());
  ASSERT_TRUE(data->app_type_browser.has_value() &&
              data->app_type_browser.value());
  EXPECT_EQ(*app_name, *data->app_name);

  // Launch the template.
  ClickFirstTemplateItem();

  // Verify that the PWA was launched correctly.
  Browser* new_pwa_browser = FindLaunchedBrowserByURLs({GURL(kExampleUrl1)});
  ASSERT_TRUE(new_pwa_browser);
  ASSERT_TRUE(new_pwa_browser->is_type_app());
  aura::Window* new_browser_window =
      new_pwa_browser->window()->GetNativeWindow();
  EXPECT_NE(new_browser_window, pwa_window);
  EXPECT_EQ(pwa_bounds, new_browser_window->bounds());
  const std::string* new_app_name =
      new_browser_window->GetProperty(app_restore::kBrowserAppNameKey);
  ASSERT_TRUE(new_app_name);
  EXPECT_EQ(*app_name, *new_app_name);
}

// Tests that saving and launching a template that contains a PWA in a browser
// window works as expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUILaunchTemplateWithPWAInBrowser) {
  Browser* pwa_browser =
      InstallAndLaunchPWA(GURL(kYoutubeUrl), /*launch_in_browser=*/true);
  aura::Window* pwa_window = pwa_browser->window()->GetNativeWindow();
  const int32_t pwa_window_id =
      pwa_window->GetProperty(app_restore::kWindowIdKey);

  // Capture the active desk, which contains the PWA.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  std::vector<ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  // Test that `pwa_browser` restore data can be found.
  ash::DeskTemplate* desk_template = templates.front();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, app_id_to_launch_list.size());

  // Test that `pwa_browser`'s restore data is saved under the Chrome browser
  // app id app_constants::kChromeAppId, not Youtube app id
  // extension_misc::kYoutubeAppId.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(pwa_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());

  iter = app_id_to_launch_list.find(extension_misc::kYoutubeAppId);
  EXPECT_TRUE(iter == app_id_to_launch_list.end());
}

// Tests that browsers and SWAs can be captured correctly in the desk template.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUICaptureActiveDeskAsTemplateTest) {
  // Change `browser`'s bounds.
  const gfx::Rect browser_bounds(800, 200);
  aura::Window* window = browser()->window()->GetNativeWindow();
  window->SetBounds(browser_bounds);
  // Make the window visible on all desks.
  window->SetProperty(aura::client::kWindowWorkspaceKey,
                      aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);

  // Create the settings app, which is a system web app.
  web_app::AppId settings_app_id =
      CreateSettingsSystemWebApp(browser()->profile());

  // Change the Settings app's bounds too.
  const gfx::Rect settings_app_bounds(100, 100, 800, 300);
  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  const int32_t settings_window_id =
      settings_window->GetProperty(app_restore::kWindowIdKey);
  ASSERT_TRUE(settings_window);
  settings_window->SetBounds(settings_app_bounds);

  auto* desks_controller = ash::DesksController::Get();
  const std::u16string desk_name =
      desks_controller->GetDeskName(desks_controller->GetActiveDeskIndex());

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  std::vector<ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  ash::DeskTemplate* desk_template = templates.front();

  // Test the default template's name is the desk's name it was created from.
  EXPECT_EQ(desk_name, desk_template->template_name());

  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(2u, app_id_to_launch_list.size());

  // Find `browser` window's app restore data.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_NE(iter, app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_NE(iter->second.end(), app_restore_data_iter);
  const auto& data = app_restore_data_iter->second;
  // Verify window info are correctly captured.
  EXPECT_EQ(browser_bounds, data->current_bounds.value());
  // `visible_on_all_workspaces` should have been reset even though
  // the captured window is visible on all workspaces.
  EXPECT_FALSE(data->desk_id.has_value());
  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  // We don't capture the window's desk_id as a template will always
  // create in a new desk.
  EXPECT_FALSE(data->desk_id.has_value());

  // Find Setting app's app restore data.
  auto iter2 = app_id_to_launch_list.find(settings_app_id);
  ASSERT_NE(app_id_to_launch_list.end(), iter2);
  auto app_restore_data_iter2 = iter2->second.find(settings_window_id);
  ASSERT_NE(iter->second.end(), app_restore_data_iter2);
  const auto& data2 = app_restore_data_iter2->second;
  EXPECT_EQ(
      static_cast<int>(apps::mojom::LaunchContainer::kLaunchContainerWindow),
      data2->container.value());
  EXPECT_EQ(static_cast<int>(WindowOpenDisposition::NEW_WINDOW),
            data2->disposition.value());
  // Verify window info are correctly captured.
  EXPECT_EQ(settings_app_bounds, data2->current_bounds.value());
  EXPECT_FALSE(data2->desk_id.has_value());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_EQ(window->GetProperty(aura::client::kShowStateKey),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_FALSE(data2->desk_id.has_value());
}

// Tests that launching a template that contains a chrome app works as expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUILaunchTemplateWithChromeApp) {
  // Create a chrome app.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);

  const std::string extension_id = extension->id();
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<app_restore::AppLaunchInfo>(
          extension_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  extensions::AppWindow* app_window = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window);
  ASSERT_TRUE(GetFirstAppWindowForApp(extension_id));

  // Enter overview and save the current desk as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  // Close the chrome app window. We'll need to verify if it reopens later.
  views::Widget* app_widget =
      views::Widget::GetWidgetForNativeWindow(app_window->GetNativeWindow());
  app_widget->CloseNow();
  ASSERT_FALSE(GetFirstAppWindowForApp(extension_id));

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // `BrowserAppLauncher::LaunchAppWithParams()` does not launch the chrome app
  // in tests, so here we set up a mock app launch handler and just verify a
  // `LaunchSystemWebAppOrChromeApp()` call with the associated extension is
  // seen.
  auto mock_app_launch_handler =
      std::make_unique<MockDesksTemplatesAppLaunchHandler>(profile());
  MockDesksTemplatesAppLaunchHandler* mock_app_launch_handler_ptr =
      mock_app_launch_handler.get();
  ScopedDesksTemplatesAppLaunchHandlerSetter scoped_launch_handler(
      std::move(mock_app_launch_handler), /*launch_id=*/1);

  EXPECT_CALL(*mock_app_launch_handler_ptr,
              LaunchSystemWebAppOrChromeApp(_, extension_id, _));

  // Launch the template we saved.
  ClickFirstTemplateItem();
}

// Tests that the windows and tabs count histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUIDeskTemplateWindowAndTabCountHistogram) {
  base::HistogramTester histogram_tester;

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(browser()->profile());

  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2), GURL(kExampleUrl3)});

  // Save a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  constexpr char kWindowCountHistogramName[] = "Ash.DeskTemplate.WindowCount";
  constexpr char kTabCountHistogramName[] = "Ash.DeskTemplate.TabCount";
  constexpr char kWindowAndTabCountHistogramName[] =
      "Ash.DeskTemplate.WindowAndTabCount";
  // NOTE: there is an existing browser with 1 tab created by BrowserMain().
  histogram_tester.ExpectBucketCount(kWindowCountHistogramName, 4, 1);
  histogram_tester.ExpectBucketCount(kTabCountHistogramName, 6, 1);
  histogram_tester.ExpectBucketCount(kWindowAndTabCountHistogramName, 7, 1);
}

// Tests that the template count histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUIDeskTemplateUserTemplateCountHistogram) {
  base::HistogramTester histogram_tester;

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  auto* desks_controller = ash::DesksController::Get();
  auto active_desk_index = desks_controller->GetActiveDeskIndex();

  // Save 3 templates.
  const int saves = 3;
  for (int i = 0; i < saves; i++) {
    ClickSaveDeskAsTemplateButton();

    // Change desk name to avoid duplication on template name. Having duplicate
    // names invokes a workflow that involves showing and accepting the replace
    // dialog, which is unnecessary for this test.
    desks_controller->desks()[active_desk_index]->SetName(
        base::UTF8ToUTF16(base::NumberToString(i)), true);

    // Exit and renenter overview to save the next template. Once we are viewing
    // the grid we can't go back to regular overview unless we exit overview or
    // delete all the templates.
    if (i != saves - 1) {
      ash::ToggleOverview();
      ash::WaitForOverviewExitAnimation();
      ash::ToggleOverview();
      ash::WaitForOverviewEnterAnimation();
    }
  }

  views::Button* delete_button = ash::GetTemplateItemDeleteButton(/*index=*/0);
  ClickButton(delete_button);

  // Confirm deleting a template. Use a key press to accept the dialog instead
  // of a click as dialog buttons think a click generated by the event generator
  // is an accidentally click and therefore ignores it.
  views::Button* dialog_accept_button =
      ash::GetDesksTemplatesDialogAcceptButton();
  ASSERT_TRUE(dialog_accept_button);
  aura::Window* root_window =
      dialog_accept_button->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressAndReleaseKey(ui::VKEY_RETURN);

  // Wait for the model to update.
  ash::WaitForDesksTemplatesUI();

  // Save one more template.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  // Verify that all template saves and deletes are captured by the histogram.
  constexpr char kUserTemplateCountHistogramName[] =
      "Ash.DeskTemplate.UserTemplateCount";
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 1, 1);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 2, 2);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 3, 2);
}

// Tests that browser session restore isn't triggered when we launch a template
// that contains a browser window.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUIPreventBrowserSessionRestoreTest) {
  // Do not exit from test or delete the Profile* when last browser is closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      browser()->profile(), ProfileKeepAliveOrigin::kBrowserWindow);

  // Enable session service.
  SessionStartupPref pref(SessionStartupPref::LAST);
  Profile* profile = browser()->profile();
  SessionStartupPref::SetStartupPref(profile, pref);

  const int expected_tab_count = 2;
  chrome::AddTabAt(browser(), GURL(kExampleUrl2), /*index=*/-1,
                   /*foreground=*/true);
  EXPECT_EQ(expected_tab_count, browser()->tab_strip_model()->count());

  // Enter overview and save the current desk as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  // Exit overview, close the browser and verify that all browser windows are
  // closed.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // Reenter overview and launch the template we saved.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ash::WaitForDesksTemplatesUI();
  ClickZeroStateTemplatesButton();
  ClickFirstTemplateItem();

  // Verify that the browser was launched with the correct number of tabs, and
  // that browser session restore did not restore any windows/tabs.
  Browser* new_browser =
      FindLaunchedBrowserByURLs({GURL(kAboutBlankUrl), GURL(kExampleUrl2)});
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

// Tests that launching the same desk template multiple times creates desks with
// different/incremented names.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUILaunchMultipleDeskTemplates) {
  const base::GUID kDeskUuid = base::GUID::GenerateRandomV4();
  const std::u16string kDeskName(u"Test Desk Name");

  auto* desks_controller = ash::DesksController::Get();

  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());
  desks_controller->desks()[0]->SetName(kDeskName, true);

  // Save a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  // `ClickSaveDeskAsTemplateButton` will take us to the templates grid. For all
  // subsuquent runs, we enter the templates grid by click the templates button
  // on the desks bar.
  bool first_run = true;
  auto check_launch_template_desk_name =
      [kDeskUuid, &first_run](const std::u16string& desk_name) {
        SCOPED_TRACE(desk_name);

        if (!first_run)
          ClickExpandedStateTemplatesButton();

        ClickFirstTemplateItem();
        content::RunAllTasksUntilIdle();

        first_run = false;
      };

  // Launching a desk from the template creates a desk with the same name as
  // the template.
  desks_controller->desks()[0]->SetName(u"Desk", true);
  check_launch_template_desk_name(kDeskName);

  // Launch more desks from the template and verify that the newly create desks
  // have unique names.
  check_launch_template_desk_name(std::u16string(kDeskName).append(u"(1)"));
  check_launch_template_desk_name(std::u16string(kDeskName).append(u"(2)"));

  // Remove "Test Desk Name (1)", which means the next created desk from
  // template will have that name. Then it will skip (2) since it already
  // exists, and create the next desk with (3).
  RemoveDesk(desks_controller->desks()[2].get());
  check_launch_template_desk_name(std::u16string(kDeskName).append(u"(1)"));
  check_launch_template_desk_name(std::u16string(kDeskName).append(u"(3)"));

  // Same as above, but make sure that deleting the desk with the exact template
  // name still functions the same by only filling in whatever name is
  // available.
  RemoveDesk(desks_controller->desks()[1].get());
  check_launch_template_desk_name(kDeskName);
  check_launch_template_desk_name(std::u16string(kDeskName).append(u"(4)"));
}

// Tests that the launch from template histogram is recorded properly.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       SystemUIDeskTemplateLaunchFromTemplateHistogram) {
  base::HistogramTester histogram_tester;

  // Create a new browser.
  CreateBrowser({});

  // Save a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  const int launches = 5;
  for (int i = 0; i < launches; i++) {
    ClickFirstTemplateItem();
    ClickExpandedStateTemplatesButton();
  }

  constexpr char kLaunchFromTemplateHistogramName[] =
      "Ash.DeskTemplate.LaunchFromTemplate";
  histogram_tester.ExpectTotalCount(kLaunchFromTemplateHistogramName, launches);
}

// Tests that launching a desk template records the appropriate performance
// metric.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientTest,
                       LaunchTemplateRecordsLoadTimeMetric) {
  base::HistogramTester histogram_tester;

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(browser()->profile());

  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2), GURL(kExampleUrl3)});

  // Save and launch a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();
  ClickFirstTemplateItem();

  // Verify that the metric was recorded.
  histogram_tester.ExpectTotalCount("Ash.DeskTemplate.TimeToLoadTemplate", 1ul);
}

class DesksTemplatesClientArcTest : public InProcessBrowserTest {
 public:
  DesksTemplatesClientArcTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kDesksTemplates},
        /*disabled_features=*/{ash::features::kDeskTemplateSync});
  }
  DesksTemplatesClientArcTest(const DesksTemplatesClientArcTest&) = delete;
  DesksTemplatesClientArcTest& operator=(const DesksTemplatesClientArcTest&) =
      delete;
  ~DesksTemplatesClientArcTest() override = default;

  ash::AppRestoreArcTestHelper* arc_helper() { return &arc_helper_; }

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc_helper_.SetUpCommandLine(command_line);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc_helper_.SetUpInProcessBrowserTestFixture();
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    arc_helper_.SetUpOnMainThread(browser()->profile());
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  ash::AppRestoreArcTestHelper arc_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that launching a template that contains an ARC app works as expected.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientArcTest,
                       SystemUILaunchTemplateWithArcApp) {
  auto* desk_model = DesksTemplatesClient::Get()->GetDeskModel();
  ASSERT_EQ(0u, desk_model->GetEntryCount());

  constexpr char kTestAppPackage[] = "test.arc.app.package";
  arc_helper()->InstallTestApps(kTestAppPackage, /*multi_app=*/false);
  const std::string app_id = ash::GetTestApp1Id(kTestAppPackage);

  int32_t session_id1 =
      full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();

  // Create the window for app1. The task id needs to match the
  // `window_app_id` arg of `CreateExoWindow`.
  const int32_t kTaskId1 = 100;
  views::Widget* widget = ash::CreateExoWindow("org.chromium.arc.100");
  widget->SetBounds(gfx::Rect(500, 500));
  full_restore::SaveAppLaunchInfo(browser()->profile()->GetPath(),
                                  std::make_unique<app_restore::AppLaunchInfo>(
                                      app_id, ui::EventFlags::EF_NONE,
                                      session_id1, display::kDefaultDisplayId));

  // Simulate creating the task.
  arc_helper()->CreateTask(app_id, kTaskId1, session_id1);

  // Enter overview and save the current desk as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();
  ASSERT_EQ(1u, desk_model->GetEntryCount());

  // Exit overview and close the Arc window. We'll need to verify if it
  // reopens later.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();
  widget->CloseNow();
  arc_helper()->GetAppHost()->OnTaskDestroyed(kTaskId1);

  // Enter overview, head over to the desks templates grid and launch the
  // template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickZeroStateTemplatesButton();
  ClickFirstTemplateItem();

  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();

  // Create the window to simulate launching the ARC app.
  const int32_t kTaskId2 = 200;
  auto* widget1 = ash::CreateExoWindow("org.chromium.arc.200");
  auto* window1 = widget1->GetNativeWindow();
  arc_helper()->CreateTask(app_id, kTaskId2, session_id1);

  // Tests that the ARC app is launched on desk 2.
  EXPECT_EQ(ash::Shell::GetContainer(window1->GetRootWindow(),
                                     ash::kShellWindowId_DeskContainerB),
            window1->parent());

  widget1->CloseNow();
  arc_helper()->GetAppHost()->OnTaskDestroyed(kTaskId2);
  arc_helper()->StopInstance();
}

class DesksTemplatesClientPolicyTest : public policy::PolicyTest {
 public:
  void SetDeskTemplateEnabledPolicy(bool policy_value) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kDeskTemplatesEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(policy_value),
                 nullptr);
    UpdateProviderPolicy(policies);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DesksTemplatesClientPolicyWithFeatureEnabledTest
    : public DesksTemplatesClientPolicyTest {
 public:
  DesksTemplatesClientPolicyWithFeatureEnabledTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(ash::features::kDesksTemplates);
  }
};

// Tests that the desks templates feature should be controlled by policy.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientPolicyWithFeatureEnabledTest,
                       CanBeControlledByPolicy) {
  const PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();

  EXPECT_FALSE(
      prefs->FindPreference(ash::prefs::kDeskTemplatesEnabled)->IsManaged());

  // Without setting up the enterprise policy, desk templates feature is
  // controlled by feature flag, which is enabled in this test.
  EXPECT_TRUE(ash::desks_templates_util::AreDesksTemplatesEnabled());

  // Disable desk templates through policy.
  SetDeskTemplateEnabledPolicy(false);
  // Desk templates feature should be disabled, despite feature flag is set to
  // enabled.
  EXPECT_FALSE(ash::desks_templates_util::AreDesksTemplatesEnabled());

  // Enable desk templates through policy.
  SetDeskTemplateEnabledPolicy(true);
  // Desk templates feature should be enabled.
  EXPECT_TRUE(ash::desks_templates_util::AreDesksTemplatesEnabled());
}

class DesksTemplatesClientPolicyWithFeatureDisabledTest
    : public DesksTemplatesClientPolicyTest {
 public:
  DesksTemplatesClientPolicyWithFeatureDisabledTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.Init();
  }
};

// Tests that the desks templates feature should be controlled by policy.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientPolicyWithFeatureDisabledTest,
                       CanBeControlledByPolicy) {
  const PrefService* prefs = chrome_test_utils::GetProfile(this)->GetPrefs();

  EXPECT_FALSE(
      prefs->FindPreference(ash::prefs::kDeskTemplatesEnabled)->IsManaged());

  // Without setting up the enterprise policy, desk templates feature is
  // controlled by feature flag, which is disabled in this test.
  EXPECT_FALSE(ash::desks_templates_util::AreDesksTemplatesEnabled());

  // Disable desk templates through policy.
  SetDeskTemplateEnabledPolicy(false);
  // Desk templates feature should be disabled.
  EXPECT_FALSE(ash::desks_templates_util::AreDesksTemplatesEnabled());

  // Enable desk templates through policy.
  SetDeskTemplateEnabledPolicy(true);
  // Desk templates feature should be enabled, despite feature flag is set to
  // disabled.
  EXPECT_TRUE(ash::desks_templates_util::AreDesksTemplatesEnabled());
}

class DesksTemplatesClientMultiProfileTest : public ash::LoginManagerTest {
 public:
  DesksTemplatesClientMultiProfileTest() : ash::LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{ash::features::kDeskTemplateSync});
  }
  ~DesksTemplatesClientMultiProfileTest() override = default;

  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();

    LoginUser(account_id1_);
    ::full_restore::SetActiveProfilePath(
        ash::ProfileHelper::Get()
            ->GetProfileByAccountId(account_id1_)
            ->GetPath());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId account_id1_;
  AccountId account_id2_;
};

IN_PROC_BROWSER_TEST_F(DesksTemplatesClientMultiProfileTest, MultiProfileTest) {
  CreateBrowser(ash::ProfileHelper::Get()->GetProfileByAccountId(account_id1_));
  // Capture the active desk, which contains the browser windows.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, app_id_to_launch_list.size());

  EXPECT_EQ(1u, GetDeskTemplates().size());

  // Now switch to |account_id2_|. Test that the captured desk template can't
  // be accessed from |account_id2_|.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  EXPECT_EQ(0u, GetDeskTemplates().size());
}

// Tests that admin templates policy can be set.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientMultiProfileTest,
                       SetAndClearAdminTemplates) {
  EXPECT_TRUE(DesksTemplatesClient::Get());

  // Set an admin template policy.
  DesksTemplatesClient::Get()->SetPolicyPreconfiguredTemplate(
      account_id1_, std::make_unique<std::string>(base::StringPrintf(
                        kTestAdminTemplateFormat, kTestAdminTemplateUuid)));

  // Verify that the admin templates is present.
  EXPECT_TRUE(
      ContainUuidInTemplates(kTestAdminTemplateUuid, GetDeskTemplates()));

  // Clear admin templates.
  DesksTemplatesClient::Get()->RemovePolicyPreconfiguredTemplate(account_id1_);

  // Verify that the admin templates is removed.
  EXPECT_FALSE(
      ContainUuidInTemplates(kTestAdminTemplateUuid, GetDeskTemplates()));
}
