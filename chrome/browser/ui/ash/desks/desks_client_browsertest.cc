// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "base/value_iterators.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_test_helper.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/chrome_desks_util.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/desks/desks_templates_app_launch_handler.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/desk.mojom-shared.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#include "chromeos/ui/wm/features.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/policy_constants.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_info.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ElementsAre;

namespace {

constexpr int32_t kSettingsWindowId = 100;
constexpr int32_t kHelpWindowId = 200;
constexpr int32_t kLaunchedWindowIdBase = -1;
constexpr int32_t kTestWindowId = 300;

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
constexpr char kTestTabGroupNameFormat[] = "test_tab_group_%u";
constexpr char kTestAppName[] = "test_app_name";

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

// Locate all browsers launched from templates whose URLs match `urls`.
std::vector<Browser*> FindLaunchedBrowsersByURLs(
    const std::vector<GURL>& urls) {
  std::vector<Browser*> browsers;
  for (auto* browser : *BrowserList::GetInstance()) {
    aura::Window* window = browser->window()->GetNativeWindow();
    if (window->GetProperty(app_restore::kRestoreWindowIdKey) <
            kLaunchedWindowIdBase &&
        GetURLsForBrowserWindow(browser) == urls) {
      browsers.push_back(browser);
    }
  }
  return browsers;
}

// Locate a browser launched from a template whose URLs match `urls`.
Browser* FindLaunchedBrowserByURLs(const std::vector<GURL>& urls) {
  auto browsers = FindLaunchedBrowsersByURLs(urls);
  return browsers.empty() ? nullptr : browsers.front();
}

// TODO(crbug.com/1286515): Remove this. Tests should navigate to overview and
// click the button using an event generator.
std::unique_ptr<ash::DeskTemplate> CaptureActiveDeskAndSaveTemplate(
    ash::DeskTemplateType desk_template_type) {
  base::RunLoop run_loop;
  std::unique_ptr<ash::DeskTemplate> desk_template;
  DesksClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindLambdaForTesting(
          [&](absl::optional<DesksClient::DeskActionError> error,
              std::unique_ptr<ash::DeskTemplate> captured_desk_template) {
            run_loop.Quit();
            ASSERT_TRUE(captured_desk_template);
            desk_template = std::move(captured_desk_template);
          }),
      desk_template_type);
  run_loop.Run();
  return desk_template;
}

std::vector<const ash::DeskTemplate*> GetDeskTemplates() {
  base::RunLoop run_loop;
  std::vector<const ash::DeskTemplate*> templates;

  DesksClient::Get()->GetDeskTemplates(base::BindLambdaForTesting(
      [&](absl::optional<DesksClient::DeskActionError> error,
          const std::vector<const ash::DeskTemplate*>& desk_templates) {
        templates = desk_templates;
        run_loop.Quit();
      }));
  run_loop.Run();

  return templates;
}

// Search `desk_templates` for a template with `uuid` and returns true if found,
// false if not.
bool ContainUuidInTemplates(
    const base::Uuid& uuid,
    const std::vector<const ash::DeskTemplate*>& desk_templates) {
  DCHECK(uuid.is_valid());
  return base::Contains(desk_templates, uuid, &ash::DeskTemplate::uuid);
}

std::string GetTemplateJson(const base::Uuid& uuid, Profile* profile) {
  base::RunLoop run_loop;
  std::string template_json_result;
  DesksClient::Get()->GetTemplateJson(
      uuid, profile,
      base::BindLambdaForTesting(
          [&](absl::optional<DesksClient::DeskActionError> error,
              const base::Value& template_json) {
            base::JSONWriter::Write(template_json, &template_json_result);
            run_loop.Quit();
            ASSERT_FALSE(error);
          }));
  run_loop.Run();
  return template_json_result;
}

void DeleteDeskTemplate(const base::Uuid& uuid) {
  base::RunLoop run_loop;
  DesksClient::Get()->DeleteDeskTemplate(
      uuid, base::BindLambdaForTesting(
                [&](absl::optional<DesksClient::DeskActionError> error) {
                  run_loop.Quit();
                }));
  run_loop.Run();
}

web_app::AppId CreateSystemWebApp(Profile* profile,
                                  ash::SystemWebAppType app_type) {
  DCHECK(app_type == ash::SystemWebAppType::SETTINGS ||
         app_type == ash::SystemWebAppType::HELP);
  web_app::AppId app_id = *ash::GetAppIdForSystemWebApp(profile, app_type);
  apps::AppLaunchParams params(
      app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
  params.restore_id = app_type == ash::SystemWebAppType::SETTINGS
                          ? kSettingsWindowId
                          : kHelpWindowId;

  base::RunLoop launch_wait;
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithParams(
      std::move(params),
      base::BindLambdaForTesting(
          [&](apps::LaunchResult&& result) { launch_wait.Quit(); }));
  launch_wait.Run();
  return app_id;
}

web_app::AppId CreateSettingsSystemWebApp(Profile* profile) {
  return CreateSystemWebApp(profile, ash::SystemWebAppType::SETTINGS);
}

web_app::AppId CreateHelpSystemWebApp(Profile* profile) {
  return CreateSystemWebApp(profile, ash::SystemWebAppType::HELP);
}

void ClickButton(const views::Button* button) {
  DCHECK(button);
  DCHECK(button->GetVisible());
  aura::Window* root_window =
      button->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseToInHost(button->GetBoundsInScreen().CenterPoint());
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
    ash::WaitForSavedDeskUI();
  // Clicking the save template button selects the newly created template's name
  // field. We can press enter or escape or click to select out of it.
  ash::SendKey(ui::VKEY_RETURN);
}

void ClickSaveDeskAsTemplateButton() {
  ClickSaveDeskAsTemplateButton(/*wait_for_ui=*/true);
}

void ClickSaveDeskForLaterButton() {
  views::Button* save_desk_for_later_button = ash::GetSaveDeskForLaterButton();
  DCHECK(save_desk_for_later_button);
  ClickButton(save_desk_for_later_button);
}

void ClickZeroStateTemplatesButton() {
  views::Button* zero_state_templates_button = ash::GetZeroStateLibraryButton();
  ASSERT_TRUE(zero_state_templates_button);
  ClickButton(zero_state_templates_button);
}

void ClickExpandedStateTemplatesButton() {
  views::Button* expanded_state_templates_button =
      ash::GetExpandedStateLibraryButton();
  ASSERT_TRUE(expanded_state_templates_button);
  ClickButton(expanded_state_templates_button);
}

void ClickFirstTemplateItem() {
  views::Button* template_item = ash::GetSavedDeskItemButton(/*index=*/0);
  DCHECK(template_item);
  ClickButton(template_item);
}

const std::vector<const ash::DeskTemplate*> GetAllEntries() {
  std::vector<const ash::DeskTemplate*> templates;
  auto error = DesksClient::Get()->GetDeskModel()->GetAllEntries();
  DCHECK_EQ(desks_storage::DeskModel::GetAllEntriesStatus::kOk, error.status);
  return error.entries;
}

// Creates a vector of tab groups based on the vector of GURLs passed into it.
// A tab group will be created for each individual tab.
std::vector<tab_groups::TabGroupInfo> MakeExpectedTabGroupsBasedOnTabVector(
    const std::vector<GURL>& urls) {
  std::vector<tab_groups::TabGroupInfo> tab_groups;

  for (uint32_t index = 0; index < urls.size(); ++index) {
    tab_groups.emplace_back(
        gfx::Range(index, index + 1),
        tab_groups::TabGroupVisualData(
            base::UTF8ToUTF16(
                base::StringPrintf(kTestTabGroupNameFormat, index)),
            tab_groups::TabGroupColorId(
                static_cast<tab_groups::TabGroupColorId>(index % 8))));
  }

  return tab_groups;
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

class BrowsersAddedObserver : public BrowserListObserver {
 public:
  explicit BrowsersAddedObserver(int num_browser_expected)
      : num_browser_adds_left_(num_browser_expected) {
    BrowserList::AddObserver(this);
  }
  BrowsersAddedObserver(const BrowsersAddedObserver&) = delete;
  BrowsersAddedObserver& operator=(const BrowsersAddedObserver&) = delete;
  ~BrowsersAddedObserver() override { BrowserList::RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    --num_browser_adds_left_;
    if (num_browser_adds_left_ == 0)
      run_loop_.Quit();
  }

  void OnBrowserRemoved(Browser* browser) override {}

 private:
  int num_browser_adds_left_;
  base::RunLoop run_loop_;
};

class BrowsersRemovedObserver : public BrowserListObserver {
 public:
  explicit BrowsersRemovedObserver(int browser_removes_expected)
      : browser_removes_left_(browser_removes_expected) {
    BrowserList::AddObserver(this);
  }
  BrowsersRemovedObserver(const BrowsersRemovedObserver&) = delete;
  BrowsersRemovedObserver& operator=(const BrowsersRemovedObserver&) = delete;
  ~BrowsersRemovedObserver() override { BrowserList::RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {}

  void OnBrowserRemoved(Browser* browser) override {
    --browser_removes_left_;
    if (browser_removes_left_ == 0)
      run_loop_.Quit();
  }

 private:
  int browser_removes_left_;
  base::RunLoop run_loop_;
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

    DesksClient* desks_client = DesksClient::Get();
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

    DesksClient* desks_client = DesksClient::Get();
    DCHECK(desks_client);
    desks_client->app_launch_handlers_[launch_id_] = nullptr;
  }

 private:
  // Variable to ensure we never have more than one instance of this object.
  inline static bool instance_active_ = false;

  // The id of the launch used for testing.
  int launch_id_;
};

class DesksClientTest : public extensions::PlatformAppBrowserTest {
 public:
  DesksClientTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kDesksTemplates,
                              chromeos::wm::features::kWindowLayoutMenu},
        /*disabled_features=*/{ash::features::kDeskTemplateSync});

    // Suppress the multitask menu nudge as we'll be checking the stacking order
    // and the count of the active desk children.
    chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(true);
  }
  DesksClientTest(const DesksClientTest&) = delete;
  DesksClientTest& operator=(const DesksClientTest&) = delete;
  ~DesksClientTest() override = default;

  // TODO(crbug.com/1286515): These functions will be removed with the
  // extension. Avoid further uses of this method and create or launch templates
  // by mocking clicks on the system UI.
  void SetTemplate(std::unique_ptr<ash::DeskTemplate> launch_template) {
    DesksClient::Get()->launch_template_for_test_ = std::move(launch_template);
  }

  void LaunchTemplate(const base::Uuid& uuid) {
    base::RunLoop waiter;
    DesksClient::Get()->LaunchDeskTemplate(
        uuid, base::BindLambdaForTesting(
                  [&](absl::optional<DesksClient::DeskActionError> error,
                      const base::Uuid& desk_uuid) { waiter.Quit(); }));
    waiter.Run();
  }

  void SetAndLaunchTemplate(std::unique_ptr<ash::DeskTemplate> desk_template) {
    ash::DeskTemplate* desk_template_ptr = desk_template.get();
    SetTemplate(std::move(desk_template));
    LaunchTemplate(desk_template_ptr->uuid());
  }

  Browser* CreateBrowser(
      const std::vector<GURL>& urls,
      absl::optional<size_t> active_url_index = absl::nullopt) {
    Browser* browser = CreateBrowserImpl(urls, active_url_index);
    browser->window()->Show();
    return browser;
  }

  Browser* CreateBrowserWithPinnedTabs(const std::vector<GURL>& urls,
                                       int first_non_pinned_tab_index) {
    Browser* browser = CreateBrowserImpl(urls, absl::nullopt);

    chrome_desks_util::SetBrowserPinnedTabs(first_non_pinned_tab_index,
                                            browser);
    browser->window()->Show();
    return browser;
  }

  Browser* CreateBrowserWithTabGroups(
      const std::vector<GURL>& urls,
      const std::vector<tab_groups::TabGroupInfo>& tab_groups) {
    Browser* browser = CreateBrowserImpl(urls, absl::nullopt);

    chrome_desks_util::AttachTabGroupsToBrowserInstance(tab_groups, browser);
    browser->window()->Show();
    return browser;
  }

  Browser* InstallAndLaunchPWA(const GURL& start_url, bool launch_in_browser) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    if (!launch_in_browser)
      web_app_info->user_display_mode =
          web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    const web_app::AppId app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));

    return launch_in_browser
               ? web_app::LaunchBrowserForWebAppInTab(profile(), app_id)
               : web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  }

  // This navigates the browser to a page that will show a close confirmation
  // dialog when closed.
  void SetupBrowserToConfirmClose(Browser* browser) {
    std::string page_that_requires_close_confirmation =
        "<html><head>"
        "<script>window.onbeforeunload = function() { return \"x\"; };</script>"
        "</head><body></body></html>";

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser,
        GURL("data:text/html, " + page_that_requires_close_confirmation)));

    // Note that the `onbeforeunload` handler will not run for a page that
    // hasn't been interacted with. To meet that requirement, we'll click on the
    // page.
    aura::Window* window = browser->window()->GetNativeWindow();
    ui::test::EventGenerator event_generator(window->GetRootWindow());
    event_generator.MoveMouseToInHost(
        window->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
  }

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    ::full_restore::SetActiveProfilePath(profile()->GetPath());
    ash::SystemWebAppManager::GetForTest(profile())
        ->InstallSystemAppsForTesting();
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
  }

 private:
  Browser* CreateBrowserImpl(const std::vector<GURL>& urls,
                             absl::optional<size_t> active_url_index) {
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
    return browser;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a browser's urls can be captured correctly in the desk template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureBrowserUrlsTest) {
  // Create a new browser and add a few tabs to it.
  Browser* browser = CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);
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
  EXPECT_EQ(data->urls, urls);
}

// Tests that a browser's tab groups can be captured correctly in a saved desk.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureBrowserTabGroupsTest) {
  std::vector<GURL> tabs = {GURL(kExampleUrl1), GURL(kExampleUrl2)};
  std::vector<tab_groups::TabGroupInfo> expected_tab_groups =
      MakeExpectedTabGroupsBasedOnTabVector(tabs);

  // Create a new browser and add a few tabs to it.
  Browser* browser = CreateBrowserWithTabGroups(tabs, expected_tab_groups);
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  std::vector<const ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  const ash::DeskTemplate* desk_template = templates.front();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, app_id_to_launch_list.size());

  // Find `browser` window's app restore data.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Check the urls are captured correctly in the `desk_template`.
  EXPECT_EQ(urls, data->urls);

  // We don't care about the order of the tab groups.
  EXPECT_THAT(expected_tab_groups,
              testing::UnorderedElementsAreArray(data->tab_group_infos));
}

// Tests that a browser's pinned tabs can be captured correctly in a saved desk.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureBrowserWithPinnedTabs) {
  std::vector<GURL> tabs = {GURL(kExampleUrl1), GURL(kExampleUrl2)};
  int expected_number_of_pinned_tabs = 1;

  // Create a new browser and add a few tabs to it.
  Browser* browser =
      CreateBrowserWithPinnedTabs(tabs, expected_number_of_pinned_tabs);
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  std::vector<const ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  const ash::DeskTemplate* desk_template = templates.front();
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  const auto& app_id_to_launch_list = restore_data->app_id_to_launch_list();
  EXPECT_EQ(1u, app_id_to_launch_list.size());

  // Find `browser` window's app restore data.
  auto iter = app_id_to_launch_list.find(app_constants::kChromeAppId);
  ASSERT_TRUE(iter != app_id_to_launch_list.end());
  auto app_restore_data_iter = iter->second.find(browser_window_id);
  ASSERT_TRUE(app_restore_data_iter != iter->second.end());
  const auto& data = app_restore_data_iter->second;
  // Check the urls are captured correctly in the `desk_template`.
  EXPECT_EQ(urls, data->urls);

  // Assert number of pinned tabs is correct.
  EXPECT_TRUE(data->first_non_pinned_tab_index.has_value());
  EXPECT_THAT(expected_number_of_pinned_tabs,
              data->first_non_pinned_tab_index.value());
}

// Tests that incognito browser windows will NOT be captured in the desk
// template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureIncognitoBrowserTest) {
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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);
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
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureActiveDeskAsTemplateTest) {
  // Test that Singleton was properly initialized.
  ASSERT_TRUE(DesksClient::Get());

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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);
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
  EXPECT_EQ(static_cast<int>(apps::LaunchContainer::kLaunchContainerWindow),
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
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchMultipleDeskTemplates) {
  const base::Uuid kDeskUuid = base::Uuid::GenerateRandomV4();
  const std::u16string kDeskName(u"Test Desk Name");

  auto* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // TODO(crbug.com/1273532): Note that `SetTemplate` allows setting an empty
  // desk template which shouldn't be possible in a real workflow. Make sure a
  // non empty desks are launched when this test is updated to use the real
  // workflow.
  auto desk_template = std::make_unique<ash::DeskTemplate>(
      kDeskUuid, ash::DeskTemplateSource::kUser, base::UTF16ToUTF8(kDeskName),
      base::Time::Now(), ash::DeskTemplateType::kTemplate);
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
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithSystemApp) {
  ASSERT_TRUE(DesksClient::Get());

  // Create the settings app, which is a system web app.
  CreateSettingsSystemWebApp(browser()->profile());

  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  ASSERT_TRUE(settings_window);
  const std::u16string settings_title = settings_window->GetTitle();

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);
  // Close the settings window. We'll need to verify if it reopens later.
  views::Widget* settings_widget =
      views::Widget::GetWidgetForNativeWindow(settings_window);
  settings_widget->CloseNow();
  ASSERT_FALSE(FindBrowserWindow(kSettingsWindowId));
  settings_window = nullptr;

  auto* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Set the template we created as the template we want to launch.
  BrowsersAddedObserver browsers_added(/*num_browser_expected=*/2);
  SetAndLaunchTemplate(std::move(desk_template));
  browsers_added.Wait();

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
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithSystemAppExisting) {
  ASSERT_TRUE(DesksClient::Get());
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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);

  // Move the settings window to a new place and stack it on top so that we can
  // later verify that it has been placed and stacked correctly.
  settings_window->SetBounds(gfx::Rect(150, 150, 650, 500));
  settings_window->Focus();

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Set the template we created as the template we want to launch.
  BrowsersAddedObserver browsers_added(/*num_browser_expected=*/1);
  SetAndLaunchTemplate(std::move(desk_template));
  browsers_added.Wait();

  // We launch a new browser window, but not a new settings app. Verify that the
  // window has been moved to the right place and stacked at the bottom.
  EXPECT_EQ(3u, BrowserList::GetInstance()->size());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(settings_window));
  EXPECT_EQ(settings_bounds, settings_window->bounds());
  ASSERT_THAT(settings_window->parent()->children(),
              ElementsAre(settings_window, _));
}

// Tests that launching a template that contains a chrome app works as expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithChromeApp) {
  DesksClient* desks_client = DesksClient::Get();
  ASSERT_TRUE(desks_client);

  // Create a chrome app.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);

  const std::string extension_id = extension->id();
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<app_restore::AppLaunchInfo>(
          extension_id, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  extensions::AppWindow* app_window = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window);
  ASSERT_TRUE(GetFirstAppWindowForApp(extension_id));

  // Capture the active desk, which contains the chrome app.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);
  ASSERT_TRUE(desk_template);

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

  // Set the template we created as the template we want to launch.
  SetAndLaunchTemplate(std::move(desk_template));
}

// Tests that launching a template that contains a browser window works as
// expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithBrowserWindow) {
  ASSERT_TRUE(DesksClient::Get());

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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);
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

// Tests that launching a template that contains a floated browser window works
// as expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithFloatedWindow) {
  // Test that Singleton was properly initialized.
  ASSERT_TRUE(DesksClient::Get());

  // Float browser window and move out from default location.
  const gfx::Rect browser_bounds = gfx::Rect(0, 0, 800, 200);
  aura::Window* window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  event_generator.PressAndReleaseKey(ui::VKEY_F,
                                     ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(ash::WindowState::Get(window)->IsFloated());
  window->SetBounds(browser_bounds);
  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);

  // Test the default template's name is the current desk's name.
  auto* desks_controller = ash::DesksController::Get();
  EXPECT_EQ(
      desk_template->template_name(),
      desks_controller->GetDeskName(desks_controller->GetActiveDeskIndex()));

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
  // Verify floated window bounds is correctly captured.
  EXPECT_EQ(browser_bounds, data->current_bounds.value());
  // Verify window float state is correctly captured.
  EXPECT_EQ(chromeos::WindowStateType::kFloated, data->window_state_type);

  // Launch saved template and test floated window is restored correctly.
  SetAndLaunchTemplate(std::move(desk_template));
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Get the floated window from newly created desk.
  auto* floated_window = ash::window_util::GetFloatedWindowForActiveDesk();
  ASSERT_TRUE(floated_window);
  ASSERT_TRUE(ash::WindowState::Get(floated_window)->IsFloated());
  // Restored floated window to the saved bounds instead of default bounds.
  EXPECT_EQ(floated_window->bounds(), browser_bounds);
}

// Tests that launching a template that contains a browser window with tab
// groups works as expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest,
                       LaunchTemplateWithBrowserWindowTabGroups) {
  ASSERT_TRUE(DesksClient::Get());

  // Create a new browser and add a few tabs to it, and specify the active tab
  // index.
  std::vector<GURL> creation_urls = {GURL(kExampleUrl1), GURL(kExampleUrl2),
                                     GURL(kExampleUrl3)};
  std::vector<tab_groups::TabGroupInfo> expected_tab_groups =
      MakeExpectedTabGroupsBasedOnTabVector(creation_urls);

  Browser* browser =
      CreateBrowserWithTabGroups(creation_urls, expected_tab_groups);

  // Get current tabs from browser.
  const std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  // Enter overview and save the current desk as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  ClickFirstTemplateItem();

  // Exit overview.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();

  // Wait for the tabs to load.
  content::RunAllTasksUntilIdle();

  // Verify that the browser was launched with the correct urls and active tab.
  Browser* new_browser = FindLaunchedBrowserByURLs(urls);
  ASSERT_TRUE(new_browser);

  std::vector<tab_groups::TabGroupInfo> got_tab_groups =
      chrome_desks_util::ConvertTabGroupsToTabGroupInfos(
          new_browser->tab_strip_model()->group_model());

  EXPECT_FALSE(got_tab_groups.empty());

  // We don't care about the order of the tab groups.
  EXPECT_THAT(expected_tab_groups,
              testing::UnorderedElementsAreArray(got_tab_groups));
}

// Tests that a browser's pinned tabs can be launched correctly in a saved desk.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchBrowserWithPinnedTabs) {
  ASSERT_TRUE(DesksClient::Get());

  // create expected values.
  std::vector<GURL> tabs = {GURL(kExampleUrl1), GURL(kExampleUrl2)};
  int expected_number_of_pinned_tabs = 1;

  // Create a new browser and add a few tabs to it.
  Browser* browser =
      CreateBrowserWithPinnedTabs(tabs, expected_number_of_pinned_tabs);

  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  ClickFirstTemplateItem();

  // Exit overview.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();

  // Wait for tabs to load.
  content::RunAllTasksUntilIdle();

  // Verify that the browser was launched with the correct urls and active tab.
  Browser* new_browser = FindLaunchedBrowserByURLs(urls);
  ASSERT_TRUE(new_browser);

  // Assert the number of pinned tabs is correct.
  ASSERT_EQ(expected_number_of_pinned_tabs,
            new_browser->tab_strip_model()->IndexOfFirstNonPinnedTab());
}

// Tests that browser session restore isn't triggered when we launch a template
// that contains a browser window.
IN_PROC_BROWSER_TEST_F(DesksClientTest, PreventBrowserSessionRestoreTest) {
  ASSERT_TRUE(DesksClient::Get());

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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);
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

// Tests that browser windows created from a template have the correct bounds
// and window state.
IN_PROC_BROWSER_TEST_F(DesksClientTest, BrowserWindowRestorationTest) {
  ASSERT_TRUE(DesksClient::Get());

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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);

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
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithPWA) {
  ASSERT_TRUE(DesksClient::Get());

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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);

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

// Tests that PWAs with out of scope urls are saved and launched correctly.
// Regression test for b/248645623.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithOutOfScopeURL) {
  ASSERT_TRUE(DesksClient::Get());

  Browser* pwa_browser =
      InstallAndLaunchPWA(GURL(kYoutubeUrl), /*launch_in_browser=*/false);
  ASSERT_TRUE(pwa_browser->is_type_app());
  aura::Window* pwa_window = pwa_browser->window()->GetNativeWindow();
  const std::string* app_name =
      pwa_window->GetProperty(app_restore::kBrowserAppNameKey);
  ASSERT_TRUE(app_name);

  const GURL out_of_scope_url(kExampleUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(pwa_browser, out_of_scope_url));
  // Verify that the PWA has navigated to the out of scope url.
  const std::vector<GURL> urls = GetURLsForBrowserWindow(pwa_browser);
  ASSERT_THAT(urls, ElementsAre(out_of_scope_url));

  // Set the template and launch it.
  SetAndLaunchTemplate(
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate));

  // Verify that the PWA was launched correctly, and that the out of scope url
  // was successfully opened in the PWA (and not in a normal browser window).
  Browser* new_pwa_browser = FindLaunchedBrowserByURLs(urls);
  ASSERT_TRUE(new_pwa_browser);
  ASSERT_TRUE(new_pwa_browser->is_type_app());
  aura::Window* new_browser_window =
      new_pwa_browser->window()->GetNativeWindow();
  EXPECT_NE(new_browser_window, pwa_window);
  const std::string* new_app_name =
      new_browser_window->GetProperty(app_restore::kBrowserAppNameKey);
  ASSERT_TRUE(new_app_name);
  EXPECT_EQ(*app_name, *new_app_name);
}

// Tests that saving and launching a template that contains a PWA in a browser
// window works as expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateWithPWAInBrowser) {
  ASSERT_TRUE(DesksClient::Get());

  Browser* pwa_browser =
      InstallAndLaunchPWA(GURL(kYoutubeUrl), /*launch_in_browser=*/true);
  aura::Window* pwa_window = pwa_browser->window()->GetNativeWindow();
  const int32_t pwa_window_id =
      pwa_window->GetProperty(app_restore::kWindowIdKey);

  // Capture the active desk, which contains the PWA.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);

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
IN_PROC_BROWSER_TEST_F(DesksClientTest, GetDeskTemplateJson) {
  // Test that Singleton was properly initialized.
  ASSERT_TRUE(DesksClient::Get());

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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);

  std::string template_json =
      GetTemplateJson(desk_template->uuid(), browser()->profile());

  // content of the conversion is tested in:
  // components/desks_storage/core/desk_template_conversion_unittests.cc in this
  // case we're simply interested in whether or not we got content back.
  ASSERT_TRUE(!template_json.empty());
}

// Tests that basic operations using the native UI work as expected.
// TODO(crbug.com/1286515): Remove the SystemUI prefix from these tests. Remove
// the tests that do not have the SystemUI prefix other than GetDeskTemplateJson
// once the extension is deprecated.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUIBasic) {
  auto* desk_model = DesksClient::Get()->GetDeskModel();
  ASSERT_EQ(0u, desk_model->GetEntryCount());

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  // Tests that since we have no saved desk right now, so the library button is
  // hidden.
  views::Button* zero_state_templates_button = ash::GetZeroStateLibraryButton();
  ASSERT_TRUE(zero_state_templates_button);
  EXPECT_FALSE(zero_state_templates_button->GetVisible());

  // Note that this button needs at least one window to show up. Browser tests
  // have an existing browser window, so no new window needs to be created.
  views::Button* save_desk_as_template_button =
      ash::GetSaveDeskAsTemplateButton();
  ASSERT_TRUE(save_desk_as_template_button);
  ClickButton(save_desk_as_template_button);

  ash::WaitForSavedDeskUI();

  EXPECT_EQ(1u, desk_model->GetEntryCount());

  // Tests that since we have one template right now, so that the expanded state
  // library button is shown, and the saved desk grid has one item.
  auto* expanded_state_templates_button = ash::GetExpandedStateLibraryButton();
  ASSERT_TRUE(expanded_state_templates_button);
  EXPECT_TRUE(expanded_state_templates_button->GetVisible());

  views::Button* template_item = ash::GetSavedDeskItemButton(/*index=*/0);
  EXPECT_TRUE(template_item);
}

// Tests launching a template with a browser window.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUILaunchBrowser) {
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

  // Exit overview.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();

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
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUICaptureBrowserUrlsTest) {
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

  std::vector<const ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  const ash::DeskTemplate* desk_template = templates.front();
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
  EXPECT_EQ(data->urls, urls);
}

// Tests that snapped window's snap ratio/percentage is maintained when
// launching a desk template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUILaunchSnappedWindow) {
  const int shelf_height = ash::ShelfConfig::Get()->shelf_size();
  display::test::DisplayManagerTestApi display_manager_test_api(
      ash::Shell::Get()->display_manager());
  display_manager_test_api.UpdateDisplay(
      "2000x" + base::NumberToString(1000 + shelf_height));

  aura::Window* window = browser()->window()->GetNativeWindow();

  // Snap the window to the left.
  const ash::WMEvent left_snap_event(ash::WM_EVENT_SNAP_PRIMARY);
  ash::WindowState::Get(window)->OnWMEvent(&left_snap_event);
  ASSERT_EQ(gfx::Rect(1000, 1000), window->GetBoundsInScreen());

  // Drag the window so it is now 60% of the work area.
  ui::test::EventGenerator event_generator(window->GetRootWindow());
  event_generator.set_current_screen_location(gfx::Point(1000, 500));
  event_generator.DragMouseBy(200, 0);
  ASSERT_EQ(gfx::Rect(1200, 1000), window->GetBoundsInScreen());

  // Enter overview and save our snapped window as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  // Launch our template and then exit overview.
  ClickFirstTemplateItem();
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();

  // Our snapped window should have the similar bounds as it did when it was
  // saved. We may lose some precision when saving a float as a percentage.
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
  aura::Window* new_browser_window =
      BrowserList::GetInstance()->get(1)->window()->GetNativeWindow();
  gfx::Rect new_bounds = new_browser_window->GetBoundsInScreen();
  EXPECT_EQ(0, new_bounds.x());
  EXPECT_EQ(0, new_bounds.y());
  EXPECT_NEAR(1200, new_bounds.width(), 5);
  EXPECT_EQ(1000, new_bounds.height());

  // Launches the first template on the template grid.
  auto launch_first_template = []() {
    // Remove a desk first, otherwise we will run into an accessibility error
    // with `DeskPreviewView` upon entering overview.
    auto* desks_controller = ash::DesksController::Get();
    RemoveDesk(desks_controller->desks()[1].get());

    // Enter overview and launch the same template.
    ash::ToggleOverview();
    ash::WaitForOverviewEnterAnimation();
    ClickZeroStateTemplatesButton();
    ClickFirstTemplateItem();
    ash::ToggleOverview();
    ash::WaitForOverviewExitAnimation();
  };

  // Tests the bounds of the window if we launch it while the display is upside
  // down.
  display_manager_test_api.UpdateDisplay(
      "2000x" + base::NumberToString(1000 + shelf_height) + "/u");
  launch_first_template();

  // The window is physically on the left, but the coordinates system is as if
  // it were on the right.
  new_browser_window =
      BrowserList::GetInstance()->get(2)->window()->GetNativeWindow();
  new_bounds = new_browser_window->GetBoundsInScreen();
  EXPECT_EQ(800, new_bounds.x());
  EXPECT_EQ(0, new_bounds.y());
  EXPECT_NEAR(1200, new_bounds.width(), 5);
  EXPECT_EQ(1000, new_bounds.height());

  // Change to portrait mode, work area is 1000x2000.
  display_manager_test_api.UpdateDisplay(
      "1000x" + base::NumberToString(2000 + shelf_height));
  launch_first_template();

  // The window is at the top of the screen since we are in portrait
  // orientation, and its height is 60% of the work area height.
  new_browser_window =
      BrowserList::GetInstance()->get(3)->window()->GetNativeWindow();
  new_bounds = new_browser_window->GetBoundsInScreen();
  EXPECT_EQ(0, new_bounds.x());
  EXPECT_EQ(0, new_bounds.y());
  EXPECT_EQ(1000, new_bounds.width());
  EXPECT_NEAR(1200, new_bounds.height(), 5);

  // Launch the window in upside down portrait mode. The height is 60% of the
  // work area height and the window is physically on the top, but the
  // coordinate system is as if it were on the bottom.
  display_manager_test_api.UpdateDisplay(
      "1000x" + base::NumberToString(2000 + shelf_height) + "/u");
  launch_first_template();

  new_browser_window =
      BrowserList::GetInstance()->get(4)->window()->GetNativeWindow();
  new_bounds = new_browser_window->GetBoundsInScreen();
  EXPECT_EQ(0, new_bounds.x());
  EXPECT_EQ(800, new_bounds.y());
  EXPECT_EQ(1000, new_bounds.width());
  EXPECT_NEAR(1200, new_bounds.height(), 5);
}

// Tests that incognito browser windows will NOT be captured in the desk
// template.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUICaptureIncognitoBrowserTest) {
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
  views::Button* dialog_accept_button = ash::GetSavedDeskDialogAcceptButton();
  ASSERT_TRUE(dialog_accept_button);
  aura::Window* root_window =
      dialog_accept_button->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressAndReleaseKey(ui::VKEY_RETURN);

  std::vector<const ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  const ash::DeskTemplate* desk_template = templates.front();
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
IN_PROC_BROWSER_TEST_F(DesksClientTest,
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

  BrowsersAddedObserver browsers_added(/*num_browser_expected=*/3);
  ClickFirstTemplateItem();
  browsers_added.Wait();

  settings_window = nullptr;
  help_window = nullptr;
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
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUILaunchTemplateWithSWAExisting) {
  Profile* profile = browser()->profile();

  // Create the settings and help apps, which are system web apps.
  CreateSettingsSystemWebApp(profile);
  CreateHelpSystemWebApp(profile);

  aura::Window* settings_window = FindBrowserWindow(kSettingsWindowId);
  aura::Window* help_window = FindBrowserWindow(kHelpWindowId);
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  aura::Window* parent = settings_window->parent();
  ASSERT_TRUE(settings_window);
  ASSERT_TRUE(help_window);
  EXPECT_EQ(3u, BrowserList::GetInstance()->size());

  // Give the settings app a known position, and maximize the help app.
  const gfx::Rect settings_bounds(100, 100, 600, 400);
  settings_window->SetBounds(settings_bounds);
  ash::WindowState::Get(help_window)->Maximize();

  // Focus the settings window and then the help window. The MRU order should be
  // [`browser_window`, `settings_window`, `help_window`].
  settings_window->Focus();
  help_window->Focus();
  ASSERT_THAT(parent->children(),
              ElementsAre(browser_window, settings_window, help_window));

  // Enter overview and save the current desk as a template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  // Exit overview and move the settings window to a new place and stack it on
  // top so that we can later verify that it has been placed.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();
  settings_window->SetBounds(gfx::Rect(150, 150, 650, 500));

  // Restore the help window so we can later verify that it remaximizes.
  ash::WindowState::Get(help_window)->Restore();

  // Focus the settings window so it is now on top. We will verify that it gets
  // restacked later.
  settings_window->Focus();
  ASSERT_THAT(parent->children(),
              ElementsAre(browser_window, help_window, settings_window));

  // Enter overview, head over to the desks templates grid and launch the
  // template.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickZeroStateTemplatesButton();
  ClickFirstTemplateItem();

  // Wait for the tabs to load.
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(4u, BrowserList::GetInstance()->size());
  aura::Window* new_browser_window =
      BrowserList::GetInstance()->get(3)->window()->GetNativeWindow();

  // Tests that the stacking is correct while in overview. The parent has other
  // children for overview mode windows, but the first three app windows should
  // be [`new_browser_window`, `settings_window`, `help_window`].
  parent = settings_window->parent();
  aura::Window::Windows app_windows = parent->children();
  app_windows.erase(
      base::ranges::remove_if(app_windows,
                              [](aura::Window* w) {
                                return w->GetProperty(aura::client::kAppType) ==
                                       static_cast<int>(ash::AppType::NON_APP);
                              }),
      app_windows.end());
  ASSERT_THAT(app_windows,
              ElementsAre(new_browser_window, settings_window, help_window));

  // Exit overview.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();

  ash::DesksController* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Verify
  // that the settings window has been moved to the right place and stacked at
  // the bottom. Verify that the help window is maximized.
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(new_browser_window));
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(settings_window));
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(help_window));
  EXPECT_EQ(settings_bounds, settings_window->bounds());
  EXPECT_TRUE(ash::WindowState::Get(help_window)->IsMaximized());

  // Tests that the stacking is correct after exiting overview.
  EXPECT_THAT(parent->children(),
              ElementsAre(new_browser_window, settings_window, help_window));

  // Tests that there is no clipping on either window.
  EXPECT_EQ(gfx::Rect(), settings_window->layer()->clip_rect());
  EXPECT_EQ(gfx::Rect(), help_window->layer()->clip_rect());
}

// Tests that browser windows created from a template have the correct bounds
// and window state.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUIBrowserWindowRestorationTest) {
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
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUILaunchTemplateWithPWA) {
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

  std::vector<const ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  // Find `pwa_browser` window's app restore data.
  const ash::DeskTemplate* desk_template = templates.front();
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
IN_PROC_BROWSER_TEST_F(DesksClientTest,
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

  std::vector<const ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  // Test that `pwa_browser` restore data can be found.
  const ash::DeskTemplate* desk_template = templates.front();
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
IN_PROC_BROWSER_TEST_F(DesksClientTest,
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

  std::vector<const ash::DeskTemplate*> templates = GetAllEntries();
  ASSERT_EQ(1u, templates.size());

  const ash::DeskTemplate* desk_template = templates.front();

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
  auto normalize_state = [](ui::WindowShowState state) {
    return state == ui::SHOW_STATE_DEFAULT ? ui::SHOW_STATE_NORMAL : state;
  };
  EXPECT_EQ(normalize_state(window->GetProperty(aura::client::kShowStateKey)),
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
  EXPECT_EQ(static_cast<int>(apps::LaunchContainer::kLaunchContainerWindow),
            data2->container.value());
  EXPECT_EQ(static_cast<int>(WindowOpenDisposition::NEW_WINDOW),
            data2->disposition.value());
  // Verify window info are correctly captured.
  EXPECT_EQ(settings_app_bounds, data2->current_bounds.value());
  EXPECT_FALSE(data2->desk_id.has_value());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window).id(),
            data->display_id.value());
  EXPECT_EQ(normalize_state(window->GetProperty(aura::client::kShowStateKey)),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_EQ(normalize_state(window->GetProperty(aura::client::kShowStateKey)),
            chromeos::ToWindowShowState(data->window_state_type.value()));
  EXPECT_FALSE(data2->desk_id.has_value());
}

// Tests that launching a template that contains a chrome app works as expected.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUILaunchTemplateWithChromeApp) {
  // Create a chrome app.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);

  const std::string extension_id = extension->id();
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<app_restore::AppLaunchInfo>(
          extension_id, apps::LaunchContainer::kLaunchContainerWindow,
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
IN_PROC_BROWSER_TEST_F(DesksClientTest,
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
IN_PROC_BROWSER_TEST_F(DesksClientTest,
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

  views::Button* delete_button = ash::GetSavedDeskItemDeleteButton(/*index=*/0);
  ClickButton(delete_button);

  // Confirm deleting a template. Use a key press to accept the dialog instead
  // of a click as dialog buttons think a click generated by the event generator
  // is an accidentally click and therefore ignores it.
  views::Button* dialog_accept_button = ash::GetSavedDeskDialogAcceptButton();
  ASSERT_TRUE(dialog_accept_button);
  aura::Window* root_window =
      dialog_accept_button->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.PressAndReleaseKey(ui::VKEY_RETURN);

  // Wait for the model to update.
  ash::WaitForSavedDeskUI();

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

IN_PROC_BROWSER_TEST_F(DesksClientTest,
                       SystemUIPreventBrowserSessionRestoreTest) {
  // Do not exit from test or delete the Profile* when last browser is closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  Profile* profile = browser()->profile();
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Enable session service.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile, pref);

  const int expected_tab_count = 2;
  GURL example2 = GURL(kExampleUrl2);
  content::TestNavigationObserver navigation_observer(example2);
  navigation_observer.StartWatchingNewWebContents();
  chrome::AddTabAt(browser(), example2, /*index=*/-1,
                   /*foreground=*/true);
  navigation_observer.Wait();
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
  ClickZeroStateTemplatesButton();
  ClickFirstTemplateItem();
  content::RunAllTasksUntilIdle();

  // Verify that the browser was launched with the correct number of tabs, and
  // that browser session restore did not restore any windows/tabs.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  Browser* new_browser = BrowserList::GetInstance()->get(0);
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(expected_tab_count, new_browser->tab_strip_model()->count());
}

// Tests that launching the same desk template multiple times creates desks with
// different/incremented names.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SystemUILaunchMultipleDeskTemplates) {
  const base::Uuid kDeskUuid = base::Uuid::GenerateRandomV4();
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
IN_PROC_BROWSER_TEST_F(DesksClientTest,
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

  histogram_tester.ExpectTotalCount(ash::kLaunchTemplateHistogramName,
                                    launches);
}

// Tests that launching a desk template records the appropriate performance
// metric.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateRecordsLoadTimeMetric) {
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

// Tests launch a template to a new desk and clean up desk. Number of windows
// closed should be properly recorded.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchTemplateAndCleanUpDesk) {
  auto* desks_controller = ash::DesksController::Get();
  // Should have 1 default active desk.
  EXPECT_EQ(1u, desks_controller->desks().size());

  // Capture and create a desk template.
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);

  // Set template for testing.
  SetTemplate(std::move(desk_template));

  // Retrieve the `desk_uuid` from `LaunchDeskTemplate` operation and
  // `closeAll` on it.
  base::RunLoop loop;
  base::Uuid desk_id;
  // Launch one template, desk size should increase by 1.
  DesksClient::Get()->LaunchDeskTemplate(
      base::Uuid(), base::BindLambdaForTesting(
                        [&](absl::optional<DesksClient::DeskActionError> error,
                            const base::Uuid& desk_uuid) {
                          EXPECT_EQ(2u, desks_controller->desks().size());
                          desk_id = desk_uuid;
                          loop.Quit();
                          ASSERT_FALSE(error);
                        }));
  loop.Run();

  ash::DeskSwitchAnimationWaiter waiter;
  // Creates a new window.
  CreateBrowser({});
  base::HistogramTester histogram_tester;
  ASSERT_FALSE(DesksClient::Get()->RemoveDesk(desk_id, false));
  waiter.Wait();
  // Record number of windows being closed per source.
  // NOTE: The template contains an existing browser with 1 tab created by
  // `BrowserMain()`.
  histogram_tester.ExpectUniqueSample("Ash.Desks.NumberOfWindowsClosed2.Api", 2,
                                      1);
  EXPECT_EQ(1u, desks_controller->desks().size());
}

// Tests that if we've been in the library, then switched to a different desk,
// and then save the desk, that the desk is closed. Regression test for
// https://crbug.com/1329350.
IN_PROC_BROWSER_TEST_F(DesksClientTest,
                       // TODO(crbug.com/1369348): Re-enable this test
                       DISABLED_SystemUIReEnterLibraryAndSaveDesk) {
  // Create a template that has a window because the "Save desk for later"
  // button is not enabled on empty desks.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  ash::DesksController* desks_controller = ash::DesksController::Get();
  const auto& desks = desks_controller->desks();
  ASSERT_EQ(1u, desks.size());
  ASSERT_TRUE(desks[0]->ContainsAppWindows());

  // Click on the "Use template" button to launch the template.
  ClickFirstTemplateItem();

  // Verify that a new desk has been created and activated, and that it has app
  // windows.
  ASSERT_EQ(2ul, desks.size());
  ASSERT_EQ(1, desks_controller->GetActiveDeskIndex());
  ASSERT_TRUE(desks_controller->active_desk()->ContainsAppWindows());

  // Now save the desk. This should close the desk.
  auto* overview_grid = ash::GetOverviewSession()->GetGridWithRootWindow(
      ash::Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  auto* save_desk_button = overview_grid->GetSaveDeskForLaterButton();
  ASSERT_TRUE(save_desk_button);

  // Wait for the bounds to finish animating.
  ash::ShellTestApi().WaitForWindowFinishAnimating(
      save_desk_button->GetWidget()->GetNativeWindow());
  ClickButton(save_desk_button);
  ash::WaitForSavedDeskUI();

  // Wait for the browser to close.
  ui_test_utils::WaitForBrowserToClose();

  // Verify that we're back to one desk.
  EXPECT_EQ(1u, desks.size());
}

// Tests trying to remove an invalid desk Id should return error.
IN_PROC_BROWSER_TEST_F(DesksClientTest, RemoveWithInvalidDeskId) {
  auto* desks_controller = ash::DesksController::Get();
  // Should have 1 default desk.
  EXPECT_EQ(1u, desks_controller->desks().size());
  // Construct an empty invalid desk_id.
  base::Uuid desk_id;
  EXPECT_THAT(DesksClient::Get()->RemoveDesk(desk_id, false),
              testing::Optional(DesksClient::DeskActionError::kInvalidIdError));
  EXPECT_EQ(1u, desks_controller->desks().size());
}

// Tests list all available desks. Remove desk should fail when there is only
// one desk.
IN_PROC_BROWSER_TEST_F(DesksClientTest, GetAllDesksAndRemove) {
  auto* desks_controller = ash::DesksController::Get();
  // Should have 1 default active desk.
  EXPECT_EQ(1u, desks_controller->desks().size());

  base::RunLoop loop;
  // Retrieve desk id;
  base::Uuid desk_id;
  auto desks = DesksClient::Get()->GetAllDesks();
  ASSERT_TRUE(desks.has_value());
  ASSERT_EQ(1u, desks.value().size());
  desk_id = desks.value().at(0)->uuid();

  EXPECT_THAT(DesksClient::Get()->RemoveDesk(desk_id, false),
              testing::Optional(
                  DesksClient::DeskActionError::kDesksCountCheckFailedError));
}

// Tests launch an empty desk with `desk_name` provided.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchEmptyDeskWithProvidedName) {
  auto* desks_controller = ash::DesksController::Get();
  // Should have 1 default active desk.
  EXPECT_EQ(1u, desks_controller->desks().size());

  base::RunLoop loop;
  std::u16string desk_name(u"test");
  ash::DeskSwitchAnimationWaiter waiter;
  auto result = DesksClient::Get()->LaunchEmptyDesk(desk_name);

  ASSERT_TRUE(result.has_value());
  // Launch one template, desk size should increase by 1.
  ASSERT_EQ(2u, desks_controller->desks().size());
  // `desk_name` should be set as provided
  EXPECT_EQ(desk_name, desks_controller->desks().back()->name());
  // `desk_uuid` should be returned.
  EXPECT_GT(result.value().AsLowercaseString().size(), 0u);
  waiter.Wait();
}

// Tests launch an empty desk with default name.
IN_PROC_BROWSER_TEST_F(DesksClientTest, LaunchEmptyDeskWithDefaultName) {
  auto* desks_controller = ash::DesksController::Get();
  // Should have 1 default active desk.
  EXPECT_EQ(1u, desks_controller->desks().size());

  base::RunLoop loop;
  ash::DeskSwitchAnimationWaiter waiter;
  auto result = DesksClient::Get()->LaunchEmptyDesk();
  ASSERT_TRUE(result.has_value());
  // Launch one template, desk size should increase by 1.
  ASSERT_EQ(2u, desks_controller->desks().size());
  // `desk_name` should be set as default desk name
  EXPECT_EQ(u"Desk 2", desks_controller->desks().back()->name());
  // `desk_uuid` should be returned.
  EXPECT_GT(result.value().AsLowercaseString().size(), 0u);
  waiter.Wait();
}

// Tests setting first window to show on all desk and then unset it.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SetWindowProperties) {
  // Create a new browser window.
  CreateBrowser({});

  auto* desks_controller = ash::DesksController::Get();

  // Start with no all-desk window.
  EXPECT_EQ(0u, desks_controller->visible_on_all_desks_windows().size());

  // Get the first browser window.
  SessionID browser_session_id =
      BrowserList::GetInstance()->get(0)->session_id();

  // Set to all-desk window.
  // Assert no error.
  ASSERT_FALSE(DesksClient::Get()->SetAllDeskPropertyByBrowserSessionId(
      browser_session_id, true));
  // Should have 1 all-desk window now.
  EXPECT_EQ(1u, desks_controller->visible_on_all_desks_windows().size());

  // Unset all-desk window.
  // Assert no error.
  ASSERT_FALSE(DesksClient::Get()->SetAllDeskPropertyByBrowserSessionId(
      browser_session_id, false));
  // Should have no all-desk window now.
  EXPECT_EQ(0u, desks_controller->visible_on_all_desks_windows().size());
}

// Tests immediate desk action should be throttled.
IN_PROC_BROWSER_TEST_F(DesksClientTest, ThrottleImmediateDeskAction) {
  base::Uuid new_desk_id;
  ash::DeskSwitchAnimationWaiter waiter;
  auto result = DesksClient::Get()->LaunchEmptyDesk();
  ASSERT_TRUE(result.has_value());
  new_desk_id = result.value();

  EXPECT_THAT(DesksClient::Get()->RemoveDesk(new_desk_id, false),
              testing::Optional(
                  DesksClient::DeskActionError::kDesksBeingModifiedError));

  auto result1 = DesksClient::Get()->LaunchEmptyDesk();
  EXPECT_EQ(DesksClient::DeskActionError::kDesksBeingModifiedError,
            result1.error());
  EXPECT_THAT(DesksClient::Get()->SwitchDesk(new_desk_id),
              testing::Optional(
                  DesksClient::DeskActionError::kDesksBeingModifiedError));
  waiter.Wait();
}

// Tests save an empty desk should fail.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SaveEmptyDesk) {
  // Create a new browser and add a few tabs to it.
  Browser* browser = CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kSaveAndRecall);
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
  // Check the urls are captured correctly in the `saved_desk`.
  EXPECT_EQ(data->urls, urls);

  // Exit overview.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();
  // An empty desk should be created.
  EXPECT_EQ(ash::DesksController::Get()->GetNumberOfDesks(), 1);
  EXPECT_EQ(ash::DesksController::Get()->active_desk()->windows().size(), 0u);
}

// Tests save an active desk to library and remove it from desk list.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SaveActiveDesk) {
  // Create a new browser and add a few tabs to it.
  Browser* browser = CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  aura::Window* window = browser->window()->GetNativeWindow();

  const int32_t browser_window_id =
      window->GetProperty(app_restore::kWindowIdKey);
  // Get current tabs from browser.
  std::vector<GURL> urls = GetURLsForBrowserWindow(browser);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kSaveAndRecall);
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
  // Check the urls are captured correctly in the `saved_desk`.
  EXPECT_EQ(data->urls, urls);

  // Exit overview.
  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();
  // An empty desk should be created.
  EXPECT_EQ(ash::DesksController::Get()->GetNumberOfDesks(), 1);
  EXPECT_EQ(ash::DesksController::Get()->active_desk()->windows().size(), 0u);
}

// Tests delete a saved desk from library.
IN_PROC_BROWSER_TEST_F(DesksClientTest, DeleteSavedDesk) {
  auto* desk_model = DesksClient::Get()->GetDeskModel();

  // Create a new browser and add a few tabs to it.
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kSaveAndRecall);
  EXPECT_EQ(1u, desk_model->GetEntryCount());

  DeleteDeskTemplate(desk_template->uuid());

  EXPECT_EQ(0u, desk_model->GetEntryCount());
}

// Tests recall a saved desk from library.
IN_PROC_BROWSER_TEST_F(DesksClientTest, RecallSavedDesk) {
  auto* desk_model = DesksClient::Get()->GetDeskModel();
  EXPECT_EQ(ash::DesksController::Get()->GetNumberOfDesks(), 1);

  // Create a new browser and add a few tabs to it.
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});

  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kSaveAndRecall);

  EXPECT_EQ(1u, desk_model->GetEntryCount());
  EXPECT_EQ(ash::DesksController::Get()->GetNumberOfDesks(), 1);

  // Restart browser process.
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  base::RunLoop loop;
  DesksClient::Get()->LaunchDeskTemplate(
      desk_template->uuid(),
      base::BindLambdaForTesting(
          [desk_model, &loop](
              absl::optional<DesksClient::DeskActionError> error,
              const base::Uuid& desk_uuid) {
            EXPECT_EQ(ash::DesksController::Get()->GetNumberOfDesks(), 2);
            EXPECT_EQ(0u, desk_model->GetEntryCount());
            loop.Quit();
          }));
  loop.Run();
}

// Tests switch to current desk should be no ops.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SwitchToCurrentDesk) {
  base::Uuid current_desk_uuid;
  current_desk_uuid = DesksClient::Get()->GetActiveDesk();

  // Expect no error message.
  EXPECT_FALSE(DesksClient::Get()->SwitchDesk(current_desk_uuid));

  base::Uuid desk_uuid = DesksClient::Get()->GetActiveDesk();
  EXPECT_EQ(current_desk_uuid, desk_uuid);
}

// Tests switch to invalid desk should return error.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SwitchToInvalidDesk) {
  EXPECT_THAT(
      DesksClient::Get()->SwitchDesk({}),
      testing::Optional(DesksClient::DeskActionError::kResourceNotFoundError));
}

// Tests switch to different desk should be trigger desk animation.
IN_PROC_BROWSER_TEST_F(DesksClientTest, SwitchToDifferentDesk) {
  base::Uuid desk_uuid = DesksClient::Get()->GetActiveDesk();

  // Launches a new desk.
  auto result = DesksClient::Get()->LaunchEmptyDesk();
  ASSERT_TRUE(result.has_value());

  // Wait for launch desk animation to settle.
  ash::DeskSwitchAnimationWaiter waiter;
  waiter.Wait();

  // Switches to previous desk. Expect no error message.
  EXPECT_FALSE(DesksClient::Get()->SwitchDesk(desk_uuid));

  // Wait for desk switch animation.
  ash::DeskSwitchAnimationWaiter waiter_;
  waiter_.Wait();
  base::Uuid desk_uuid_ = DesksClient::Get()->GetActiveDesk();
  EXPECT_EQ(desk_uuid_, desk_uuid);
}

// Tests retrieve an existing desk.
IN_PROC_BROWSER_TEST_F(DesksClientTest, GetDeskByValidDeskId) {
  base::Uuid desk_uuid = DesksClient::Get()->GetActiveDesk();
  auto result = DesksClient::Get()->GetDeskByID(desk_uuid);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->uuid(), desk_uuid);
}

// Tests retrieve an non-exist desk should return error.
IN_PROC_BROWSER_TEST_F(DesksClientTest, GetDeskByInvalidDeskId) {
  auto result = DesksClient::Get()->GetDeskByID(base::Uuid::GenerateRandomV4());

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            DesksClient::DeskActionError::kResourceNotFoundError);
}

// Tests that floating workspace template can be captured with fixed uuid.
IN_PROC_BROWSER_TEST_F(DesksClientTest, CaptureFloatingWorkspaceTemplateTest) {
  // Create a new browser and add a few tabs to it.
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(
          ash::DeskTemplateType::kFloatingWorkspace);
  EXPECT_TRUE(desk_template->uuid().is_valid());
  EXPECT_EQ(
      desk_template->uuid(),
      base::Uuid::ParseLowercase(
          desks_storage::desk_template_util::kFloatingWorkspaceTemplateUuid));
}

// Tests that floating workspace templates do not count towards template counts
// for saved desks functionality.
IN_PROC_BROWSER_TEST_F(DesksClientTest, FloatingWorkspaceOnSavedDesksUI) {
  // Create a new browser and add a few tabs to it.
  CreateBrowser({GURL(kExampleUrl1), GURL(kExampleUrl2)});
  std::unique_ptr<ash::DeskTemplate> desk_template =
      CaptureActiveDeskAndSaveTemplate(
          ash::DeskTemplateType::kFloatingWorkspace);
  EXPECT_TRUE(desk_template->uuid().is_valid());
  EXPECT_EQ(
      desk_template->uuid(),
      base::Uuid::ParseLowercase(
          desks_storage::desk_template_util::kFloatingWorkspaceTemplateUuid));

  auto* desk_model = DesksClient::Get()->GetDeskModel();
  ASSERT_EQ(0u, desk_model->GetEntryCount());

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  // Tests that since we have no saved desk right now, so the library button is
  // hidden.
  views::Button* zero_state_templates_button = ash::GetZeroStateLibraryButton();
  ASSERT_TRUE(zero_state_templates_button);
  EXPECT_FALSE(zero_state_templates_button->GetVisible());
}

class DesksTemplatesClientLacrosTest : public InProcessBrowserTest {
 public:
  DesksTemplatesClientLacrosTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kDesksTemplates},
        /*disabled_features=*/{ash::features::kDeskTemplateSync});
  }
  DesksTemplatesClientLacrosTest(const DesksTemplatesClientLacrosTest&) =
      delete;
  DesksTemplatesClientLacrosTest& operator=(
      const DesksTemplatesClientLacrosTest&) = delete;
  ~DesksTemplatesClientLacrosTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    if (!ash_starter_.HasLacrosArgument())
      return;

    ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
  }

  void SetUpOnMainThread() override {
    if (!ash_starter_.HasLacrosArgument())
      return;

    // `StartLacros()` will bring up one lacros browser. There will also be one
    // classic browser from `InProcessBrowserTest` that can be accessed with
    // `browser()`.
    LacrosWindowWaiter waiter;
    ash_starter_.StartLacros(this);
    std::ignore = waiter.Wait(/*expected_count=*/1u);
  }

 protected:
  // Helper class which waits for lacros windows to become visible.
  class LacrosWindowWaiter : public aura::WindowObserver {
   public:
    LacrosWindowWaiter() {
      window_observation_.Observe(ash::Shell::GetPrimaryRootWindow());
    }
    LacrosWindowWaiter(const LacrosWindowWaiter&) = delete;
    LacrosWindowWaiter& operator=(const LacrosWindowWaiter&) = delete;
    ~LacrosWindowWaiter() override = default;

    // Spins the loop and waits for `expected_count` number of lacros windows to
    // become visible.
    aura::Window::Windows Wait(size_t expected_count) {
      DCHECK(windows_.empty());
      DCHECK_GT(expected_count, 0u);

      expected_count_ = expected_count;
      run_loop_.Run();
      return windows_;
    }

    // aura::WindowObserver::
    void OnWindowVisibilityChanged(aura::Window* window,
                                   bool visible) override {
      if (!visible || !crosapi::browser_util::IsLacrosWindow(window))
        return;

      windows_.push_back(window);
      if (windows_.size() < expected_count_)
        return;

      run_loop_.Quit();
    }

   private:
    size_t expected_count_ = 0u;

    // The vector of lacros windows that where shown while waiting.
    aura::Window::Windows windows_;

    base::RunLoop run_loop_;

    base::ScopedObservation<aura::Window, aura::WindowObserver>
        window_observation_{this};
  };

  base::test::ScopedFeatureList scoped_feature_list_;
  test::AshBrowserTestStarter ash_starter_;
};

// Tests launching a template with a browser window.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientLacrosTest, SystemUILaunchBrowser) {
  if (!ash_starter_.HasLacrosArgument())
    return;

  ASSERT_TRUE(crosapi::BrowserManager::Get()->IsRunning());

  // Enter overview and save the current desk as a template. The current desk
  // has one lacros browser, and one regular browser.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  // Launch the saved desk template. We expect two launched lacros windows,
  // since the regular browser window was saved and will be launched as a lacros
  // window. Check the launched windows will have data in
  // `app_restore::kWindowInfoKey`, otherwise ash does not know that they are
  // launched from desk templates. See https://crbug.com/1333965 for more
  // details.
  LacrosWindowWaiter waiter;
  ClickFirstTemplateItem();
  aura::Window::Windows launched_windows = waiter.Wait(/*expected_count=*/2u);
  ASSERT_EQ(2u, launched_windows.size());
  for (auto* window : launched_windows)
    EXPECT_TRUE(window->GetProperty(app_restore::kWindowInfoKey));

  ash::ToggleOverview();
  ash::WaitForOverviewExitAnimation();
}

// Tests that app browsers are captured correctly, coverage for launching
// lacros browsers can be found in
// c/b/lacros/desk_template_client_browsertest.cc This simply confirms that the
// chrome desk client handles apps correctly when converting the returned mojom
// from crosapi to app_launch_info.
IN_PROC_BROWSER_TEST_F(DesksTemplatesClientLacrosTest,
                       CapturesLacrosAppCorrectly) {
  // Prevents test from running when running in a build without lacros.
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }

  ASSERT_TRUE(crosapi::BrowserManager::Get()->IsRunning());

  // Add our browser under test, this is the only way to launch an app
  // via the BrowserManager.
  crosapi::BrowserManager::Get()->CreateBrowserWithRestoredData(
      {GURL(kExampleUrl1)}, {0, 0, 256, 256}, {},
      ui::WindowShowState::SHOW_STATE_DEFAULT,
      /*active_tab_index=*/0, /*first_non_pinned_tab_index=*/0, kTestAppName,
      kTestWindowId);
  LacrosWindowWaiter waiter;
  aura::Window::Windows launched_windows = waiter.Wait(/*expected_count=*/1u);
  ASSERT_EQ(1u, launched_windows.size());

  // Enter overview and save the current desk as a template. The current desk
  // has one lacros app and the default browser.
  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();

  // Grab all entries to assert.
  const std::vector<const ash::DeskTemplate*> all_entries = GetAllEntries();
  ASSERT_EQ(all_entries.size(), 1u);

  // Since we only have one template grab the first one.
  const app_restore::RestoreData* desk_restore_data =
      all_entries[0]->desk_restore_data();
  ASSERT_NE(desk_restore_data, nullptr);

  // Through an exhaustive process of trial and error we cannot retrieve our
  // desired window through any other means than running a search for the app
  // name in the mapping of window ID's to app launch info.  There isn't a way,
  // currently at least, within this test class or through the crosapi
  // to close the default browser.  We can't use the test's class' close browser
  // method either as both have been tried and failed to successfully close the
  // window.  Furthermore obscuring the window from the capture logic doesn't
  // seem to work in the testing logic either.  Simply using the window ID the
  // browser was launched with doesn't work either because the window ID is
  // changed on capture.
  const auto& app_id_to_launch_list =
      desk_restore_data->app_id_to_launch_list();
  const auto& launch_list =
      app_id_to_launch_list.at(app_constants::kLacrosAppId);
  const app_restore::AppRestoreData* actual_app_data = nullptr;

  for (const auto& it : launch_list) {
    if (it.second->app_name.has_value() &&
        it.second->app_name.value() == kTestAppName) {
      actual_app_data = it.second.get();
      break;
    }
  }
  ASSERT_NE(actual_app_data, nullptr);

  // Finally assert we set the relevant fields properly.
  EXPECT_TRUE(actual_app_data->app_type_browser.has_value());
  EXPECT_TRUE(actual_app_data->app_type_browser.value());
}

using SaveAndRecallBrowserTest = DesksClientTest;

IN_PROC_BROWSER_TEST_F(SaveAndRecallBrowserTest,
                       SystemUIBlockingDialogAccepted) {
  SetupBrowserToConfirmClose(browser());

  // We'll now save the desk as Save & Recall. After saving desks, this
  // operation will try to automatically close windows.

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskForLaterButton();
  ash::WaitForSavedDeskUI();

  ash::SavedDeskPresenterTestApi::WaitForSaveAndRecallBlockingDialog();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Send a key to OK the close dialog.
  BrowsersRemovedObserver browsers_removed(/*browser_removes_expected=*/1);
  ash::SendKey(ui::VKEY_RETURN);
  browsers_removed.Wait();

  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // Verify that we are in the library and that there's one saved desk.
  auto* overview_grid = ash::GetOverviewSession()->GetGridWithRootWindow(
      ash::Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_TRUE(overview_grid->IsShowingSavedDeskLibrary());

  std::vector<const ash::DeskTemplate*> templates = GetAllEntries();
  EXPECT_EQ(1u, templates.size());
}

IN_PROC_BROWSER_TEST_F(SaveAndRecallBrowserTest,
                       SystemUIBlockingDialogRejected) {
  SetupBrowserToConfirmClose(browser());

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();
  ClickSaveDeskForLaterButton();
  ash::WaitForSavedDeskUI();

  ash::SavedDeskPresenterTestApi::WaitForSaveAndRecallBlockingDialog();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Send escape to cancel the dialog (keep the browser running).
  ash::SendKey(ui::VKEY_ESCAPE);
  content::RunAllTasksUntilIdle();

  ash::SavedDeskPresenterTestApi::FireWindowWatcherTimer();

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // We should be in overview mode.
  ASSERT_TRUE(ash::Shell::Get()->overview_controller()->overview_session());
}
// TODO(crbug.com/1333965): Add some tests to launch LaCros browser.

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
  auto* desk_model = DesksClient::Get()->GetDeskModel();
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
  full_restore::SaveAppLaunchInfo(
      browser()->profile()->GetPath(),
      std::make_unique<app_restore::AppLaunchInfo>(
          app_id, ui::EF_NONE, session_id1, display::kDefaultDisplayId));

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
  EXPECT_TRUE(ash::saved_desk_util::AreDesksTemplatesEnabled());

  // Disable desk templates through policy.
  SetDeskTemplateEnabledPolicy(false);
  // Desk templates feature should be disabled, despite feature flag is set to
  // enabled.
  EXPECT_FALSE(ash::saved_desk_util::AreDesksTemplatesEnabled());

  // Enable desk templates through policy.
  SetDeskTemplateEnabledPolicy(true);
  // Desk templates feature should be enabled.
  EXPECT_TRUE(ash::saved_desk_util::AreDesksTemplatesEnabled());
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
  EXPECT_FALSE(ash::saved_desk_util::AreDesksTemplatesEnabled());

  // Disable desk templates through policy.
  SetDeskTemplateEnabledPolicy(false);
  // Desk templates feature should be disabled.
  EXPECT_FALSE(ash::saved_desk_util::AreDesksTemplatesEnabled());

  // Enable desk templates through policy.
  SetDeskTemplateEnabledPolicy(true);
  // Desk templates feature should be enabled, despite feature flag is set to
  // disabled.
  EXPECT_TRUE(ash::saved_desk_util::AreDesksTemplatesEnabled());
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
      CaptureActiveDeskAndSaveTemplate(ash::DeskTemplateType::kTemplate);
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
  EXPECT_TRUE(DesksClient::Get());

  base::Uuid admin_template_uuid =
      base::Uuid::ParseCaseInsensitive(kTestAdminTemplateUuid);

  // Set an admin template policy.
  DesksClient::Get()->SetPolicyPreconfiguredTemplate(
      account_id1_, std::make_unique<std::string>(base::StringPrintf(
                        kTestAdminTemplateFormat, kTestAdminTemplateUuid)));

  // Verify that the admin templates is present.
  EXPECT_TRUE(ContainUuidInTemplates(admin_template_uuid, GetDeskTemplates()));

  // Clear admin templates.
  DesksClient::Get()->RemovePolicyPreconfiguredTemplate(account_id1_);

  // Verify that the admin templates is removed.
  EXPECT_FALSE(ContainUuidInTemplates(admin_template_uuid, GetDeskTemplates()));
}

class AdminTemplateTest : public extensions::PlatformAppBrowserTest {
 public:
  AdminTemplateTest()
      : scoped_feature_list_(ash::features::kAppLaunchAutomation) {
    // Suppress the multitask menu nudge as we'll be checking the stacking order
    // and the count of the active desk children.
    chromeos::MultitaskMenuNudgeController::SetSuppressNudgeForTesting(true);
  }
  AdminTemplateTest(const AdminTemplateTest&) = delete;
  AdminTemplateTest& operator=(const AdminTemplateTest&) = delete;
  ~AdminTemplateTest() override = default;

  // The definition of an admin template for a test.
  struct AdminTemplateDefinition {
    struct WindowDefinition {
      std::vector<std::string> urls;
      absl::optional<gfx::Rect> bounds;
    };

    std::vector<WindowDefinition> windows;
  };

  // Converts a `gfx::Rect` to a list, as expected by `RestoreData`.
  base::Value::List CreateBounds(const gfx::Rect& bounds) {
    base::Value::List list;
    list.Append(bounds.x());
    list.Append(bounds.y());
    list.Append(bounds.width());
    list.Append(bounds.height());
    return list;
  }

  // Creates an admin template with the windows and URLs given by `definition`.
  std::unique_ptr<ash::DeskTemplate> CreateAdminTemplate(
      const AdminTemplateDefinition& definition) {
    base::Value::Dict windows;
    for (size_t i = 0; i != definition.windows.size(); ++i) {
      base::Value::Dict window;
      window.Set("title", "Chrome");
      window.Set("window_state_type", 0);

      if (definition.windows[i].bounds) {
        window.Set("current_bounds",
                   CreateBounds(*definition.windows[i].bounds));
      }

      base::Value::List urls;
      for (const std::string& url : definition.windows[i].urls) {
        urls.Append(url);
      }
      window.Set("urls", std::move(urls));

      windows.Set(base::NumberToString(i + 1), std::move(window));
    }

    base::Value::Dict root;
    root.Set(app_constants::kChromeAppId, std::move(windows));

    auto admin_template = std::make_unique<ash::DeskTemplate>(
        base::Uuid::GenerateRandomV4(), ash::DeskTemplateSource::kPolicy,
        "Admin template", base::Time::Now(), ash::DeskTemplateType::kTemplate);

    admin_template->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>(
            base::Value(std::move(root))));

    return admin_template;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b/273803538): Add tests for lacros.
IN_PROC_BROWSER_TEST_F(AdminTemplateTest, LaunchAdminTemplate) {
  // Launch an admin template with a single browser. Verifies that a browser was
  // actually launched.
  auto admin_template = CreateAdminTemplate(
      {.windows = {
           {.urls = {kExampleUrl1}, .bounds = gfx::Rect(100, 50, 400, 300)}}});
  ASSERT_NE(admin_template, nullptr);

  base::Uuid template_uuid = admin_template->uuid();

  auto* saved_desk_controller = ash::Shell::Get()->saved_desk_controller();
  ash::SavedDeskControllerTestApi(saved_desk_controller)
      .SetAdminTemplate(std::move(admin_template));

  saved_desk_controller->LaunchAdminTemplate(
      template_uuid, display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // Verify that there are two browsers (one from the suite and one from the
  // test), and verify that our launched browser is stacked on top.
  Browser* new_browser = FindLaunchedBrowserByURLs({GURL(kExampleUrl1)});
  ASSERT_TRUE(new_browser);

  aura::Window* old_browser_window = browser()->window()->GetNativeWindow();
  aura::Window* new_browser_window = new_browser->window()->GetNativeWindow();

  // Both browsers should be on the same desk.
  ASSERT_EQ(old_browser_window->parent(), new_browser_window->parent());

  // Verify that the new browser window is stacked in front of the
  // existing. Children are ordered from bottommost to topmost. We therefore
  // expect the new window to have an index that is higher than the old.
  const auto& container = new_browser_window->parent()->children();
  size_t new_index =
      base::ranges::find(container, new_browser_window) - container.begin();
  size_t old_index =
      base::ranges::find(container, old_browser_window) - container.begin();

  EXPECT_GT(new_index, old_index);
}

IN_PROC_BROWSER_TEST_F(AdminTemplateTest, AdminTemplateWindowOffset) {
  // Launch an admin template with a single browser. Verifies that a browser was
  // actually launched.
  auto admin_template = CreateAdminTemplate(
      {.windows = {
           {.urls = {kExampleUrl1}, .bounds = gfx::Rect(50, 50, 640, 480)}}});
  ASSERT_NE(admin_template, nullptr);

  base::Uuid template_uuid = admin_template->uuid();

  auto* saved_desk_controller = ash::Shell::Get()->saved_desk_controller();
  ash::SavedDeskControllerTestApi(saved_desk_controller)
      .SetAdminTemplate(std::move(admin_template));

  // Launch the template twice.
  for (int i = 0; i != 2; ++i) {
    saved_desk_controller->LaunchAdminTemplate(
        template_uuid, display::Screen::GetScreen()->GetPrimaryDisplay().id());
  }

  auto browsers = FindLaunchedBrowsersByURLs({GURL(kExampleUrl1)});
  ASSERT_EQ(browsers.size(), 2u);

  // Verify that the two windows are not in the same position.
  aura::Window* window1 = browsers[0]->window()->GetNativeWindow();
  aura::Window* window2 = browsers[1]->window()->GetNativeWindow();
  EXPECT_NE(window1->bounds(), window2->bounds());
}
