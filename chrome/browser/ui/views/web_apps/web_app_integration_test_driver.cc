// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_integration_test_driver.h"

#include <cstddef>
#include <cstring>
#include <ios>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/web_app_startup_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/create_application_shortcut_view_test_support.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/file_handler_launch_dialog_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/window_controls_overlay_toggle_button.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/ui/webui/app_settings/web_app_settings_ui.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/debug_info_printer.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom-forward.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_uninstall_dialog_view.h"
#else
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"
#include "chrome/browser/ui/webui/app_management/web_app_settings_page_handler.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/version.h"
#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <ImageIO/ImageIO.h>

#include "base/mac/mac_util.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/apps/app_shim/web_app_shim_manager_delegate_mac.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_launch.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/test/base/launchservices_utils_mac.h"
#include "net/base/filename_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#include "base/win/shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace web_app::integration_tests {

namespace {

base::FilePath GetTestDataDir() {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_dir));
  return root_dir.AppendASCII("chrome").AppendASCII("test").AppendASCII("data");
}

Site InstallableSiteToSite(InstallableSite site) {
  switch (site) {
    case InstallableSite::kStandalone:
      return Site::kStandalone;
    case InstallableSite::kMinimalUi:
      return Site::kMinimalUi;
    case InstallableSite::kTabbed:
      return Site::kTabbed;
    case InstallableSite::kTabbedWithHomeTab:
      return Site::kTabbedWithHomeTab;
    case InstallableSite::kStandaloneNestedA:
      return Site::kStandaloneNestedA;
    case InstallableSite::kStandaloneNestedB:
      return Site::kStandaloneNestedB;
    case InstallableSite::kStandaloneNotStartUrl:
      return Site::kStandaloneNotStartUrl;
    case InstallableSite::kWco:
      return Site::kWco;
    case InstallableSite::kFileHandler:
      return Site::kFileHandler;
    case InstallableSite::kNoServiceWorker:
      return Site::kNoServiceWorker;
    case InstallableSite::kNotInstalled:
      return Site::kNotInstalled;
    case InstallableSite::kScreenshots:
      return Site::kScreenshots;
    case InstallableSite::kChromeUrl:
      return Site::kChromeUrl;
  }
}

int NumberToInt(Number number) {
  switch (number) {
    case Number::kOne:
      return 1;
    case Number::kTwo:
      return 2;
  }
}

// Flushes the shortcuts tasks, which seem to sometimes still hang around after
// our tasks are done.
// TODO(crbug.com/40206415): Investigate the true source of flakiness instead of
// papering over it here.
void FlushShortcutTasks() {
  // Execute the UI thread task runner before and after the shortcut task runner
  // to ensure that tasks get to the shortcut runner, and then any scheduled
  // replies on the UI thread get run.
  {
    base::RunLoop loop;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  {
    base::RunLoop loop;
    internals::GetShortcutIOTaskRunner()->PostTask(FROM_HERE,
                                                   loop.QuitClosure());
    loop.Run();
  }
  {
    base::RunLoop loop;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
}

struct SiteConfig {
  std::string relative_url;
  std::string relative_manifest_id;
  std::string app_name;
  std::u16string wco_not_enabled_title;
  SkColor icon_color;
  base::flat_set<std::string> alternate_titles;
  std::string base_url;  // if not specified, use GetTestServerForSiteMode
  std::optional<Site> parent_site;
  bool is_isolated = false;
};

base::flat_map<Site, SiteConfig> g_site_configs = {
    {Site::kStandalone,
     {.relative_url = "/webapps_integration/standalone/basic.html",
      .relative_manifest_id = "webapps_integration/standalone/basic.html",
      .app_name = "Site A",
      // WCO disabled is the defaulting state so the title when disabled
      // should match with the app's name.
      .wco_not_enabled_title = u"Site A",
      .icon_color = SK_ColorGREEN,
      .alternate_titles = {"Site A - Updated name"}}},
    {Site::kMinimalUi,
     {.relative_url = "/webapps_integration/minimal_ui/basic.html",
      .relative_manifest_id = "webapps_integration/minimal_ui/basic.html",
      .app_name = "Site B",
      .wco_not_enabled_title = u"Site B",
      .icon_color = SK_ColorBLACK}},
    {Site::kTabbed,
     {.relative_url = "/webapps_integration/tabbed/basic.html",
      .relative_manifest_id = "webapps_integration/tabbed/basic.html",
      .app_name = "Tabbed",
      .wco_not_enabled_title = u"Tabbed",
      .icon_color = SK_ColorRED}},
    {Site::kTabbedWithHomeTab,
     {.relative_url =
          "/webapps_integration/tabbed/basic.html?manifest=home_tab.json",
      .relative_manifest_id =
          "webapps_integration/tabbed/basic.html?manifest=home_tab.json",
      .app_name = "Tabbed with home tab",
      .wco_not_enabled_title = u"Tabbed with home tab",
      .icon_color = SK_ColorRED}},
    {Site::kTabbedNestedA,
     {.relative_url = "/webapps_integration/tabbed/sub_page_1.html",
      .relative_manifest_id =
          "webapps_integration/tabbed/basic.html?manifest=home_tab.json",
      .app_name = "Tabbed with home tab",
      .wco_not_enabled_title = u"Tabbed with home tab",
      .icon_color = SK_ColorRED}},
    {Site::kTabbedNestedB,
     {.relative_url = "/webapps_integration/tabbed/sub_page_2.html",
      .relative_manifest_id =
          "webapps_integration/tabbed/basic.html?manifest=home_tab.json",
      .app_name = "Tabbed with home tab",
      .wco_not_enabled_title = u"Tabbed with home tab",
      .icon_color = SK_ColorRED}},
    {Site::kTabbedNestedC,
     {.relative_url = "/webapps_integration/tabbed/sub_page_3.html",
      .relative_manifest_id =
          "webapps_integration/tabbed/basic.html?manifest=home_tab.json",
      .app_name = "Tabbed with home tab",
      .wco_not_enabled_title = u"Tabbed with home tab",
      .icon_color = SK_ColorRED}},
    {Site::kNotPromotable,
     {.relative_url = "/webapps_integration/not_promotable/basic.html",
      .relative_manifest_id = "webapps_integration/not_promotable/basic.html",
      .app_name = "Site C",
      .wco_not_enabled_title = u"Site C",
      .icon_color = SK_ColorTRANSPARENT}},
    {Site::kWco,
     {.relative_url = "/webapps_integration/wco/basic.html",
      .relative_manifest_id = "webapps_integration/wco/basic.html",
      .app_name = "Site WCO",
      .wco_not_enabled_title = u"Site WCO",
      .icon_color = SK_ColorGREEN}},
    {Site::kStandaloneNestedA,
     {.relative_url = "/webapps_integration/standalone/foo/basic.html",
      .relative_manifest_id = "webapps_integration/standalone/foo/basic.html",
      .app_name = "Site A Foo",
      .wco_not_enabled_title = u"Site A Foo",
      .icon_color = SK_ColorGREEN}},
    {Site::kStandaloneNestedB,
     {.relative_url = "/webapps_integration/standalone/bar/basic.html",
      .relative_manifest_id = "webapps_integration/standalone/bar/basic.html",
      .app_name = "Site A Bar",
      .wco_not_enabled_title = u"Site A Bar",
      .icon_color = SK_ColorGREEN}},
    {Site::kFileHandler,
     {.relative_url = "/webapps_integration/file_handler/basic.html",
      .relative_manifest_id = "webapps_integration/file_handler/basic.html",
      .app_name = "File Handler",
      .wco_not_enabled_title = u"File Handler",
      .icon_color = SK_ColorBLACK,
      .alternate_titles = {"File Handler - Text Handler",
                           "File Handler - Image Handler"}}},
    {Site::kNoServiceWorker,
     {.relative_url = "/webapps_integration/site_no_service_worker/basic.html",
      .relative_manifest_id =
          "webapps_integration/site_no_service_worker/basic.html",
      .app_name = "Site NoServiceWorker",
      .wco_not_enabled_title = u"Site NoServiceWorker",
      .icon_color = SK_ColorGREEN}},
    {Site::kNotInstalled,
     {.relative_url = "/webapps_integration/not_installed/basic.html",
      .relative_manifest_id = "webapps_integration/not_installed/basic.html",
      .app_name = "Not Installed",
      .wco_not_enabled_title = u"Not Installed",
      .icon_color = SK_ColorBLUE}},
    {Site::kStandaloneNotStartUrl,
     {.relative_url =
          "/webapps_integration/standalone/not_start_url/basic.html",
      .relative_manifest_id =
          "webapps_integration/standalone/not_start_url/basic.html",
      .app_name = "Not Start URL",
      .wco_not_enabled_title = u"Not Start URL",
      .icon_color = SK_ColorGREEN}},
    {Site::kScreenshots,
     {.relative_url = "/webapps_integration/screenshots/basic.html",
      .relative_manifest_id = "webapps_integration/screenshots/basic.html",
      .app_name = "Site With Screenshots",
      .wco_not_enabled_title = u"Site With Screenshots",
      .icon_color = SK_ColorGREEN}},
    {Site::kHasSubApps,
     {.relative_url = "/webapps_integration/has_sub_apps/basic.html",
      .relative_manifest_id = "webapps_integration/has_sub_apps/basic.html",
      .app_name = "Site With Sub Apps",
      .wco_not_enabled_title = u"Site With Sub Apps",
      .icon_color = SK_ColorGREEN,
      .is_isolated = true}},
    {Site::kSubApp1,
     {.relative_url = "/webapps_integration/has_sub_apps/sub_app1/basic.html",
      .relative_manifest_id =
          "webapps_integration/has_sub_apps/sub_app1/basic.html",
      .app_name = "Sub App 1",
      .wco_not_enabled_title = u"Sub App 1",
      .icon_color = SK_ColorBLUE,
      .parent_site = Site::kHasSubApps,
      .is_isolated = true}},
    {Site::kSubApp2,
     {.relative_url = "/webapps_integration/has_sub_apps/sub_app2/basic.html",
      .relative_manifest_id =
          "webapps_integration/has_sub_apps/sub_app2/basic.html",
      .app_name = "Sub App 2",
      .wco_not_enabled_title = u"Sub App 2",
      .icon_color = SK_ColorBLUE,
      .parent_site = Site::kHasSubApps,
      .is_isolated = true}},
    {Site::kChromeUrl,
     {.relative_url = "/webapps_integration/standalone/basic.html",
      .relative_manifest_id = "webapps_integration/standalone/basic.html",
      .app_name = "Site A",
      // WCO disabled is the defaulting state so the title when disabled
      // should match with the app's name.
      .wco_not_enabled_title = u"Site A",
      .icon_color = SK_ColorGREEN,
      .alternate_titles = {"Site A - Updated name"},
      .base_url = content::GetWebUIURLString("webapps_integration_tests")}},
};

struct DisplayConfig {
  std::string manifest_url_param;
};

base::flat_map<Display, DisplayConfig> g_display_configs = {
    {Display::kBrowser,
     {.manifest_url_param = "?manifest=manifest_browser.json"}},
    {Display::kMinimalUi,
     {.manifest_url_param = "?manifest=manifest_minimal_ui.json"}},
    {Display::kTabbed,
     {.manifest_url_param = "?manifest=manifest_tabbed.json"}},
    {Display::kStandalone, {.manifest_url_param = "?manifest=basic.json"}},
    {Display::kWco,
     {.manifest_url_param =
          "?manifest=manifest_window_controls_overlay.json"}}};

struct ScopeConfig {
  std::string manifest_url_param;
};

base::flat_map<Site, ScopeConfig> g_scope_configs = {
    {Site::kStandalone,
     {.manifest_url_param = "?manifest=manifest_scope_Standalone.json"}}};

ScopeConfig GetScopeUpdateConfiguration(Site scope) {
  CHECK(base::Contains(g_scope_configs, scope));
  return g_scope_configs.find(scope)->second;
}

DisplayConfig GetDisplayUpdateConfiguration(Display display) {
  CHECK(base::Contains(g_display_configs, display));
  return g_display_configs.find(display)->second;
}

SiteConfig GetSiteConfiguration(Site site) {
  CHECK(base::Contains(g_site_configs, site));
  return g_site_configs.find(site)->second;
}

std::string GetRelativeSubAppPath(Site sub_app) {
  SiteConfig sub_app_config = GetSiteConfiguration(sub_app);
  std::optional<Site> parent_site = sub_app_config.parent_site;
  CHECK(parent_site);
  std::string parent_app_path =
      GetSiteConfiguration(parent_site.value()).relative_url;
  parent_app_path.erase(parent_app_path.find_last_of("/"));
  std::string sub_app_path = sub_app_config.relative_url;
  sub_app_path.erase(0, parent_app_path.length());
  return sub_app_path;
}
std::string GetSiteId(Site site) {
  return base::NumberToString(base::to_underlying(site));
}

web_package::test::Ed25519KeyPair GetKeyPairForSite(Site site) {
  std::string site_id = GetSiteId(site);
  size_t seed_length = 32;
  site_id.resize(seed_length, 'a');
  base::span<const uint8_t> seed = base::as_bytes(base::make_span(site_id));

  uint8_t public_key[ED25519_PUBLIC_KEY_LEN];
  uint8_t private_key[ED25519_PRIVATE_KEY_LEN];
  ED25519_keypair_from_seed(public_key, private_key, seed.data());
  return web_package::test::Ed25519KeyPair(public_key, private_key);
}

std::string GetFileExtension(FileExtension file_extension) {
  switch (file_extension) {
    case FileExtension::kFoo:
      return "foo";
    case FileExtension::kBar:
      return "bar";
  }
  return std::string();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
SiteConfig GetSiteConfigurationFromAppName(const std::string& app_name) {
  SiteConfig config;
  bool is_app_found = false;
  for (auto const& [site, check_config] : g_site_configs) {
    if (check_config.app_name == app_name ||
        base::Contains(check_config.alternate_titles, app_name)) {
      config = check_config;
      is_app_found = true;
      break;
    }
  }
  CHECK(is_app_found) << "Could not find " << app_name;
  return config;
}
#endif

class BrowserAddedWaiter final : public BrowserListObserver {
 public:
  BrowserAddedWaiter() { BrowserList::AddObserver(this); }
  ~BrowserAddedWaiter() override { BrowserList::RemoveObserver(this); }

  void Wait(const base::Location& location = base::Location::Current()) {
    run_loop_.Run(location);
  }

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override {
    browser_added_ = browser;
    BrowserList::RemoveObserver(this);
    // Post a task to ensure the Remove event has been dispatched to all
    // observers.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_.QuitClosure());
  }
  Browser* browser_added() const { return browser_added_; }

 private:
  base::RunLoop run_loop_;
  raw_ptr<Browser> browser_added_ = nullptr;
};

class PageLoadWaiter final : public content::WebContentsObserver {
 public:
  explicit PageLoadWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    CHECK(web_contents);
  }
  ~PageLoadWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    run_loop_.Quit();
  }

  void WebContentsDestroyed() override {
    Observe(nullptr);
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

Browser* GetAppBrowserForAppId(const Profile* profile,
                               const webapps::AppId& app_id) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  for (auto it = browser_list->begin_browsers_ordered_by_activation();
       it != browser_list->end_browsers_ordered_by_activation(); ++it) {
    Browser* browser = *it;
    if (browser->profile() != profile) {
      continue;
    }
    if (AppBrowserController::IsForWebApp(browser, app_id)) {
      return browser;
    }
  }
  return nullptr;
}

bool AreAppBrowsersOpen(const Profile* profile, const webapps::AppId& app_id) {
  return GetAppBrowserForAppId(profile, app_id) != nullptr;
}

content::WebContents* GetAnyWebContentsForAppId(const webapps::AppId& app_id) {
  auto* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    for (int i = 0; i < browser->tab_strip_model()->GetTabCount(); i++) {
      content::WebContents* web_contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      const webapps::AppId* web_contents_id =
          WebAppTabHelper::GetAppId(web_contents);
      if (web_contents_id && *web_contents_id == app_id) {
        return web_contents;
      }
    }
  }
  return nullptr;
}

class UninstallCompleteWaiter final : public BrowserListObserver,
                                      public WebAppInstallManagerObserver {
 public:
  explicit UninstallCompleteWaiter(
      Profile* profile,
      const webapps::AppId& app_id,
      apps::Readiness readiness = apps::Readiness::kUninstalledByUser)
      : profile_(profile),
        app_id_(app_id),
        app_unregistration_waiter_(profile, app_id, readiness) {
    BrowserList::AddObserver(this);
    WebAppProvider* provider = WebAppProvider::GetForTest(profile);
    observation_.Observe(&provider->install_manager());
    uninstall_complete_ =
        provider->registrar_unsafe().GetAppById(app_id) == nullptr;
    MaybeFinishWaiting();
  }

  ~UninstallCompleteWaiter() override {
    BrowserList::RemoveObserver(this);
    observation_.Reset();
  }

  void Wait() {
    app_unregistration_waiter_.Await();
    run_loop_.Run();
  }

  // BrowserListObserver
  void OnBrowserRemoved(Browser* browser) override { MaybeFinishWaiting(); }

  // WebAppInstallManagerObserver
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override {
    if (app_id != app_id_) {
      return;
    }
    uninstall_complete_ = true;
    MaybeFinishWaiting();
  }

  void MaybeFinishWaiting() {
    if (!uninstall_complete_) {
      return;
    }
    if (AreAppBrowsersOpen(profile_, app_id_)) {
      return;
    }

    BrowserList::RemoveObserver(this);
    observation_.Reset();
    // Post a task to ensure the Remove event has been dispatched to all
    // observers.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_.QuitClosure());
  }

 private:
  raw_ptr<const Profile> profile_;
  const webapps::AppId app_id_;
  bool uninstall_complete_ = false;
  base::RunLoop run_loop_;
  apps::AppReadinessWaiter app_unregistration_waiter_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      observation_{this};
};

std::optional<ProfileState> GetStateForProfile(StateSnapshot* state_snapshot,
                                               Profile* profile) {
  CHECK(state_snapshot);
  CHECK(profile);
  auto it = state_snapshot->profiles.find(profile);
  return it == state_snapshot->profiles.end()
             ? std::nullopt
             : std::make_optional<ProfileState>(it->second);
}

std::optional<BrowserState> GetStateForBrowser(StateSnapshot* state_snapshot,
                                               Profile* profile,
                                               Browser* browser) {
  std::optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return std::nullopt;
  }

  auto it = profile_state->browsers.find(browser);
  return it == profile_state->browsers.end()
             ? std::nullopt
             : std::make_optional<BrowserState>(it->second);
}

std::optional<TabState> GetStateForActiveTab(BrowserState browser_state) {
  if (!browser_state.active_tab) {
    return std::nullopt;
  }

  auto it = browser_state.tabs.find(browser_state.active_tab);
  CHECK(it != browser_state.tabs.end());
  return std::make_optional<TabState>(it->second);
}

std::optional<AppState> GetStateForAppId(StateSnapshot* state_snapshot,
                                         Profile* profile,
                                         const webapps::AppId& id) {
  std::optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return std::nullopt;
  }

  auto it = profile_state->apps.find(id);
  return it == profile_state->apps.end()
             ? std::nullopt
             : std::make_optional<AppState>(it->second);
}

#if !BUILDFLAG(IS_CHROMEOS)
WebAppSettingsPageHandler CreateAppManagementPageHandler(Profile* profile) {
  mojo::PendingReceiver<app_management::mojom::Page> page;
  mojo::Remote<app_management::mojom::PageHandler> handler;
  static auto delegate =
      WebAppSettingsUI::CreateAppManagementPageHandlerDelegate(profile);
  return WebAppSettingsPageHandler(handler.BindNewPipeAndPassReceiver(),
                                   page.InitWithNewPipeAndPassRemote(), profile,
                                   *delegate);
}
#endif

void ActivateBrowserAndWait(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  CHECK(browser);
  ASSERT_TRUE(browser->window());
  auto waiter = ui_test_utils::BrowserActivationWaiter(browser);
  browser->window()->Activate();
  waiter.WaitForActivation();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void WaitForAndAcceptInstallDialogForSite(InstallableSite site) {
  std::string simple_dialog_name =
      base::FeatureList::IsEnabled(features::kWebAppUniversalInstall)
          ? "WebAppSimpleInstallDialog"
          : "PWAConfirmationBubbleView";
  std::string widget_name = site == InstallableSite::kScreenshots
                                ? "WebAppDetailedInstallDialog"
                                : simple_dialog_name;
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       widget_name);
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  views::test::AcceptDialog(widget);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)

// Clear any apps that may have been left in the Ash App Service cache by
// earlier tests.
void ReinitializeAppService(Profile* profile) {
  if (chromeos::IsAshVersionAtLeastForTesting(base::Version({108, 0, 5354}))) {
    base::test::TestFuture<void> future;
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::TestController>()
        ->ReinitializeAppService(future.GetCallback());
    ASSERT_TRUE(future.Wait());

    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->ReinitializeForTesting(profile);
    apps::AppTypeInitializationWaiter(profile, apps::AppType::kWeb).Await();
  } else {
    LOG(ERROR) << "Cannot ReinitializeAppService - Unsupported ash version.";
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Determines whether, when attempting to load a path, we want to, instead of
// using the regular handler, load it from a file on disk.
bool ShouldLoadResponseFromDisk(const base::FilePath& root,
                                const std::string& path) {
  const base::FilePath expanded = root.AppendASCII(path);
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool exists = base::PathExists(expanded);
  if (exists) {
    VLOG(1) << "Loading test data from " << expanded << " for " << path;
  } else {
    VLOG(1) << "Unable to load test data from " << expanded << " for " << path
            << ", as the file doesn't exist.";
  }
  return exists;
}

void LoadFileFromDisk(const base::FilePath& path,
                      content::WebUIDataSource::GotDataCallback callback) {
  std::string result;
  CHECK(base::ReadFileToString(path, &result));

  std::move(callback).Run(
      new base::RefCountedBytes(base::as_byte_span(result)));
}

void LoadResponseFromDisk(const base::FilePath& root,
                          const std::string& path,
                          content::WebUIDataSource::GotDataCallback callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(LoadFileFromDisk, root.AppendASCII(path),
                     std::move(callback)));
}

}  // anonymous namespace

BrowserState::BrowserState(
    Browser* browser_ptr,
    base::flat_map<content::WebContents*, TabState> tab_state,
    content::WebContents* active_web_contents,
    const webapps::AppId& app_id,
    bool launch_icon_visible)
    : browser(browser_ptr),
      tabs(std::move(tab_state)),
      active_tab(active_web_contents),
      app_id(app_id),
      launch_icon_shown(launch_icon_visible) {}
BrowserState::~BrowserState() = default;
BrowserState::BrowserState(const BrowserState&) = default;
bool BrowserState::operator==(const BrowserState& other) const {
  return browser == other.browser && tabs == other.tabs &&
         active_tab == other.active_tab && app_id == other.app_id &&
         launch_icon_shown == other.launch_icon_shown;
}

AppState::AppState(webapps::AppId app_id,
                   std::string app_name,
                   GURL app_scope,
                   apps::RunOnOsLoginMode run_on_os_login_mode,
                   blink::mojom::DisplayMode effective_display_mode,
                   std::optional<mojom::UserDisplayMode> user_display_mode,
                   std::string manifest_launcher_icon_filename,
                   bool installed_locally,
                   bool shortcut_created)
    : id(std::move(app_id)),
      name(std::move(app_name)),
      scope(std::move(app_scope)),
      run_on_os_login_mode(run_on_os_login_mode),
      effective_display_mode(effective_display_mode),
      user_display_mode(user_display_mode),
      manifest_launcher_icon_filename(
          std::move(manifest_launcher_icon_filename)),
      is_installed_locally(installed_locally),
      is_shortcut_created(shortcut_created) {}
AppState::~AppState() = default;
AppState::AppState(const AppState&) = default;
bool AppState::operator==(const AppState& other) const {
  return id == other.id && name == other.name && scope == other.scope &&
         run_on_os_login_mode == other.run_on_os_login_mode &&
         effective_display_mode == other.effective_display_mode &&
         user_display_mode == other.user_display_mode &&
         manifest_launcher_icon_filename ==
             other.manifest_launcher_icon_filename &&
         is_installed_locally == other.is_installed_locally &&
         is_shortcut_created == other.is_shortcut_created;
}

ProfileState::ProfileState(base::flat_map<Browser*, BrowserState> browser_state,
                           base::flat_map<webapps::AppId, AppState> app_state)
    : browsers(std::move(browser_state)), apps(std::move(app_state)) {}
ProfileState::~ProfileState() = default;
ProfileState::ProfileState(const ProfileState&) = default;
bool ProfileState::operator==(const ProfileState& other) const {
  return browsers == other.browsers && apps == other.apps;
}

StateSnapshot::StateSnapshot(
    base::flat_map<Profile*, ProfileState> profile_state)
    : profiles(std::move(profile_state)) {}
StateSnapshot::~StateSnapshot() = default;
StateSnapshot::StateSnapshot(const StateSnapshot&) = default;
bool StateSnapshot::operator==(const StateSnapshot& other) const {
  return profiles == other.profiles;
}

std::ostream& operator<<(std::ostream& os, const StateSnapshot& snapshot) {
  base::Value::Dict root;
  base::Value::Dict& profiles_dict = *root.EnsureDict("profiles");
  for (const auto& profile_pair : snapshot.profiles) {
    base::Value::Dict profile_dict;

    base::Value::Dict browsers_dict;
    const ProfileState& profile = profile_pair.second;
    for (const auto& browser_pair : profile.browsers) {
      base::Value::Dict browser_dict;
      const BrowserState& browser = browser_pair.second;

      browser_dict.Set("browser",
                       base::StringPrintf("%p", browser.browser.get()));

      base::Value::Dict tab_dicts;
      for (const auto& tab_pair : browser.tabs) {
        base::Value::Dict tab_dict;
        const TabState& tab = tab_pair.second;
        tab_dict.Set("url", tab.url.spec());
        tab_dicts.Set(base::StringPrintf("%p", tab_pair.first),
                      std::move(tab_dict));
      }
      browser_dict.Set("tabs", std::move(tab_dicts));
      browser_dict.Set("active_tab",
                       base::StringPrintf("%p", browser.active_tab.get()));
      browser_dict.Set("app_id", browser.app_id);
      browser_dict.Set("launch_icon_shown", browser.launch_icon_shown);

      browsers_dict.Set(base::StringPrintf("%p", browser_pair.first),
                        std::move(browser_dict));
    }
    base::Value::Dict app_dicts;
    for (const auto& app_pair : profile.apps) {
      base::Value::Dict app_dict;
      const AppState& app = app_pair.second;

      app_dict.Set("id", app.id);
      app_dict.Set("name", app.name);
      app_dict.Set("effective_display_mode",
                   static_cast<int>(app.effective_display_mode));
      app_dict.Set("user_display_mode",
                   static_cast<int>(app.effective_display_mode));
      app_dict.Set("manifest_launcher_icon_filename",
                   app.manifest_launcher_icon_filename);
      app_dict.Set("is_installed_locally", app.is_installed_locally);
      app_dict.Set("is_shortcut_created", app.is_shortcut_created);

      app_dicts.Set(app_pair.first, std::move(app_dict));
    }

    profile_dict.Set("browsers", std::move(browsers_dict));
    profile_dict.Set("apps", std::move(app_dicts));
    profiles_dict.Set(base::StringPrintf("%p", profile_pair.first),
                      std::move(profile_dict));
  }
  os << root.DebugString();
  return os;
}

WebAppIntegrationTestDriver::WebAppIntegrationTestDriver(TestDelegate* delegate)
    : delegate_(delegate),
      update_dialog_scope_(web_app::SetIdentityUpdateDialogActionForTesting(
          web_app::AppIdentityUpdate::kSkipped)) {}

WebAppIntegrationTestDriver::~WebAppIntegrationTestDriver() = default;

void WebAppIntegrationTestDriver::SetUp() {
  webapps::TestAppBannerManagerDesktop::SetUp();
  ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
}

void WebAppIntegrationTestDriver::SetUpOnMainThread() {
  override_registration_ = OsIntegrationTestOverrideImpl::OverrideForTesting();

  // Only support manifest updates on non-sync tests, as the current
  // infrastructure here only supports listening on one profile.
  if (!delegate_->IsSyncTest()) {
    observation_.Observe(&provider()->install_manager());
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ReinitializeAppService(browser()->profile());
#endif

  // Add chrome://webapps_integration_tests/ date source.
  auto root_path = base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(browser()->profile(),
                                             "webapps_integration_tests");
  valid_chrome_url_for_webapps_registration_ =
      AddValidWebAppChromeUrlHostForTesting("webapps_integration_tests");
  data_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc,
      "default-src * 'unsafe-eval' 'unsafe-inline'; ");
  data_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src * 'unsafe-inline' 'unsafe-eval'; ");
  data_source->DisableTrustedTypesCSP();
  data_source->SetRequestFilter(
      base::BindRepeating(&ShouldLoadResponseFromDisk, root_path),
      base::BindRepeating(LoadResponseFromDisk, root_path));

  web_app::test::WaitUntilReady(
      web_app::WebAppProvider::GetForTest(browser()->profile()));
}

void WebAppIntegrationTestDriver::TearDownOnMainThread() {
  in_tear_down_ = true;
  LOG(INFO) << "TearDownOnMainThread: Start.";
  observation_.Reset();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (delegate_->IsSyncTest()) {
    SyncTurnOff();
  }
#endif
  for (auto* profile : GetAllProfiles()) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    if (delegate_->IsSyncTest()) {
      delegate_->SyncSignOut(profile);
    }
#endif
    auto* provider = GetProviderForProfile(profile);
    if (!provider) {
      continue;
    }
    std::vector<webapps::AppId> app_ids =
        provider->registrar_unsafe().GetAppIds();
    for (auto& app_id : app_ids) {
      LOG(INFO) << "TearDownOnMainThread: Uninstalling " << app_id << ".";
      const WebApp* app = provider->registrar_unsafe().GetAppById(app_id);
      if (!app) {
        // This might happen if |app_id| was a sub-app of a previously
        // uninstalled app.
        LOG(INFO) << "TearDownOnMainThread: " << app_id
                  << " was already removed.";
        continue;
      }
      if (app->IsPolicyInstalledApp()) {
        UninstallPolicyAppById(profile, app_id);
      }
      if (provider->registrar_unsafe().IsInstalled(app_id)) {
        ASSERT_TRUE(app->CanUserUninstallWebApp());
        UninstallCompleteWaiter uninstall_waiter(profile, app_id);
        base::test::TestFuture<webapps::UninstallResultCode> future;
        provider->scheduler().RemoveUserUninstallableManagements(
            app_id, webapps::WebappUninstallSource::kAppsPage,
            future.GetCallback());
        EXPECT_TRUE(UninstallSucceeded(future.Get()));
        uninstall_waiter.Wait();
      }
      LOG(INFO) << "TearDownOnMainThread: Uninstall complete.";
    }
    // TODO(crbug.com/40206415): Investigate the true source of flakiness
    // instead of papering over it here.
    provider->command_manager().AwaitAllCommandsCompleteForTesting();
    FlushShortcutTasks();
  }
  LOG(INFO)
      << "TearDownOnMainThread: Destroying shortcut override and waiting.";
  override_registration_.reset();

  LOG(INFO) << "TearDownOnMainThread: Complete.";

  // Print debug information if there was a failure.
  if (testing::Test::HasFailure()) {
    base::TimeDelta log_time = base::TimeTicks::Now() - start_time_;
    test::LogDebugInfoToConsole(GetAllProfiles(), log_time);
  }
}

void WebAppIntegrationTestDriver::HandleAppIdentityUpdateDialogResponse(
    UpdateDialogResponse response) {
  // This is used to test the silent updating of policy installed apps
  // which do not trigger the manifest update dialog to be shown.
  if (response == UpdateDialogResponse::kSkipDialog) {
    return;
  }

  // Resetting the global test state for app identity update dialogs so that
  // tests can accept/cancel the app identity update dialog.
  update_dialog_scope_ =
      web_app::SetIdentityUpdateDialogActionForTesting(std::nullopt);
  views::Widget* manifest_update_widget =
      app_id_update_dialog_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(manifest_update_widget != nullptr);
  auto uninstall_dialog_view = std::make_unique<views::NamedWidgetShownWaiter>(
      views::test::AnyWidgetTestPasskey{}, "WebAppUninstallDialogDelegateView");
  views::Widget* uninstall_dialog_widget = nullptr;
  switch (response) {
    case UpdateDialogResponse::kAcceptUpdate:
      views::test::AcceptDialog(manifest_update_widget);
      break;
    case UpdateDialogResponse::kCancelDialogAndUninstall:
      manifest_update_widget->widget_delegate()
          ->AsDialogDelegate()
          ->CancelDialog();
      uninstall_dialog_widget = uninstall_dialog_view->WaitIfNeededAndGet();
      ASSERT_NE(uninstall_dialog_widget, nullptr);
      views::test::AcceptDialog(uninstall_dialog_widget);
      break;
    case UpdateDialogResponse::kCancelUninstallAndAcceptUpdate: {
      manifest_update_widget->widget_delegate()
          ->AsDialogDelegate()
          ->CancelDialog();
      uninstall_dialog_widget = uninstall_dialog_view->WaitIfNeededAndGet();
      ASSERT_NE(uninstall_dialog_widget, nullptr);
      views::test::WidgetDestroyedWaiter uninstall_destroyed(
          uninstall_dialog_widget);
      views::test::CancelDialog(uninstall_dialog_widget);
      uninstall_destroyed.Wait();
      views::test::AcceptDialog(manifest_update_widget);
      break;
    }
    case UpdateDialogResponse::kSkipDialog:
      NOTREACHED();
  }
}

void WebAppIntegrationTestDriver::AwaitManifestUpdate(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  const WebApp* web_app = provider()->registrar_unsafe().GetAppById(app_id);
  // If the update resulted in an uninstall, then no need to wait.
  if (web_app) {
    if (!previous_manifest_updates_.contains(app_id)) {
      waiting_for_update_id_ = app_id;
      waiting_for_update_run_loop_ = std::make_unique<base::RunLoop>();
      waiting_for_update_run_loop_->Run();
      waiting_for_update_run_loop_.reset();
    }

    // Wait for the app's scope in the App Service app cache to be consistent
    // with the app's scope in the web app database. Returns immediately if they
    // are already consistent.
    apps::WebAppScopeWaiter(profile(), app_id,
                            provider()->registrar_unsafe().GetAppScope(app_id))
        .Await();
  }
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::CloseCustomToolbar() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser());
  content::WebContents* web_contents = app_view->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(web_contents);
  EXPECT_TRUE(app_view->toolbar()
                  ->custom_tab_bar()
                  ->close_button_for_testing()
                  ->GetVisible());
  app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
  nav_observer.Wait();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ClosePwa() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser()) << "No current app browser";

  ui_test_utils::BrowserChangeObserver close_observer(
      app_browser(),
      ui_test_utils::BrowserChangeObserver::ChangeType::kRemoved);
  app_browser()->window()->Close();
  close_observer.Wait();
  app_browser_ = nullptr;

  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::MaybeClosePwa() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  if (app_browser()) {
    ClosePwa();
  }
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::DisableRunOnOsLoginFromAppSettings(
    Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
#if !BUILDFLAG(IS_CHROMEOS)
  auto app_management_page_handler = CreateAppManagementPageHandler(profile());
  app_management_page_handler.SetRunOnOsLoginMode(
      app_id, apps::RunOnOsLoginMode::kNotRun);
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::DisableRunOnOsLoginFromAppHome(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
#if !BUILDFLAG(IS_CHROMEOS)
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home_page_handler.SetRunOnOsLoginMode(app_id,
                                            web_app::RunOnOsLoginMode::kNotRun);
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::EnableRunOnOsLoginFromAppSettings(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
#if !BUILDFLAG(IS_CHROMEOS)
  auto app_management_page_handler = CreateAppManagementPageHandler(profile());
  app_management_page_handler.SetRunOnOsLoginMode(
      app_id, apps::RunOnOsLoginMode::kWindowed);
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::EnableRunOnOsLoginFromAppHome(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
#if !BUILDFLAG(IS_CHROMEOS)
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home_page_handler.SetRunOnOsLoginMode(
      app_id, web_app::RunOnOsLoginMode::kWindowed);
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::EnterFullScreenApp() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  FullscreenController* fullscreen_controller =
      app_browser()->exclusive_access_manager()->fullscreen_controller();
  ASSERT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  ui_test_utils::ToggleFullscreenModeAndWait(app_browser());
  ASSERT_TRUE(fullscreen_controller->IsFullscreenForBrowser());
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ExitFullScreenApp() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  FullscreenController* fullscreen_controller =
      app_browser()->exclusive_access_manager()->fullscreen_controller();
  ASSERT_TRUE(fullscreen_controller->IsFullscreenForBrowser());
  ui_test_utils::ToggleFullscreenModeAndWait(app_browser());
  ASSERT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::DisableFileHandling(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  SetFileHandlingEnabled(site, false);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::EnableFileHandling(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  SetFileHandlingEnabled(site, true);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::CreateShortcut(Site site,
                                                 WindowOptions options) {
  bool open_in_window = options == WindowOptions::kWindowed;

#if !BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/344912771): Remove tests that use the current create
  // shortcut flow once ShortcutsNotApps is launched to 100% Stable.
  if (base::FeatureList::IsEnabled(features::kShortcutsNotApps)) {
    GTEST_SKIP()
        << "Shortcuts are no longer web apps if kShortcutsNotApps is enabled";
  }
#endif

  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  MaybeNavigateTabbedBrowserInScope(site);

  SetAutoAcceptWebAppDialogForTesting(
      /*auto_accept=*/true,
      /*auto_open_in_window=*/open_in_window);
  WebAppTestInstallWithOsHooksObserver observer(profile());
  observer.BeginListening();
  BrowserAddedWaiter browser_added_waiter;
  CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
  active_app_id_ = observer.Wait();
  SetAutoAcceptWebAppDialogForTesting(false, false);
  if (open_in_window) {
    browser_added_waiter.Wait();
    app_browser_ = browser_added_waiter.browser_added();
    ASSERT_TRUE(app_browser_);
    ActivateBrowserAndWait(app_browser_);
  }
  apps::AppReadinessWaiter(profile(), active_app_id_).Await();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::InstallMenuOption(InstallableSite site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  MaybeNavigateTabbedBrowserInScope(InstallableSiteToSite(site));
  BrowserAddedWaiter browser_added_waiter;
  WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();
  auto dont_close_bubble_on_deactivate =
      web_app::SetDontCloseOnDeactivateForTesting();

  CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));

  CHECK_NE(site, InstallableSite::kScreenshots)
      << "Installing via menu option with detailed dialog not supported, as "
         "waiting for a worker is impossible here. https://crbug.com/1368324.";
  WaitForAndAcceptInstallDialogForSite(site);

  browser_added_waiter.Wait();
  app_browser_ = browser_added_waiter.browser_added();
  ASSERT_TRUE(app_browser_);
  active_app_id_ = install_observer.Wait();
  ActivateBrowserAndWait(app_browser_);
  apps::AppReadinessWaiter(profile(), active_app_id_).Await();
  AfterStateChangeAction();
}

#if !BUILDFLAG(IS_CHROMEOS)
void WebAppIntegrationTestDriver::InstallLocally(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);

  WebAppTestInstallWithOsHooksObserver observer(profile());
  observer.BeginListening();
  app_home_page_handler.InstallAppLocally(app_id);
  observer.Wait();
  apps::AppReadinessWaiter(profile(), app_id).Await();
  AfterStateChangeAction();
}
#endif

void WebAppIntegrationTestDriver::InstallOmniboxIcon(InstallableSite site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  MaybeNavigateTabbedBrowserInScope(InstallableSiteToSite(site));

  auto* app_banner_manager =
      webapps::TestAppBannerManagerDesktop::FromWebContents(
          GetCurrentTab(browser()));
  app_banner_manager->WaitForInstallableCheck();

  webapps::AppId app_id;
  base::RunLoop run_loop;
  web_app::SetInstalledCallbackForTesting(base::BindLambdaForTesting(
      [&app_id, &run_loop](const webapps::AppId& installed_app_id,
                           webapps::InstallResultCode code) {
        app_id = installed_app_id;
        run_loop.Quit();
      }));

  auto dont_close_bubble_on_deactivate =
      web_app::SetDontCloseOnDeactivateForTesting();

  BrowserAddedWaiter browser_added_waiter;
  ASSERT_TRUE(pwa_install_view()->GetVisible());
  WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();
  pwa_install_view()->ExecuteForTesting();

  WaitForAndAcceptInstallDialogForSite(site);

  run_loop.Run();
  browser_added_waiter.Wait();
  app_browser_ = browser_added_waiter.browser_added();
  ASSERT_TRUE(app_browser_);
  active_app_id_ = install_observer.Wait();
  ASSERT_EQ(app_id, active_app_id_);
  ActivateBrowserAndWait(app_browser_);
  apps::AppReadinessWaiter(profile(), active_app_id_).Await();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::InstallPolicyApp(Site site,
                                                   ShortcutOptions shortcut,
                                                   WindowOptions window,
                                                   InstallMode mode) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  base::Value container = base::Value(window == WindowOptions::kWindowed
                                          ? kDefaultLaunchContainerWindowValue
                                          : kDefaultLaunchContainerTabValue);
  InstallPolicyAppInternal(
      site, std::move(container),
      /*create_shortcut=*/shortcut == ShortcutOptions::kWithShortcut,
      /*install_as_shortcut=*/mode == InstallMode::kWebShortcut);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::InstallPreinstalledApp(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  // Many CUJs rely on operating on an opened window / tab after installation,
  // and this state is true for all installations except for policy install. To
  // help keep CUJs combined for all installs, do a navigation here.
  MaybeNavigateTabbedBrowserInScope(site);
  GURL url = GetUrlForSite(site);
  WebAppTestInstallObserver observer(profile());
  observer.BeginListening();

  constexpr char kAppConfigTemplate[] =
      R"({
        "app_url": "$1",
        "launch_container": "window",
        "user_type": ["unmanaged"]
      })";
  std::string app_config = base::ReplaceStringPlaceholders(
      kAppConfigTemplate, {url.spec()}, nullptr);
  SyncAndInstallPreinstalledAppConfig(url, app_config);
  active_app_id_ = observer.Wait();
  apps::AppReadinessWaiter(profile(), active_app_id_).Await();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::InstallIsolatedApp(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }

  // The site is installed as an Isolated Web App by packaging the directory of
  // the site into a Signed Web Bundle.
  // The scope and manifest ID of an Isolated Web App are always a unique
  // isolated-app:// origin based on the signing key of the app.

  auto builder = TestSignedWebBundleBuilder(GetKeyPairForSite(site));
  auto app_folder =
      base::FilePath::FromASCII(GetSiteConfiguration(site).relative_manifest_id)
          .DirName();
  builder.AddFilesFromFolder(GetTestDataDir().Append(app_folder));
  TestSignedWebBundle bundle = builder.Build();
  auto bundle_file_name = base::FilePath::FromASCII(bundle.id.id() + ".swbn");
  base::FilePath bundle_path =
      scoped_temp_dir_.GetPath().Append(bundle_file_name);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(bundle_path, bundle.data));
  }

  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle.id);
  webapps::AppId app_id = url_info.app_id();

  SetTrustedWebBundleIdsForTesting({bundle.id});

  {
    base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                          InstallIsolatedWebAppCommandError>>
        future;
    provider()->scheduler().InstallIsolatedWebApp(
        url_info,
        IsolatedWebAppInstallSource::FromGraphicalInstaller(
            IwaSourceBundleProdModeWithFileOp(
                bundle_path, IwaSourceBundleProdFileOp::kCopy)),
        base::Version("1.0.0"),
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    auto install_result = future.Take();
    ASSERT_TRUE(install_result.has_value()) << install_result.error();
  }

  LaunchWebAppBrowserAndWait(profile(), app_id,
                             WindowOpenDisposition::NEW_WINDOW);
  active_app_id_ = app_id;
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::InstallSubApp(
    Site parent_app,
    Site sub_app,
    SubAppInstallDialogOptions option) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }

  auto dialog_action =
      SubAppsInstallDialogController::SetAutomaticActionForTesting(
          (option == SubAppInstallDialogOptions::kUserDeny)
              ? SubAppsInstallDialogController::DialogActionForTesting::kCancel
              : SubAppsInstallDialogController::DialogActionForTesting::
                    kAccept);

  content::WebContents* web_contents =
      GetAnyWebContentsForAppId(GetAppIdBySiteMode(parent_app));
  ASSERT_TRUE(web_contents)
      << "No open tab or window for the parent app was found.";

  std::string sub_url = GetRelativeSubAppPath(sub_app);

  // The argument of add() is a dictionary-valued dictionary:
  // { $manifest_id : {'installURL' : $installURL} }
  // In our case, both $manifest_id and $installURL are sub_url.
  base::Value::Dict inner_dict;
  inner_dict.Set("installURL", sub_url);
  base::Value::Dict outer_dict;
  outer_dict.Set(sub_url, std::move(inner_dict));

  std::string script =
      content::JsReplace("navigator.subApps.add($1)", std::move(outer_dict));
  const content::EvalJsResult add_result =
      content::EvalJs(web_contents, script);

  if (option == SubAppInstallDialogOptions::kUserDeny) {
    EXPECT_FALSE(add_result.error.empty());
  } else {
    base::Value::Dict expected_output;
    expected_output.Set(sub_url, "success");
    EXPECT_EQ(expected_output, add_result.value);
  }

  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::RemoveSubApp(Site parent_app, Site sub_app) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  content::WebContents* web_contents =
      GetAnyWebContentsForAppId(GetAppIdBySiteMode(parent_app));
  ASSERT_TRUE(web_contents)
      << "No open tab or window for the parent app was found.";
  std::string sub_url = GetRelativeSubAppPath(sub_app);

  const base::Value& remove_result =
      content::EvalJs(
          web_contents,
          content::JsReplace("navigator.subApps.remove([$1])", sub_url))
          .value;

  base::Value::Dict expected_output;
  expected_output.Set(sub_url, "success");
  EXPECT_EQ(expected_output, remove_result);

  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::EnableWindowControlsOverlay(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser());

  ASSERT_FALSE(app_view->IsWindowControlsOverlayEnabled());
  content::TitleWatcher title_watcher(
      app_view->GetActiveWebContents(),
      GetSiteConfiguration(site).wco_not_enabled_title + u": WCO Enabled");
  base::test::TestFuture<void> future;
  app_view->ToggleWindowControlsOverlayEnabled(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  std::ignore = title_watcher.WaitAndGetTitle();
  ASSERT_TRUE(app_view->IsWindowControlsOverlayEnabled());
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::DisableWindowControlsOverlay(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser());

  ASSERT_TRUE(app_view->IsWindowControlsOverlayEnabled());
  content::TitleWatcher title_watcher(
      app_view->GetActiveWebContents(),
      GetSiteConfiguration(site).wco_not_enabled_title);
  base::test::TestFuture<void> future;
  app_view->ToggleWindowControlsOverlayEnabled(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  std::ignore = title_watcher.WaitAndGetTitle();
  ASSERT_FALSE(app_view->IsWindowControlsOverlayEnabled());
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ApplyRunOnOsLoginPolicyAllowed(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ApplyRunOnOsLoginPolicy(site, kAllowed);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ApplyRunOnOsLoginPolicyBlocked(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ApplyRunOnOsLoginPolicy(site, kBlocked);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ApplyRunOnOsLoginPolicyRunWindowed(
    Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ApplyRunOnOsLoginPolicy(site, kRunWindowed);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::RemoveRunOnOsLoginPolicy(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  base::RunLoop run_loop;
  WebAppProvider::GetForTest(profile())
      ->policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          run_loop.QuitClosure());
  GURL url = GetUrlForSite(site);
  {
    ScopedListPrefUpdate update_list(profile()->GetPrefs(),
                                     prefs::kWebAppSettings);
    update_list->EraseIf([&](const base::Value& item) {
      return *item.GetDict().FindString(kManifestId) == url.spec();
    });
  }
  run_loop.Run();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::LaunchFileExpectDialog(
    Site site,
    FilesOptions files_options,
    AllowDenyOptions allow_deny,
    AskAgainOptions ask_again) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "FileHandlerLaunchDialogView");
  FileHandlerLaunchDialogView::SetDefaultRememberSelectionForTesting(
      ask_again == AskAgainOptions::kRemember);

  base::RunLoop run_loop;
  BrowserAddedWaiter browser_added_waiter;
#if BUILDFLAG(IS_MAC)
  apps::SetMacShimStartupDoneCallbackForTesting(run_loop.QuitClosure());
#else
  web_app::startup::SetStartupDoneCallbackForTesting(run_loop.QuitClosure());
#endif

  LaunchFile(site, files_options);

  // Check the file handling dialog shows up.
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(widget != nullptr);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  views::Widget::ClosedReason close_reason;
  if (allow_deny == AllowDenyOptions::kDeny) {
    close_reason = views::Widget::ClosedReason::kCancelButtonClicked;
    if (ask_again == AskAgainOptions::kRemember) {
      site_remember_deny_open_file_.emplace(site);
    }
  } else {
    close_reason = views::Widget::ClosedReason::kAcceptButtonClicked;
  }
  // File handling dialog should be destroyed after choosing the action.
  widget->CloseWithReason(close_reason);
  destroyed_waiter.Wait();
  run_loop.Run();

  // TODO(cliffordcheng): Wait for multiple browsers and
  //                      support multiple client file handling.
  DisplayMode display_mode =
      provider()->registrar_unsafe().GetAppEffectiveDisplayMode(app_id);
  if ((display_mode != blink::mojom::DisplayMode::kBrowser) &&
      (allow_deny != AllowDenyOptions::kDeny)) {
    browser_added_waiter.Wait();
    app_browser_ = browser_added_waiter.browser_added();
  }

  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::LaunchFileExpectNoDialog(
    Site site,
    FilesOptions files_options) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  base::RunLoop run_loop;
  BrowserAddedWaiter browser_added_waiter;
#if BUILDFLAG(IS_MAC)
  apps::SetMacShimStartupDoneCallbackForTesting(run_loop.QuitClosure());
#else
  web_app::startup::SetStartupDoneCallbackForTesting(run_loop.QuitClosure());
#endif

  LaunchFile(site, files_options);

  // If the user previously denied access to open files with this app, a window
  // is still opened for the app. The only difference is that no files would
  // have been passed to the app. Either way, we should always wait for a
  // window / tab to be added.
  run_loop.Run();

  // TODO(cliffordcheng): Wait for multiple browsers and
  //                      support multiple client file handling.
  DisplayMode display_mode =
      provider()->registrar_unsafe().GetAppEffectiveDisplayMode(app_id);
  if (display_mode != blink::mojom::DisplayMode::kBrowser) {
    browser_added_waiter.Wait();
    app_browser_ = browser_added_waiter.browser_added();
  }

  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::LaunchFromChromeApps(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  WebAppRegistrar& app_registrar = provider()->registrar_unsafe();
  const DisplayMode display_mode =
      app_registrar.GetAppEffectiveDisplayMode(app_id);
  const bool is_open_in_app_browser =
      (display_mode != blink::mojom::DisplayMode::kBrowser);
#if BUILDFLAG(IS_CHROMEOS)
  if (is_open_in_app_browser) {
    app_browser_ = LaunchWebAppBrowserAndWait(profile(), app_id);
    active_app_id_ = app_id;
  } else {
    ui_test_utils::UrlLoadObserver url_observer(
        app_registrar.GetAppLaunchUrl(app_id));
    LaunchBrowserForWebAppInTab(profile(), app_id);
    url_observer.Wait();
  }
#else
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  webapps::AppHomePageHandler app_home_page_handler =
      GetTestAppHomePageHandler(&test_web_ui);

  auto event_ptr = app_home::mojom::ClickEvent::New();
  event_ptr->button = 0.0;
  event_ptr->alt_key = false;
  event_ptr->ctrl_key = false;
  event_ptr->meta_key = false;
  event_ptr->shift_key = false;

  BrowserAddedWaiter browser_added_waiter;
  ui_test_utils::UrlLoadObserver url_observer(
      app_registrar.GetAppLaunchUrl(app_id));
  app_home_page_handler.LaunchApp(app_id, std::move(event_ptr));
  url_observer.Wait();

  // The app_browser_ is needed only for apps that open in a new window.
  if (is_open_in_app_browser) {
    browser_added_waiter.Wait();
    app_browser_ = browser_added_waiter.browser_added();
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser(), app_id));
  }
  active_app_id_ = app_id;
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::LaunchFromLaunchIcon(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  base::AutoReset<bool> intent_picker_bubble_scope =
      IntentPickerBubbleView::SetAutoAcceptIntentPickerBubbleForTesting();
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  NavigateTabbedBrowserToSite(GetInScopeURL(site), NavigationMode::kNewTab);

  BrowserAddedWaiter browser_added_waiter;

  EXPECT_TRUE(ClickIntentPickerChip(browser()));
  browser_added_waiter.Wait();
  app_browser_ = browser_added_waiter.browser_added();
  ASSERT_TRUE(app_browser_);
  ActivateBrowserAndWait(app_browser_);
  ASSERT_TRUE(app_browser_->is_type_app());
  ASSERT_TRUE(AppBrowserController::IsForWebApp(app_browser_, app_id));
  active_app_id_ = app_browser()->app_controller()->app_id();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::LaunchFromMenuOption(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  NavigateTabbedBrowserToSite(GetInScopeURL(site), NavigationMode::kNewTab);

  BrowserAddedWaiter browser_added_waiter;
  CHECK(chrome::ExecuteCommand(browser(), IDC_OPEN_IN_PWA_WINDOW));
  browser_added_waiter.Wait();
  app_browser_ = browser_added_waiter.browser_added();
  ASSERT_TRUE(app_browser_);
  ActivateBrowserAndWait(app_browser_);
  active_app_id_ = app_id;

  ASSERT_TRUE(AppBrowserController::IsForWebApp(app_browser(), active_app_id_));
  EXPECT_EQ(app_browser()->app_controller()->app_id(), app_id);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::LaunchFromPlatformShortcut(Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  WebAppRegistrar& app_registrar = provider()->registrar_unsafe();
  DisplayMode display_mode = app_registrar.GetAppEffectiveDisplayMode(app_id);
  bool is_open_in_app_browser =
      (display_mode != blink::mojom::DisplayMode::kBrowser);
#if BUILDFLAG(IS_MAC)
  if (is_open_in_app_browser) {
    BrowserAddedWaiter browser_added_waiter;
    // If there already is an open app browser for this app the launch is not
    // expected to open a new one, so only wait for a new browser to be added
    // if there wasn't an open one already.
    app_browser_ = GetAppBrowserForAppId(profile(), app_id);
    bool had_open_browsers = false;
    for (auto* profile : GetAllProfiles()) {
      auto* provider = GetProviderForProfile(profile);
      if (!provider) {
        continue;
      }
      if (provider->ui_manager().GetNumWindowsForApp(app_id) > 0) {
        had_open_browsers = true;
      }
    }
    base::RunLoop run_loop;
    apps::SetMacShimStartupDoneCallbackForTesting(run_loop.QuitClosure());
    ASSERT_TRUE(LaunchFromAppShim(site, /*urls=*/{},
                                  /*wait_for_complete_launch=*/true));
    run_loop.Run();
    if (!app_browser_ && !had_open_browsers) {
      browser_added_waiter.Wait();
      app_browser_ = browser_added_waiter.browser_added();
    }
    if (app_browser_) {
      active_app_id_ = app_id;
      EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser(), app_id));
    }
  } else {
    base::RunLoop run_loop;
    apps::SetMacShimStartupDoneCallbackForTesting(run_loop.QuitClosure());
    ASSERT_TRUE(LaunchFromAppShim(site, /*urls=*/{},
                                  /*wait_for_complete_launch=*/true));
    run_loop.Run();
  }
#else
  if (is_open_in_app_browser) {
    BrowserAddedWaiter browser_added_waiter;
    LaunchAppStartupBrowserCreator(app_id);
    browser_added_waiter.Wait();
    app_browser_ = browser_added_waiter.browser_added();
    ActivateBrowserAndWait(app_browser_);
    active_app_id_ = app_id;
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser(), app_id));
  } else {
    LaunchAppStartupBrowserCreator(app_id);
  }
#endif
  AfterStateChangeAction();
#endif
}

#if BUILDFLAG(IS_MAC)
void WebAppIntegrationTestDriver::LaunchFromAppShimFallback(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }

  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  WebAppRegistrar& app_registrar = provider()->registrar_unsafe();
  DisplayMode display_mode = app_registrar.GetAppEffectiveDisplayMode(app_id);
  bool is_open_in_app_browser =
      (display_mode != blink::mojom::DisplayMode::kBrowser);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, app_id);
  command_line.AppendSwitchASCII(switches::kTestType, "browser");
  command_line.AppendSwitchASCII(switches::kProfileDirectory, "");

  if (is_open_in_app_browser) {
    BrowserAddedWaiter browser_added_waiter;
    // This should have similar logic to the IS_MAC branch in
    // LaunchFromPlatformShortcut, however currently launching from app shim
    // fallback actually uses the non-mac launch code, so for now that is what
    // this is expecting.
    ASSERT_TRUE(ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
        command_line, /*current_directory=*/{}));
    content::RunAllTasksUntilIdle();
    browser_added_waiter.Wait();
    app_browser_ = browser_added_waiter.browser_added();
    active_app_id_ = app_id;
    EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser(), app_id));
  } else {
    ASSERT_TRUE(ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
        command_line, /*current_directory=*/{}));
    content::RunAllTasksUntilIdle();
  }
  AfterStateChangeAction();
}
#endif

void WebAppIntegrationTestDriver::OpenAppSettingsFromAppMenu(Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  Browser* app_browser = GetAppBrowserForSite(site);
  ASSERT_TRUE(app_browser);

  // Click App info from app browser.
  CHECK(chrome::ExecuteCommand(app_browser, IDC_WEB_APP_MENU_APP_INFO));

  content::WebContentsAddedObserver nav_observer;

  // Click settings from page info bubble.
  views::Widget* page_info_bubble =
      PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetWidget();
  EXPECT_TRUE(page_info_bubble);

  views::View* settings_button = page_info_bubble->GetRootView()->GetViewByID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS);

  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  settings_button->HandleAccessibleAction(data);

  // Wait for new web content to be created.
  nav_observer.GetWebContents();

  AfterStateChangeAction();
#endif
}

void WebAppIntegrationTestDriver::OpenAppSettingsFromChromeApps(Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(web_contents);
  test_web_ui.set_web_contents(web_contents);
  webapps::AppHomePageHandler app_home_page_handler =
      GetTestAppHomePageHandler(&test_web_ui);
  content::WebContentsAddedObserver nav_observer;
  app_home_page_handler.ShowAppSettings(app_id);
  // Wait for new web contents to be created.
  nav_observer.GetWebContents();
  AfterStateChangeAction();
#endif
}

void WebAppIntegrationTestDriver::OpenAppSettingsFromCommand(Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  Browser* app_browser = GetAppBrowserForSite(site);
  ASSERT_TRUE(app_browser);

  content::WebContentsAddedObserver nav_observer;

  // Click App Settings from app browser.
  CHECK(chrome::ExecuteCommand(app_browser, IDC_WEB_APP_SETTINGS));
  // Wait for new web content to be created.
  nav_observer.GetWebContents();
  AfterStateChangeAction();
#endif
}

void WebAppIntegrationTestDriver::CreateShortcutsFromList(Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else  // !BUILDFLAG(IS_CHROMEOS)
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(web_contents);
  test_web_ui.set_web_contents(web_contents);
  webapps::AppHomePageHandler app_home_page_handler =
      GetTestAppHomePageHandler(&test_web_ui);
  base::test::TestFuture<void> shortcuts_future;
#if BUILDFLAG(IS_MAC)
  app_home_page_handler.CreateAppShortcut(app_id,
                                          shortcuts_future.GetCallback());
#else   // !BUILDFLAG(IS_MAC)
  CreateChromeApplicationShortcutViewWaiter waiter;
  app_home_page_handler.CreateAppShortcut(app_id,
                                          shortcuts_future.GetCallback());
  FlushShortcutTasks();
  std::move(waiter).WaitForAndAccept();
#endif  // BUILDFLAG(IS_MAC)
  EXPECT_TRUE(shortcuts_future.Wait());
  AfterStateChangeAction();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void WebAppIntegrationTestDriver::DeletePlatformShortcut(Site site) {
  if (!before_state_change_action_state_ && !after_state_change_action_state_) {
    return;
  }
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  base::ScopedAllowBlockingForTesting allow_blocking;
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  std::string app_name = provider()->registrar_unsafe().GetAppShortName(app_id);
  if (app_name.empty()) {
    app_name = GetSiteConfiguration(site).app_name;
  }
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  ASSERT_TRUE(override_registration_->test_override().IsShortcutCreated(
      profile(), app_id, app_name));
  ASSERT_TRUE(
      override_registration_->test_override().SimulateDeleteShortcutsByUser(
          profile(), app_id, app_name));
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::CheckAppSettingsAppState(
    Profile* profile,
    const AppState& app_state) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  auto app_management_page_handler = CreateAppManagementPageHandler(profile);

  app_management::mojom::AppPtr app;
  app_management_page_handler.GetApp(
      app_state.id,
      base::BindLambdaForTesting([&](app_management::mojom::AppPtr result) {
        app = std::move(result);
      }));

  EXPECT_EQ(app->id, app_state.id);
  EXPECT_EQ(app->title.value(), app_state.name);
  ASSERT_TRUE(app->run_on_os_login.has_value());
  EXPECT_EQ(app->run_on_os_login.value()->login_mode,
            app_state.run_on_os_login_mode);
#endif
}

base::FilePath WebAppIntegrationTestDriver::GetResourceFile(
    base::FilePath::StringPieceType relative_path) {
  base::FilePath base_dir;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &base_dir)) {
    return base::FilePath();
  }
  base::FilePath full_path = base_dir.Append(relative_path);
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    if (!PathExists(full_path)) {
      return base::FilePath();
    }
  }
  return full_path;
}

std::vector<base::FilePath> WebAppIntegrationTestDriver::GetTestFilePaths(
    FilesOptions files_options) {
  std::vector<base::FilePath> file_paths;
  base::FilePath foo_file_path = GetResourceFile(
      FILE_PATH_LITERAL("webapps_integration/files/file_handler_test.foo"));
  base::FilePath bar_file_path = GetResourceFile(
      FILE_PATH_LITERAL("webapps_integration/files/file_handler_test.bar"));
  switch (files_options) {
    case FilesOptions::kOneFooFile:
      file_paths.push_back(foo_file_path);
      break;
    case FilesOptions::kMultipleFooFiles:
      file_paths.push_back(foo_file_path);
      file_paths.push_back(foo_file_path);
      break;
    case FilesOptions::kOneBarFile:
      file_paths.push_back(bar_file_path);
      break;
    case FilesOptions::kMultipleBarFiles:
      file_paths.push_back(bar_file_path);
      file_paths.push_back(bar_file_path);
      break;
    case FilesOptions::kAllFooAndBarFiles:
      file_paths.push_back(foo_file_path);
      file_paths.push_back(bar_file_path);
      break;
  }
  return file_paths;
}

// TODO(b/240449120): Remove for testing behavior when preinstalled app
// CUJs are implemented.
void WebAppIntegrationTestDriver::SyncAndInstallPreinstalledAppConfig(
    const GURL& install_url,
    std::string_view app_config_string) {
  base::AutoReset<bool> bypass_offline_manifest_requirement =
      PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();
  base::FilePath test_config_dir = GetResourceFile(
      FILE_PATH_LITERAL("webapps_integration/preinstalled_config_dir/"));
  web_app::SetPreinstalledWebAppConfigDirForTesting(&test_config_dir);

  base::Value::List app_configs;
  auto json_parse_result =
      base::JSONReader::ReadAndReturnValueWithError(app_config_string);
  EXPECT_TRUE(json_parse_result.has_value())
      << "JSON parse error: " << json_parse_result.error().message;
  if (!json_parse_result.has_value()) {
    return;
  }
  app_configs.Append(std::move(*json_parse_result));
  base::AutoReset<const base::Value::List*> configs_for_testing =
      PreinstalledWebAppManager::SetConfigsForTesting(&app_configs);

  using InstallAppsResults =
      std::map<GURL, web_app::ExternallyManagedAppManager::InstallResult>;
  using UninstallAppsResults = std::map<GURL, webapps::UninstallResultCode>;
  base::test::TestFuture<InstallAppsResults, UninstallAppsResults> test_future;
  provider()->preinstalled_web_app_manager().LoadAndSynchronizeForTesting(
      test_future.GetCallback());
  EXPECT_TRUE(test_future.Wait());
  web_app::SetPreinstalledWebAppConfigDirForTesting(nullptr);
}

void WebAppIntegrationTestDriver::NavigateAppHome() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  GURL app_home_url = GURL(chrome::kChromeUIAppsURL);
  WindowOpenDisposition win_disposition;
  content::TestNavigationObserver url_observer(app_home_url);
  if (BrowserList::IsOffTheRecordBrowserInUse(browser()->profile())) {
    win_disposition = WindowOpenDisposition::OFF_THE_RECORD;
    url_observer.StartWatchingNewWebContents();
  } else {
    win_disposition = WindowOpenDisposition::CURRENT_TAB;
    url_observer.WatchExistingWebContents();
  }
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), app_home_url, win_disposition,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  url_observer.Wait();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::NavigateBrowser(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  NavigateTabbedBrowserToSite(GetInScopeURL(site), NavigationMode::kCurrentTab);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::NavigatePwa(Site pwa, Site to) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  app_browser_ = GetAppBrowserForSite(pwa);
  ASSERT_TRUE(app_browser_);

  content::TestNavigationObserver url_observer(GetUrlForSite(to));
  url_observer.StartWatchingNewWebContents();
  url_observer.WatchExistingWebContents();
  NavigateViaLinkClickToURLAndWait(app_browser(), GetUrlForSite(to), false);
  url_observer.Wait();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::NavigateNotfoundUrl() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  NavigateTabbedBrowserToSite(
      delegate_->EmbeddedTestServer()->GetURL("/non-existant/index.html"),
      NavigationMode::kCurrentTab);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::NewAppTab(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  chrome::NewTab(GetAppBrowserForSite(site));
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ManifestUpdateIcon(
    Site site,
    UpdateDialogResponse response) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ASSERT_EQ(Site::kStandalone, site)
      << "Only site mode of 'Standalone' is supported";

  app_id_update_dialog_waiter_ =
      std::make_unique<views::NamedWidgetShownWaiter>(
          views::test::AnyWidgetTestPasskey{},
          "WebAppIdentityUpdateConfirmationView");

  // The kLauncherIcon size is used here, as it is guaranteed to be written to
  // the shortcut on all platforms, as opposed to kInstallIconSize, for example,
  // which, on ChromeOS, is not written to the shortcut because it is not within
  // the intersection between `kDesiredIconSizesForShortcut` (which is platform-
  // dependent) and `SizesToGenerate()` (which is fixed on all platforms).
  GURL url = GetUrlForSite(
      site, base::StringPrintf("?manifest=manifest_icon_red_%u.json",
                               kLauncherIconSize));

  ForceUpdateManifestContents(site, url);
  HandleAppIdentityUpdateDialogResponse(response);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ManifestUpdateTitle(
    Site site,
    Title title,
    UpdateDialogResponse response) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ASSERT_EQ(Site::kStandalone, site)
      << "Only site mode of 'Standalone' is supported";
  ASSERT_EQ(Title::kStandaloneUpdated, title)
      << "Only site mode of 'kStandaloneUpdated' is supported";

  app_id_update_dialog_waiter_ =
      std::make_unique<views::NamedWidgetShownWaiter>(
          views::test::AnyWidgetTestPasskey{},
          "WebAppIdentityUpdateConfirmationView");

  auto relative_url_path = GetSiteConfiguration(site).relative_url;
  GURL url = GetUrlForSite(site, "?manifest=manifest_title.json");
  ForceUpdateManifestContents(site, url);
  HandleAppIdentityUpdateDialogResponse(response);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ManifestUpdateDisplay(Site site,
                                                        Display display) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }

  std::string relative_url_path = GetSiteConfiguration(site).relative_url;
  std::string manifest_url_param =
      GetDisplayUpdateConfiguration(display).manifest_url_param;
  GURL url = GetUrlForSite(site, manifest_url_param);

  ForceUpdateManifestContents(site, url);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::ManifestUpdateScopeTo(Site app, Site scope) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  // The `scope_mode` would be changing the scope set in the manifest file. For
  // simplicity, right now only Standalone is supported, so that is just
  // hardcoded in manifest_scope_Standalone.json, which is specified in the URL.
  auto relative_url_path = GetSiteConfiguration(app).relative_url;
  GURL url =
      GetUrlForSite(app, GetScopeUpdateConfiguration(scope).manifest_url_param);
  ForceUpdateManifestContents(app, url);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::OpenInChrome() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(IsBrowserOpen(app_browser())) << "No current app browser.";
  webapps::AppId app_id = app_browser()->app_controller()->app_id();
  GURL app_url = GetCurrentTab(app_browser())->GetURL();
  ASSERT_TRUE(AppBrowserController::IsForWebApp(app_browser(), app_id));
  CHECK(chrome::ExecuteCommand(app_browser_, IDC_OPEN_IN_CHROME));
  ui_test_utils::WaitForBrowserToClose(app_browser());
  ASSERT_FALSE(IsBrowserOpen(app_browser())) << "App browser should be closed.";
  app_browser_ = nullptr;
  EXPECT_EQ(GetCurrentTab(browser())->GetURL(), app_url);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SetOpenInTabFromAppHome(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
#if BUILDFLAG(IS_CHROMEOS)
  auto& sync_bridge =
      WebAppProvider::GetForTest(profile())->sync_bridge_unsafe();
  sync_bridge.SetAppUserDisplayModeForTesting(
      app_id, mojom::UserDisplayMode::kStandalone);
  apps::AppWindowModeWaiter(profile(), app_id, apps::WindowMode::kWindow)
      .Await();
#else
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home_page_handler.SetUserDisplayMode(
      app_id, web_app::mojom::UserDisplayMode::kBrowser);
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SetOpenInTabFromAppSettings(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
#if BUILDFLAG(IS_CHROMEOS)
  auto& sync_bridge =
      WebAppProvider::GetForTest(profile())->sync_bridge_unsafe();
  sync_bridge.SetAppUserDisplayModeForTesting(app_id,
                                              mojom::UserDisplayMode::kBrowser);
  apps::AppWindowModeWaiter(profile(), app_id, apps::WindowMode::kBrowser)
      .Await();
#else
  auto app_management_page_handler = CreateAppManagementPageHandler(profile());
  app_management_page_handler.SetWindowMode(app_id, apps::WindowMode::kBrowser);
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SetOpenInWindowFromAppHome(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
#if BUILDFLAG(IS_CHROMEOS)
  auto& sync_bridge =
      WebAppProvider::GetForTest(profile())->sync_bridge_unsafe();
  sync_bridge.SetAppUserDisplayModeForTesting(
      app_id, mojom::UserDisplayMode::kStandalone);
  apps::AppWindowModeWaiter(profile(), app_id, apps::WindowMode::kWindow)
      .Await();
#else
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home_page_handler.SetUserDisplayMode(
      app_id, web_app::mojom::UserDisplayMode::kStandalone);
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SetOpenInWindowFromAppSettings(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
  ;
  // Will need to add feature flag based condition for web app settings page.
#if BUILDFLAG(IS_CHROMEOS)
  auto& sync_bridge =
      WebAppProvider::GetForTest(profile())->sync_bridge_unsafe();
  sync_bridge.SetAppUserDisplayModeForTesting(
      app_id, mojom::UserDisplayMode::kStandalone);
  apps::AppWindowModeWaiter(profile(), app_id, apps::WindowMode::kWindow)
      .Await();
#else
  auto app_management_page_handler = CreateAppManagementPageHandler(profile());
  app_management_page_handler.SetWindowMode(app_id, apps::WindowMode::kWindow);
#endif
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SwitchIncognitoProfile() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  content::WebContentsAddedObserver nav_observer;
  CHECK(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  ASSERT_EQ(1U, BrowserList::GetIncognitoBrowserCount());
  nav_observer.GetWebContents();
  std::vector<Profile*> otr_profiles = profile()->GetAllOffTheRecordProfiles();
  CHECK(!otr_profiles.empty());
  active_profile_ = otr_profiles.back();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SwitchProfileClients(ProfileClient client) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  CHECK(active_profile_);
  active_profile_ = delegate_->GetProfileClient(client);
  CHECK(active_profile_)
      << "Cannot switch profile clients if delegate only supports one profile";
  delegate_->AwaitWebAppQuiescence();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SwitchActiveProfile(
    ProfileName profile_name) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  active_profile_ = GetOrCreateProfile(profile_name);
  // Make sure the profile has at least one browser by creating one if one
  // doesn't exist already.
  if (!chrome::FindTabbedBrowser(active_profile_,
                                 /*match_original_profiles=*/false)) {
    delegate_->CreateBrowser(active_profile_);
  }
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SyncTurnOff() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  delegate_->SyncTurnOff();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SyncTurnOn() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  delegate_->SyncTurnOn();
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SyncSignOut() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  delegate_->SyncSignOut(active_profile_);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::SyncSignIn() {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  delegate_->SyncSignIn(active_profile_);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::UninstallFromList(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  UninstallCompleteWaiter uninstall_waiter(profile(), app_id);
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::AppServiceProxy* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  base::RunLoop run_loop;
  app_service_proxy->UninstallForTesting(
      app_id, nullptr,
      base::BindLambdaForTesting([&](bool) { run_loop.Quit(); }));
  run_loop.Run();

  ASSERT_NE(nullptr, AppUninstallDialogView::GetActiveViewForTesting());
  AppUninstallDialogView::GetActiveViewForTesting()->AcceptDialog();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // The lacros implementation doesn't use a confirmation dialog so we can
  // call the normal method.
  apps::AppServiceProxy* app_service_proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  app_service_proxy->Uninstall(app_id, apps::UninstallSource::kAppList,
                               nullptr);
#else
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home_page_handler.UninstallApp(app_id);
#endif
  uninstall_waiter.Wait();
  site_remember_deny_open_file_.erase(site);

  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::UninstallFromAppSettings(Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  UninstallCompleteWaiter uninstall_waiter(profile(), app_id);

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  if (web_contents->GetURL() !=
      GURL(chrome::kChromeUIWebAppSettingsURL + app_id)) {
    OpenAppSettingsFromChromeApps(site);
    CheckBrowserNavigationIsAppSettings(site);
  }

  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  auto app_management_page_handler = CreateAppManagementPageHandler(profile());
  app_management_page_handler.Uninstall(app_id);

  uninstall_waiter.Wait();

  // Wait for app settings page to be closed.
  destroyed_watcher.Wait();

  site_remember_deny_open_file_.erase(site);

  AfterStateChangeAction();
#endif
}

void WebAppIntegrationTestDriver::UninstallFromMenu(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  UninstallCompleteWaiter uninstall_waiter(profile(), app_id);
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  Browser* app_browser = GetAppBrowserForSite(site);
  ASSERT_TRUE(app_browser);
  auto app_menu_model =
      std::make_unique<WebAppMenuModel>(/*provider=*/nullptr, app_browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  size_t index = 0;
  const bool found = app_menu_model->GetModelAndIndexForCommandId(
      WebAppMenuModel::kUninstallAppCommandId, &model, &index);
  EXPECT_TRUE(found);
  EXPECT_TRUE(model->IsEnabledAt(index));

  app_menu_model->ExecuteCommand(WebAppMenuModel::kUninstallAppCommandId,
                                 /*event_flags=*/0);
  // The |app_menu_model| must be destroyed here, as the |observer| waits
  // until the app is fully uninstalled, which includes closing and deleting
  // the app_browser.
  app_menu_model.reset();
  uninstall_waiter.Wait();
  site_remember_deny_open_file_.erase(site);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::UninstallPolicyApp(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  GURL url = GetUrlForSite(site);
  auto policy_app = GetAppBySiteMode(before_state_change_action_state_.get(),
                                     profile(), site);
  ASSERT_TRUE(policy_app);
  base::RunLoop run_loop;

  UninstallCompleteWaiter uninstall_waiter(
      profile(), policy_app->id, apps::Readiness::kUninstalledByNonUser);
  WebAppInstallManagerObserverAdapter observer(profile());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& app_id) {
        if (policy_app->id == app_id) {
          run_loop.Quit();
        }
      }));
  // If there are still install sources, the app might not be fully uninstalled,
  // so this will listen for the removal of the policy install source.
  observer.SetWebAppSourceRemovedDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& app_id) {
        if (policy_app->id == app_id) {
          run_loop.Quit();
        }
      }));
  {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kWebAppInstallForceList);
    size_t removed_count = update->EraseIf([&](const base::Value& item) {
      const base::Value* url_value = item.GetDict().Find(kUrlKey);
      return url_value && url_value->GetString() == url.spec();
    });
    ASSERT_GT(removed_count, 0U);
  }
  run_loop.Run();
  const WebApp* app = provider()->registrar_unsafe().GetAppById(policy_app->id);
  // If the app was fully uninstalled, wait for the change to propagate through
  // App Service.
  if (app == nullptr) {
    uninstall_waiter.Wait();
  }
  site_remember_deny_open_file_.erase(site);
  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::UninstallFromOs(Site site) {
#if BUILDFLAG(IS_WIN)
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);

  UninstallCompleteWaiter uninstall_waiter(profile(), app_id);

  // Trigger app uninstall via command line.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kUninstallAppId, app_id);
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, {},
      {profile()->GetPath(), StartupProfileModeReason::kAppRequested});

  uninstall_waiter.Wait();
  site_remember_deny_open_file_.erase(site);
  AfterStateChangeAction();
#else
  NOTREACHED() << "Not supported on non-Windows platforms";
#endif
}

#if BUILDFLAG(IS_MAC)
void WebAppIntegrationTestDriver::CorruptAppShim(Site site,
                                                 AppShimCorruption corruption) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  base::ScopedAllowBlockingForTesting allow_blocking;
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  std::string app_name = GetSiteConfiguration(site).app_name;
  base::FilePath app_path = GetShortcutPath(
      override_registration_->test_override().chrome_apps_folder(), app_name,
      app_id);
  base::FilePath bin_path = app_path.AppendASCII("Contents")
                                .AppendASCII("MacOS")
                                .AppendASCII("app_mode_loader");

  switch (corruption) {
    case AppShimCorruption::kNoExecutable:
      EXPECT_TRUE(base::DeleteFile(bin_path));
      break;
    case AppShimCorruption::kIncompatibleVersion: {
      // Find and replace the entry point symbol in the app shim executable with
      // something that definitely doesn't exist in the Chrome framework.
      std::string bin_contents;
      EXPECT_TRUE(base::ReadFileToString(bin_path, &bin_contents));
      auto pos = bin_contents.find(APP_SHIM_ENTRY_POINT_NAME_STRING);
      ASSERT_NE(pos, std::string::npos);
      bin_contents[pos] = 'D';
      EXPECT_TRUE(base::WriteFile(bin_path, bin_contents));

      // Since we modified the binary, we need to re-sign it.
      if (base::mac::MacOSMajorVersion() >= 12) {
        std::string codesign_output;
        std::vector<std::string> codesign_argv = {
            "codesign", "--force", "--sign", "-", bin_path.value()};
        EXPECT_TRUE(base::GetAppOutputAndError(base::CommandLine(codesign_argv),
                                               &codesign_output))
            << "Failed to sign executable at " << bin_path << ": "
            << codesign_output;
      }
      break;
    }
  }

  AfterStateChangeAction();
}

void WebAppIntegrationTestDriver::QuitAppShim(Site site) {
  if (!BeforeStateChangeAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  std::string app_name = GetSiteConfiguration(site).app_name;
  base::FilePath app_path = GetShortcutPath(
      override_registration_->test_override().chrome_apps_folder(), app_name,
      app_id);

  if (AppBrowserController::IsForWebApp(app_browser_, app_id)) {
    app_browser_ = nullptr;
  }

  WaitForShimToQuitForTesting(app_path, app_id, /*terminate=*/true);
  AfterStateChangeAction();
}
#endif

void WebAppIntegrationTestDriver::CheckAppListEmpty() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<ProfileState> state =
      GetStateForProfile(after_state_change_action_state_.get(), profile());
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->apps.empty());
#if !BUILDFLAG(IS_CHROMEOS)
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  base::test::TestFuture<std::vector<app_home::mojom::AppInfoPtr>>
      result_future;
  app_home_page_handler.GetApps(result_future.GetCallback());
  EXPECT_TRUE(
      result_future.Get<std::vector<app_home::mojom::AppInfoPtr>>().empty());
#endif
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppInListIconCorrect(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  GURL icon_url;
  int icon_size_to_test = icon_size::k128;
#if !BUILDFLAG(IS_CHROMEOS)
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home::mojom::AppInfoPtr expected_app;
  expected_app = app_home_page_handler.GetApp(active_app_id_);

  EXPECT_NE(expected_app, app_home::mojom::AppInfoPtr());
  icon_url = expected_app->icon_url;
  icon_size_to_test = icon_size::k64;
#else
  icon_url = apps::AppIconSource::GetIconURL(active_app_id_, icon_size::k128);
#endif
  SkBitmap icon_bitmap;
  base::RunLoop run_loop;

  NavigateTabbedBrowserToSite(icon_url, NavigationMode::kNewTab);
  content::WebContents* web_contents_active =
      browser()->tab_strip_model()->GetActiveWebContents();

  web_contents_active->DownloadImage(
      icon_url, false, gfx::Size(), 0, false,
      base::BindLambdaForTesting([&](int id, int http_status_code,
                                     const GURL& image_url,
                                     const std::vector<SkBitmap>& bitmaps,
                                     const std::vector<gfx::Size>& sizes) {
        EXPECT_EQ(200, http_status_code);
        ASSERT_EQ(bitmaps.size(), 1u);
        icon_bitmap = bitmaps[0];
        run_loop.Quit();
      }));
  run_loop.Run();

  SkColor expected_color = GetSiteConfiguration(site).icon_color;
  // Compare the center pixel color instead of top left corner
  // The app list icon has a filter that changes the color at the corner.
  EXPECT_EQ(expected_color,
            icon_bitmap.getColor(icon_size_to_test / 2, icon_size_to_test / 2));
  chrome::CloseTab(browser());
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppInListNotLocallyInstalled(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  // Note: This is a partially supported action.
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_FALSE(app_state->is_installed_locally);
#if !BUILDFLAG(IS_CHROMEOS)
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home::mojom::AppInfoPtr expected_app;
  const webapps::AppId app_id = GetAppIdBySiteMode(site);
  expected_app = app_home_page_handler.GetApp(app_id);

  EXPECT_NE(expected_app, app_home::mojom::AppInfoPtr());
  EXPECT_FALSE(expected_app->is_locally_installed);
#endif
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppInListWindowed(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  // Note: This is a partially supported action.
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(app_state->user_display_mode, mojom::UserDisplayMode::kStandalone);
#if !BUILDFLAG(IS_CHROMEOS)
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home::mojom::AppInfoPtr expected_app;
  const webapps::AppId app_id = GetAppIdBySiteMode(site);
  expected_app = app_home_page_handler.GetApp(app_id);

  EXPECT_NE(expected_app, app_home::mojom::AppInfoPtr());
  EXPECT_TRUE(expected_app->open_in_window);
#endif
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppInListTabbed(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  // Note: This is a partially supported action.
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(app_state->user_display_mode, mojom::UserDisplayMode::kBrowser);
#if !BUILDFLAG(IS_CHROMEOS)
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home::mojom::AppInfoPtr expected_app;
  const webapps::AppId app_id = GetAppIdBySiteMode(site);
  expected_app = app_home_page_handler.GetApp(app_id);

  EXPECT_NE(expected_app, app_home::mojom::AppInfoPtr());
  EXPECT_FALSE(expected_app->open_in_window);
#endif
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppNavigation(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  GURL url =
      app_browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();
  EXPECT_EQ(GetUrlForSite(site), url);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppNavigationIsStartUrl() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_FALSE(active_app_id_.empty());
  ASSERT_TRUE(app_browser());
  GURL url =
      app_browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();
  EXPECT_EQ(url, provider()->registrar_unsafe().GetAppStartUrl(active_app_id_));
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppTabIsSite(Site site, Number number) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  ASSERT_TRUE(
      app_browser()->tab_strip_model()->ContainsIndex(NumberToInt(number)));
  GURL url = app_browser()
                 ->tab_strip_model()
                 ->GetWebContentsAt(NumberToInt(number))
                 ->GetURL();
  EXPECT_EQ(url, GetUrlForSite(site));

  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppTabCreated() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  CHECK(before_state_change_action_state_);
  std::optional<BrowserState> most_recent_browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), app_browser());
  std::optional<BrowserState> previous_browser_state = GetStateForBrowser(
      before_state_change_action_state_.get(), profile(), app_browser());
  ASSERT_TRUE(most_recent_browser_state.has_value());

  ASSERT_TRUE(previous_browser_state.has_value());
  ASSERT_EQ(most_recent_browser_state->tabs.size(),
            previous_browser_state->tabs.size() + 1);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckBrowserNavigation(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(browser());
  GURL url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_EQ(url, GetUrlForSite(site));
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckBrowserNavigationIsAppSettings(
    Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
  ;

  ASSERT_TRUE(browser());
  GURL url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_EQ(url, GURL(chrome::kChromeUIWebAppSettingsURL + app_id));
  AfterStateCheckAction();
#endif
}

void WebAppIntegrationTestDriver::CheckBrowserNotAtAppHome() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  GURL current_url =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL();
  EXPECT_NE(current_url, GURL(chrome::kChromeUIAppsURL));
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppNotInList(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  EXPECT_FALSE(app_state.has_value());
#if !BUILDFLAG(IS_CHROMEOS)
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);
  app_home::mojom::AppInfoPtr expected_app;
  const webapps::AppId app_id = GetAppIdBySiteMode(site);
  expected_app = app_home_page_handler.GetApp(app_id);

  // An empty app received means that the app does not exist in chrome://apps.
  EXPECT_EQ(expected_app, app_home::mojom::AppInfoPtr());
#endif
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckPlatformShortcutAndIcon(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state);
  EXPECT_TRUE(app_state->is_shortcut_created);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckPlatformShortcutNotExists(Site site) {
  // This is to handle if the check happens at the very beginning of the test,
  // when no web app is installed (or any other action has happened yet).
  if (!before_state_change_action_state_ && !after_state_change_action_state_) {
    return;
  }
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  if (!app_state) {
    app_state = GetAppBySiteMode(before_state_change_action_state_.get(),
                                 profile(), site);
  }
  std::string app_name;
  webapps::AppId app_id;
  // If app_state is still nullptr, the site is manually mapped to get an
  // app_name and app_id remains empty.
  if (!app_state) {
    app_name = GetSiteConfiguration(site).app_name;
  } else {
    app_name = app_state->name;
    app_id = app_state->id;
  }
  EXPECT_FALSE(IsShortcutAndIconCreated(profile(), app_name, app_id));
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppIcon(Site site, Color color) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state);
  std::string color_str;
  switch (color) {
    case Color::kGreen:
      color_str = "green";
      break;
    case Color::kRed:
      color_str = "red";
      break;
  }
  EXPECT_EQ(app_state->manifest_launcher_icon_filename,
            base::StringPrintf("%ux%u-%s.png", kLauncherIconSize,
                               kLauncherIconSize, color_str.c_str()));

  // A mapping of image sizes to shortcut colors. Note that the top left
  // pixel color for each size is used as the representation color for that
  // size, even if the image is multi-colored.
  std::map<int, SkColor> shortcut_colors;

  base::RunLoop shortcut_run_loop;
  provider()->os_integration_manager().GetShortcutInfoForAppFromRegistrar(
      active_app_id_, base::BindLambdaForTesting(
                          [&](std::unique_ptr<ShortcutInfo> shortcut_info) {
                            if (shortcut_info) {
                              gfx::ImageFamily::const_iterator it;
                              for (it = shortcut_info->favicon.begin();
                                   it != shortcut_info->favicon.end(); ++it) {
                                shortcut_colors[it->Size().width()] =
                                    it->AsBitmap().getColor(0, 0);
                              }
                            }

                            shortcut_run_loop.Quit();
                          }));
  shortcut_run_loop.Run();

  SkColor launcher_icon_color = shortcut_colors[kLauncherIconSize];
  SkColor expected_color;
  switch (color) {
    case Color::kGreen:
      expected_color = SK_ColorGREEN;
      break;
    case Color::kRed:
      expected_color = SK_ColorRED;
      break;
  }
  EXPECT_EQ(expected_color, launcher_icon_color)
      << "Size " << kLauncherIconSize << ": Expecting ARGB " << std::hex
      << expected_color << " but found " << std::hex << launcher_icon_color;

  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppTitle(Site site, Title title) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state);
  std::string expected;
  switch (title) {
    case Title::kStandaloneOriginal:
      expected = "Site A";
      break;
    case Title::kStandaloneUpdated:
      expected = "Site A - Updated name";
      break;
  }
  EXPECT_EQ(app_state->name, expected);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckCreateShortcutNotShown() {
#if !BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/344912771): Remove tests that use the current create
  // shortcut flow once ShortcutsNotApps is launched to 100% Stable.
  if (base::FeatureList::IsEnabled(features::kShortcutsNotApps)) {
    GTEST_SKIP()
        << "Shortcuts are no longer web apps if kShortcutsNotApps is enabled";
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kDisabled);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckCreateShortcutShown() {
#if !BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/344912771): Remove tests that use the current create
  // shortcut flow once ShortcutsNotApps is launched to 100% Stable.
  if (base::FeatureList::IsEnabled(features::kShortcutsNotApps)) {
    GTEST_SKIP()
        << "Shortcuts are no longer web apps if kShortcutsNotApps is enabled";
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  EXPECT_EQ(GetAppMenuCommandState(IDC_CREATE_SHORTCUT, browser()), kEnabled);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckFilesLoadedInSite(
    Site site,
    FilesOptions files_options) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }

  std::vector<std::string> expected_foo_files;
  std::vector<std::string> expected_bar_files;
  std::vector<std::string> found_foo_files;
  std::vector<std::string> found_bar_files;
  const std::string foo_file_extension = GetFileExtension(FileExtension::kFoo);
  const std::string bar_file_extension = GetFileExtension(FileExtension::kBar);

  std::vector<base::FilePath> file_paths = GetTestFilePaths(files_options);
  for (const base::FilePath& path : file_paths) {
    std::string content;
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    base::ReadFileToString(path, &content);

    if (base::EndsWith(path.AsUTF8Unsafe(), foo_file_extension)) {
      expected_foo_files.push_back(content);
    } else if (base::EndsWith(path.AsUTF8Unsafe(), bar_file_extension)) {
      expected_bar_files.push_back(content);
    }
  }

  auto* browser_list = BrowserList::GetInstance();
  // Opening multiple files at the same time can result in multiple app windows.
  // All browser windows are checked.
  for (Browser* browser : *browser_list) {
    for (int i = 0; i < browser->tab_strip_model()->GetTabCount(); i++) {
      auto site_config = GetSiteConfiguration(site);
      content::WebContents* web_contents =
          browser->tab_strip_model()->GetWebContentsAt(i);

      if (!WebAppTabHelper::GetAppId(web_contents)) {
        continue;
      }

      static const std::string kFooHandler = "foo_handler.html";
      static const std::string kBarHandler = "bar_handler.html";
      webapps::AppId app_id = *WebAppTabHelper::GetAppId(web_contents);
      std::string url_str = web_contents->GetURL().spec();

      if (app_id != GetAppIdBySiteMode(site) ||
          !(base::EndsWith(url_str, kFooHandler) ||
            base::EndsWith(url_str, kBarHandler))) {
        continue;
      }

      base::Value test_contents =
          EvalJs(web_contents, "launchFinishedPromise").ExtractList();
      auto& test_content_list = test_contents.GetList();

      for (const auto& test_content : test_content_list) {
        if (base::EndsWith(url_str, kFooHandler)) {
          found_foo_files.push_back(test_content.GetString());
        } else {
          CHECK(base::EndsWith(url_str, kBarHandler));
          found_bar_files.push_back(test_content.GetString());
        }
      }
    }
  }
  ASSERT_THAT(expected_foo_files,
              ::testing::UnorderedElementsAreArray(found_foo_files));
  ASSERT_THAT(expected_bar_files,
              ::testing::UnorderedElementsAreArray(found_bar_files));
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckInstallIconShown() {
  // Currently this function does not support tests that check install icons
  // for sites that have a manifest but no service worker.
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  content::WebContents* web_contents = GetCurrentTab(browser());
  if (webapps::AppBannerManagerDesktop::FromWebContents(web_contents)) {
    auto* app_banner_manager =
        webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents);
    app_banner_manager->WaitForInstallableCheck();
  }
  EXPECT_TRUE(pwa_install_view()->GetVisible());
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckInstallIconNotShown() {
  // Currently this function does not support tests that check install icons
  // for sites that have a manifest but no service worker.
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  content::WebContents* web_contents = GetCurrentTab(browser());
  if (webapps::AppBannerManagerDesktop::FromWebContents(web_contents)) {
    auto* app_banner_manager =
        webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents);
    app_banner_manager->WaitForInstallableCheck();
  }
  EXPECT_FALSE(pwa_install_view()->GetVisible());
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckLaunchIconShown() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<BrowserState> browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_TRUE(browser_state->launch_icon_shown);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckLaunchIconNotShown() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<BrowserState> browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_FALSE(browser_state->launch_icon_shown);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckTabCreated(Number number) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  CHECK(before_state_change_action_state_);
  std::optional<BrowserState> most_recent_browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  std::optional<BrowserState> previous_browser_state = GetStateForBrowser(
      before_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(most_recent_browser_state.has_value());
#if BUILDFLAG(IS_MAC)
  if (!previous_browser_state.has_value()) {
    // The tab was created in a new browser, which is expected if the original
    // browser lived on a non-primary display.
  } else {
#endif
    ASSERT_TRUE(previous_browser_state.has_value());
    EXPECT_GE(most_recent_browser_state->tabs.size(),
              previous_browser_state->tabs.size());
    int tab_diff = most_recent_browser_state->tabs.size() -
                   previous_browser_state->tabs.size();
    ASSERT_EQ(NumberToInt(number), tab_diff);
#if BUILDFLAG(IS_MAC)
  }
#endif

  std::optional<TabState> active_tab =
      GetStateForActiveTab(most_recent_browser_state.value());
  ASSERT_TRUE(active_tab.has_value());
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckTabNotCreated() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  CHECK(before_state_change_action_state_);
  std::optional<BrowserState> most_recent_browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  std::optional<BrowserState> previous_browser_state = GetStateForBrowser(
      before_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(most_recent_browser_state.has_value());
  ASSERT_TRUE(previous_browser_state.has_value());
  EXPECT_EQ(most_recent_browser_state->tabs.size(),
            previous_browser_state->tabs.size());
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckCustomToolbar() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  EXPECT_TRUE(app_browser()->app_controller()->ShouldShowCustomTabBar());
  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser());
  EXPECT_TRUE(app_view->toolbar()
                  ->custom_tab_bar()
                  ->close_button_for_testing()
                  ->GetVisible());
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckNoToolbar() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  EXPECT_FALSE(app_browser()->app_controller()->ShouldShowCustomTabBar());
  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser());
  EXPECT_FALSE(app_view->toolbar()->custom_tab_bar()->GetVisible());
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckRunOnOsLoginEnabled(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state);
  EXPECT_EQ(app_state->run_on_os_login_mode, apps::RunOnOsLoginMode::kWindowed);
  base::ScopedAllowBlockingForTesting allow_blocking;
#if BUILDFLAG(IS_WIN)
  ASSERT_TRUE(override_registration_->test_override().IsRunOnOsLoginEnabled(
      profile(), app_state->id, app_state->name));
  SiteConfig site_config = GetSiteConfigurationFromAppName(app_state->name);
  std::optional<SkColor> icon_color =
      override_registration_->test_override().GetShortcutIconTopLeftColor(
          profile(), override_registration_->test_override().startup(),
          app_state->id, app_state->name);
  ASSERT_TRUE(icon_color.has_value());
  ASSERT_THAT(site_config.icon_color, testing::Eq(icon_color.value()));
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  ASSERT_TRUE(override_registration_->test_override().IsRunOnOsLoginEnabled(
      profile(), app_state->id, app_state->name));
#endif
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckRunOnOsLoginDisabled(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state);
  base::ScopedAllowBlockingForTesting allow_blocking;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  ASSERT_FALSE(override_registration_->test_override().IsRunOnOsLoginEnabled(
      profile(), app_state->id, app_state->name));
#endif
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckSiteHandlesFile(
    Site site,
    FileExtension file_extension) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  std::string app_name = GetSiteConfiguration(site).app_name;
  std::string file_extension_str = "." + GetFileExtension(file_extension);
  ASSERT_TRUE(override_registration_->test_override().IsFileExtensionHandled(
      profile(), app_id, app_name, file_extension_str));
  AfterStateCheckAction();
#endif
}

void WebAppIntegrationTestDriver::CheckSiteNotHandlesFile(
    Site site,
    FileExtension file_extension) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  std::string app_name = GetSiteConfiguration(site).app_name;
  std::string file_extension_str = "." + GetFileExtension(file_extension);
  ASSERT_FALSE(override_registration_->test_override().IsFileExtensionHandled(
      profile(), app_id, app_name, file_extension_str));
  AfterStateCheckAction();
#endif
}

void WebAppIntegrationTestDriver::CheckUserCannotSetRunOnOsLoginAppSettings(
    Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state);
  auto app_management_page_handler = CreateAppManagementPageHandler(profile());

  app_management::mojom::AppPtr app;
  app_management_page_handler.GetApp(
      app_state->id,
      base::BindLambdaForTesting([&](app_management::mojom::AppPtr result) {
        app = std::move(result);
      }));

  ASSERT_TRUE(app->run_on_os_login.has_value());
  ASSERT_TRUE(app->run_on_os_login.value()->is_managed);
  if (app_state->run_on_os_login_mode == apps::RunOnOsLoginMode::kWindowed) {
    DisableRunOnOsLoginFromAppSettings(site);
    CheckRunOnOsLoginEnabled(site);
  } else {
    EnableRunOnOsLoginFromAppSettings(site);
    CheckRunOnOsLoginDisabled(site);
  }
  AfterStateCheckAction();
#endif
}

void WebAppIntegrationTestDriver::CheckUserCannotSetRunOnOsLoginAppHome(
    Site site) {
#if BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "Not implemented on Chrome OS.";
#else
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state =
      GetAppBySiteMode(after_state_change_action_state_.get(), profile(), site);
  ASSERT_TRUE(app_state);
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  CHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  auto app_home_page_handler = GetTestAppHomePageHandler(&test_web_ui);

  app_home::mojom::AppInfoPtr app;
  app = app_home_page_handler.GetApp(app_state->id);

  ASSERT_NE(app, app_home::mojom::AppInfoPtr());
  bool run_on_os_login_mode_set =
      (app->run_on_os_login_mode == web_app::RunOnOsLoginMode::kWindowed) ||
      (app->run_on_os_login_mode == web_app::RunOnOsLoginMode::kNotRun);
  EXPECT_TRUE(run_on_os_login_mode_set);
  EXPECT_FALSE(app->may_toggle_run_on_os_login_mode);
  if (app_state->run_on_os_login_mode == apps::RunOnOsLoginMode::kWindowed) {
    DisableRunOnOsLoginFromAppHome(site);
    CheckRunOnOsLoginEnabled(site);
  } else {
    EnableRunOnOsLoginFromAppHome(site);
    CheckRunOnOsLoginDisabled(site);
  }
  AfterStateCheckAction();
#endif
}

void WebAppIntegrationTestDriver::CheckUserDisplayModeInternal(
    mojom::UserDisplayMode user_display_mode) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  std::optional<AppState> app_state = GetStateForAppId(
      after_state_change_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(user_display_mode, app_state->user_display_mode);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowClosed() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  CHECK(before_state_change_action_state_);
  std::optional<ProfileState> after_action_profile =
      GetStateForProfile(after_state_change_action_state_.get(), profile());
  std::optional<ProfileState> before_action_profile =
      GetStateForProfile(before_state_change_action_state_.get(), profile());
  ASSERT_TRUE(after_action_profile.has_value());
  ASSERT_TRUE(before_action_profile.has_value());
  EXPECT_LT(after_action_profile->browsers.size(),
            before_action_profile->browsers.size());
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowCreated() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  CHECK(before_state_change_action_state_);
  std::optional<ProfileState> after_action_profile =
      GetStateForProfile(after_state_change_action_state_.get(), profile());
  std::optional<ProfileState> before_action_profile =
      GetStateForProfile(before_state_change_action_state_.get(), profile());
  ASSERT_TRUE(after_action_profile.has_value());
  ASSERT_TRUE(before_action_profile.has_value());
  ASSERT_GT(after_action_profile->browsers.size(),
            before_action_profile->browsers.size())
      << "Before: \n"
      << *before_state_change_action_state_ << "\nAfter:\n"
      << *after_state_change_action_state_;
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckPwaWindowCreated(Site site,
                                                        Number number) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  CheckPwaWindowCreatedImpl(profile(), site, number);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckPwaWindowCreatedImpl(Profile* profile,
                                                            Site site,
                                                            Number number) {
  CHECK(before_state_change_action_state_);
  std::optional<ProfileState> after_action_profile =
      GetStateForProfile(after_state_change_action_state_.get(), profile);
  std::optional<ProfileState> before_action_profile =
      GetStateForProfile(before_state_change_action_state_.get(), profile);
  ASSERT_TRUE(after_action_profile.has_value());
  ASSERT_TRUE(before_action_profile.has_value());

  int before_state_app_window_count = 0;
  int after_state_app_window_count = 0;
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  for (const auto& browser_pair : before_action_profile->browsers) {
    if (AppBrowserController::IsForWebApp(browser_pair.first, app_id)) {
      before_state_app_window_count++;
    }
  }
  for (const auto& browser_pair : after_action_profile->browsers) {
    if (AppBrowserController::IsForWebApp(browser_pair.first, app_id)) {
      after_state_app_window_count++;
    }
  }
  int app_window_diff =
      after_state_app_window_count - before_state_app_window_count;
  ASSERT_EQ(NumberToInt(number), app_window_diff);
}

void WebAppIntegrationTestDriver::CheckPwaWindowCreatedInProfile(
    Site site,
    Number number,
    ProfileName profile_name) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  Profile* profile = GetOrCreateProfile(profile_name);
  CheckPwaWindowCreatedImpl(profile, site, number);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowNotCreated() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  CHECK(before_state_change_action_state_);
  std::optional<ProfileState> after_action_profile =
      GetStateForProfile(after_state_change_action_state_.get(), profile());
  std::optional<ProfileState> before_action_profile =
      GetStateForProfile(before_state_change_action_state_.get(), profile());
  ASSERT_TRUE(after_action_profile.has_value());
  ASSERT_TRUE(before_action_profile.has_value());
  EXPECT_EQ(after_action_profile->browsers.size(),
            before_action_profile->browsers.size())
      << "Before: \n"
      << *before_state_change_action_state_ << "\nAfter:\n"
      << *after_state_change_action_state_;
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowControlsOverlayToggle(
    Site site,
    IsShown is_shown) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser(),
                                                GetAppIdBySiteMode(site)));
  EXPECT_EQ(app_browser()->app_controller()->AppUsesWindowControlsOverlay(),
            is_shown == IsShown::kShown);
  WebAppFrameToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(app_browser())
          ->web_app_frame_toolbar_for_testing();
  ASSERT_EQ(toolbar->get_right_container_for_testing()
                    ->window_controls_overlay_toggle_button() &&
                toolbar->get_right_container_for_testing()
                    ->window_controls_overlay_toggle_button()
                    ->GetVisible(),
            is_shown == IsShown::kShown);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowControlsOverlayToggleIcon(
    IsShown is_shown) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  WebAppFrameToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(app_browser())
          ->web_app_frame_toolbar_for_testing();
  // If toolbar is not visible, then the WCO toggle button is also invisible.
  if (toolbar->GetVisible()) {
    ASSERT_EQ(toolbar->get_right_container_for_testing()
                      ->window_controls_overlay_toggle_button() &&
                  toolbar->get_right_container_for_testing()
                      ->window_controls_overlay_toggle_button()
                      ->GetVisible(),
              is_shown == IsShown::kShown);
  }
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowControlsOverlay(Site site,
                                                             IsOn is_on) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser(),
                                                GetAppIdBySiteMode(site)));
  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser());
  EXPECT_EQ(app_view->IsWindowControlsOverlayEnabled(), is_on == IsOn::kOn);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowDisplayMinimal() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());
  web_app::AppBrowserController* app_controller =
      app_browser()->app_controller();
  ASSERT_TRUE(app_controller->AsWebAppBrowserController());
  std::optional<AppState> app_state = GetStateForAppId(
      after_state_change_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());

  content::WebContents* web_contents =
      app_browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  DisplayMode window_display_mode =
      web_contents->GetDelegate()->GetDisplayMode(web_contents);

  EXPECT_TRUE(app_controller->HasMinimalUiButtons());
  EXPECT_FALSE(app_controller->AppUsesTabbed());

  EXPECT_EQ(app_state->effective_display_mode,
            blink::mojom::DisplayMode::kMinimalUi);
  EXPECT_EQ(window_display_mode, blink::mojom::DisplayMode::kMinimalUi);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowDisplayTabbed() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());

  web_app::AppBrowserController* app_controller =
      app_browser()->app_controller();
  ASSERT_TRUE(app_controller->AsWebAppBrowserController());
  std::optional<AppState> app_state = GetStateForAppId(
      after_state_change_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());

  content::WebContents* web_contents =
      app_browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  DisplayMode window_display_mode =
      web_contents->GetDelegate()->GetDisplayMode(web_contents);

  EXPECT_FALSE(app_controller->HasMinimalUiButtons());
  EXPECT_TRUE(app_controller->AppUsesTabbed());

  EXPECT_EQ(app_state->effective_display_mode,
            blink::mojom::DisplayMode::kTabbed);
  EXPECT_EQ(window_display_mode, blink::mojom::DisplayMode::kTabbed);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckWindowDisplayStandalone() {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }
  ASSERT_TRUE(app_browser());

  web_app::AppBrowserController* app_controller =
      app_browser()->app_controller();
  ASSERT_TRUE(app_controller->AsWebAppBrowserController());
  std::optional<AppState> app_state = GetStateForAppId(
      after_state_change_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());

  content::WebContents* web_contents =
      app_browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  DisplayMode window_display_mode =
      web_contents->GetDelegate()->GetDisplayMode(web_contents);

  EXPECT_FALSE(app_controller->HasMinimalUiButtons());
  EXPECT_FALSE(app_controller->AppUsesTabbed());

  EXPECT_EQ(app_state->effective_display_mode,
            blink::mojom::DisplayMode::kStandalone);
  EXPECT_EQ(window_display_mode, blink::mojom::DisplayMode::kStandalone);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckHasSubApp(Site parent_app,
                                                 Site sub_app) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }

  content::WebContents* web_contents =
      GetAnyWebContentsForAppId(GetAppIdBySiteMode(parent_app));
  ASSERT_TRUE(web_contents)
      << "No open tab or window for the parent app was found.";

  std::string sub_app_url = GetRelativeSubAppPath(sub_app);

  const base::Value& list_result =
      content::EvalJs(web_contents, "navigator.subApps.list()").value;

  const base::Value::Dict& list_result_dict = list_result.GetDict();

  // Check that list() contained the sub_app_url key.
  EXPECT_NE(nullptr, list_result_dict.FindDict(sub_app_url));

  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckNotHasSubApp(Site parent_app,
                                                    Site sub_app) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }

  content::WebContents* web_contents =
      GetAnyWebContentsForAppId(GetAppIdBySiteMode(parent_app));
  ASSERT_TRUE(web_contents)
      << "No open tab or window for the parent app was found.";

  std::string sub_app_url = GetRelativeSubAppPath(sub_app);

  const base::Value& list_result =
      content::EvalJs(web_contents, "navigator.subApps.list()").value;

  const base::Value::Dict& list_result_dict = list_result.GetDict();

  // Check that list() did not contain the sub_app_url key.
  EXPECT_EQ(nullptr, list_result_dict.FindDict(sub_app_url));

  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckNoSubApps(Site parent_app) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }

  content::WebContents* web_contents =
      GetAnyWebContentsForAppId(GetAppIdBySiteMode(parent_app));
  ASSERT_TRUE(web_contents)
      << "No open tab or window for the parent app was found.";

  const base::Value& result =
      content::EvalJs(web_contents, "navigator.subApps.list()").value;

  // Check that list() returned an empty dictionary.
  EXPECT_EQ(base::Value(base::Value::Type::DICT), result);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::CheckAppLoadedInTab(Site site) {
  if (!BeforeStateCheckAction(__FUNCTION__)) {
    return;
  }

  bool app_launched = false;
  auto* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    // Bypass apps that open in standalone windows.
    if (AppBrowserController::IsWebApp(browser)) {
      continue;
    }

    for (int i = 0; i < browser->tab_strip_model()->GetTabCount(); i++) {
      content::WebContents* web_contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      const webapps::AppId* app_id = WebAppTabHelper::GetAppId(web_contents);
      if (!app_id) {
        continue;
      }

      if (*app_id == GetAppIdBySiteMode(site)) {
        app_launched = true;
        break;
      }
    }
  }
  EXPECT_TRUE(app_launched);
  AfterStateCheckAction();
}

void WebAppIntegrationTestDriver::OnWebAppManifestUpdated(
    const webapps::AppId& app_id) {
  LOG(INFO) << "Manifest update received for " << app_id << ".";
  CHECK(!delegate_->IsSyncTest())
      << "Manifest update waiting only supported on non-sync tests.";

  previous_manifest_updates_.insert(app_id);
  if (waiting_for_update_id_ == app_id) {
    CHECK(waiting_for_update_run_loop_);
    waiting_for_update_run_loop_->Quit();
    waiting_for_update_id_ = std::nullopt;
    // The `BeforeState*Action()` methods check that the
    // `after_state_change_action_state_` has not changed from the current
    // state. This is great, except for the manifest update edge case, which can
    // happen asynchronously outside of actions. In this case, re-grab the
    // snapshot after the update.
    if (executing_action_level_ == 0 && after_state_change_action_state_) {
      after_state_change_action_state_ = ConstructStateSnapshot();
    }
  }
}

void WebAppIntegrationTestDriver::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  if (!waiting_for_update_id_.has_value()) {
    return;
  }

  if (waiting_for_update_id_.value() == app_id &&
      waiting_for_update_run_loop_ != nullptr) {
    waiting_for_update_run_loop_->Quit();
    waiting_for_update_id_ = std::nullopt;
    // The `BeforeState*Action()` methods check that the
    // `after_state_change_action_state_` has not changed from the current
    // state. This is great, except for the manifest update edge case, which can
    // happen asynchronously outside of actions. In this case, re-grab the
    // snapshot after the update.
    if (executing_action_level_ == 0 && after_state_change_action_state_) {
      after_state_change_action_state_ = ConstructStateSnapshot();
    }
  }
}

bool WebAppIntegrationTestDriver::BeforeStateChangeAction(
    const char* function) {
  CHECK(!base::StartsWith(function, "Check"));
  if (testing::Test::HasFatalFailure() && !in_tear_down_) {
    return false;
  }
  if (testing::Test::IsSkipped() && !in_tear_down_) {
    return false;
  }
  LOG(INFO) << "BeforeStateChangeAction: "
            << std::string(executing_action_level_, ' ') << function;
  ++executing_action_level_;
  std::unique_ptr<StateSnapshot> current_state = ConstructStateSnapshot();
  if (after_state_change_action_state_) {
    CHECK_EQ(*after_state_change_action_state_, *current_state)
        << "State cannot be changed outside of state change actions.";
    before_state_change_action_state_ =
        std::move(after_state_change_action_state_);
  } else {
    before_state_change_action_state_ = std::move(current_state);
  }
  return true;
}

void WebAppIntegrationTestDriver::AfterStateChangeAction() {
  CHECK(executing_action_level_ > 0);
  --executing_action_level_;
#if BUILDFLAG(IS_MAC)
  std::map<webapps::AppId, size_t> open_browsers_per_app;
  for (auto* profile : GetAllProfiles()) {
    auto* provider = GetProviderForProfile(profile);
    if (!provider) {
      continue;
    }
    std::vector<webapps::AppId> app_ids =
        provider->registrar_unsafe().GetAppIds();
    for (auto& app_id : app_ids) {
      // Wait for any shims to finish connecting.
      auto* app_shim_manager = apps::AppShimManager::Get();
      AppShimHost* app_shim_host = app_shim_manager->FindHost(profile, app_id);
      if (app_shim_host && !app_shim_host->HasBootstrapConnected()) {
        base::RunLoop loop;
        app_shim_host->SetOnShimConnectedForTesting(loop.QuitClosure());
        loop.Run();
      }

      // But also wait for any shims for apps that don't have any browsers open
      // anymore to finish quitting, as otherwise attempting to launch them
      // again too soon could fail.
      open_browsers_per_app[app_id] +=
          provider->ui_manager().GetNumWindowsForApp(app_id);
    }
  }
  for (const auto& [app_id, open_browsers] : open_browsers_per_app) {
    if (open_browsers != 0) {
      continue;
    }
    std::string app_name =
        provider()->registrar_unsafe().GetAppShortName(app_id);
    base::FilePath app_path = GetShortcutPath(
        override_registration_->test_override().chrome_apps_folder(), app_name,
        app_id);
    WaitForShimToQuitForTesting(app_path, app_id);
  }
#endif
  if (delegate_->IsSyncTest()) {
    delegate_->AwaitWebAppQuiescence();
  }
  FlushShortcutTasks();
  provider()->command_manager().AwaitAllCommandsCompleteForTesting();
  AwaitManifestSystemIdle();

  auto get_first_loading_web_contents = []() -> content::WebContents* {
    for (Browser* browser : *BrowserList::GetInstance()) {
      for (int i = 0; i < browser->tab_strip_model()->GetTabCount(); i++) {
        content::WebContents* web_contents =
            browser->tab_strip_model()->GetWebContentsAt(i);
        if (!web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
          return web_contents;
        }
      }
    }
    return nullptr;
  };

  // In some circumstances, this loop could hang forever (if pages never
  // complete loading, or if they cause reloads, etc). However, these
  // tests only use static test pages that don't do that, so this should
  // be safe.
  while (content::WebContents* loading_web_content =
             get_first_loading_web_contents()) {
    PageLoadWaiter page_load_waiter(loading_web_content);
    page_load_waiter.Wait();
  }
  after_state_change_action_state_ = ConstructStateSnapshot();
}

bool WebAppIntegrationTestDriver::BeforeStateCheckAction(const char* function) {
  CHECK(strstr(function, "Check") != nullptr) << function;
  if (testing::Test::HasFatalFailure() && !in_tear_down_) {
    return false;
  }
  if (testing::Test::IsSkipped() && !in_tear_down_) {
    return false;
  }
  ++executing_action_level_;
  provider()->command_manager().AwaitAllCommandsCompleteForTesting();
  LOG(INFO) << "BeforeStateCheckAction: "
            << std::string(executing_action_level_, ' ') << function;
  CHECK(after_state_change_action_state_);
  return true;
}

void WebAppIntegrationTestDriver::AfterStateCheckAction() {
  CHECK(executing_action_level_ > 0);
  --executing_action_level_;
  if (!after_state_change_action_state_) {
    return;
  }
  ASSERT_EQ(*after_state_change_action_state_, *ConstructStateSnapshot());
}

void WebAppIntegrationTestDriver::AwaitManifestSystemIdle() {
  if (!is_performing_manifest_update_) {
    return;
  }

  // Wait till pending manifest update processes have finished loading the page
  // to start the manifest update.
  ManifestUpdateManager& manifest_update_manager =
      provider()->manifest_update_manager();
  WebAppCommandManager& command_manager = provider()->command_manager();
  // TODO(crbug.com/40873503): Figure out a better way of streamlining
  //  the waiting instead of doing it separately for manifest updates
  //  and commands. This fails WebAppIntegrationTestDriver::CloseCustomToolbar()
  //  because DidFinishLoad() is not triggered for a backwards navigation, thus
  //  a manifest update is triggered but is stuck.
  while (manifest_update_manager.HasUpdatesPendingLoadFinishForTesting()) {
    base::RunLoop loop_for_load_finish;
    manifest_update_manager.SetLoadFinishedCallbackForTesting(
        loop_for_load_finish.QuitClosure());
    loop_for_load_finish.Run();
  }
  // Wait till all manifest update data fetch commands have completed.
  command_manager.AwaitAllCommandsCompleteForTesting();

  // Wait till all manifest update finalize commands have completed (if any).
  command_manager.AwaitAllCommandsCompleteForTesting();
}

webapps::AppId GetAppIdForIsolatedSite(Site site) {
  auto parent_site = GetSiteConfiguration(site).parent_site;
  web_package::test::Ed25519KeyPair key_pair =
      GetKeyPairForSite(parent_site ? parent_site.value() : site);

  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      web_package::SignedWebBundleId::CreateForPublicKey(key_pair.public_key));

  if (parent_site) {
    // The scope and manifest ID of an Isolated Web App are always the unique
    // isolated-app:// origin based on the signing key of the app.
    GURL parent_app_origin = url_info.origin().GetURL();
    GURL start_url = parent_app_origin.Resolve(GetRelativeSubAppPath(site));
    return GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url,
                         /*parent_manifest_id=*/parent_app_origin);
  }

  return url_info.app_id();
}

webapps::AppId WebAppIntegrationTestDriver::GetAppIdBySiteMode(Site site) {
  auto site_config = GetSiteConfiguration(site);

  if (site_config.is_isolated) {
    return GetAppIdForIsolatedSite(site);
  }

  std::string manifest_id = site_config.relative_manifest_id;
  GURL start_url = GetUrlForSite(site);
  CHECK(start_url.is_valid());

  if (site_config.parent_site) {
    auto parent_site = GetSiteConfiguration(site_config.parent_site.value());
    return GenerateAppId(
        manifest_id, start_url,
        GetTestServerForSiteMode(site).GetURL(parent_site.relative_url));
  } else {
    return GenerateAppId(manifest_id, start_url);
  }
}

GURL WebAppIntegrationTestDriver::GetUrlForSite(Site site,
                                                const std::string& suffix) {
  auto site_config = GetSiteConfiguration(site);
  if (site_config.base_url.empty()) {
    return GetTestServerForSiteMode(site).GetURL(
        base::StrCat({site_config.relative_url, suffix}));
  }
  return GURL(
      base::StrCat({site_config.base_url, site_config.relative_url, suffix}));
}

std::optional<AppState> WebAppIntegrationTestDriver::GetAppBySiteMode(
    StateSnapshot* state_snapshot,
    Profile* profile,
    Site site) {
  std::optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return std::nullopt;
  }

  webapps::AppId app_id = GetAppIdBySiteMode(site);
  auto it = profile_state->apps.find(app_id);
  return it == profile_state->apps.end()
             ? std::nullopt
             : std::make_optional<AppState>(it->second);
}

WebAppProvider* WebAppIntegrationTestDriver::GetProviderForProfile(
    Profile* profile) {
  return WebAppProvider::GetForTest(profile);
}

std::unique_ptr<StateSnapshot>
WebAppIntegrationTestDriver::ConstructStateSnapshot() {
  base::flat_map<Profile*, ProfileState> profile_state_map;
  for (Profile* profile : GetAllProfiles()) {
    base::flat_map<Browser*, BrowserState> browser_state;
    auto* browser_list = BrowserList::GetInstance();
    for (Browser* browser : *browser_list) {
      if (browser->profile() != profile) {
        continue;
      }

      TabStripModel* tabs = browser->tab_strip_model();
      base::flat_map<content::WebContents*, TabState> tab_state_map;
      for (int i = 0; i < tabs->count(); ++i) {
        content::WebContents* tab = tabs->GetWebContentsAt(i);
        CHECK(tab);
        GURL url = tab->GetURL();
        tab_state_map.emplace(tab, TabState(url));
      }
      content::WebContents* active_tab_contents = tabs->GetActiveWebContents();
      bool launch_icon_shown = false;
      bool is_app_browser = AppBrowserController::IsWebApp(browser);
      if (!is_app_browser && active_tab_contents != nullptr) {
        AwaitIntentPickerTabHelperIconUpdateComplete(active_tab_contents);
        launch_icon_shown = intent_chip_view()->GetVisible();
      }
      webapps::AppId app_id;
      if (AppBrowserController::IsWebApp(browser)) {
        app_id = browser->app_controller()->app_id();
      }

      browser_state.emplace(
          browser, BrowserState(browser, tab_state_map, active_tab_contents,
                                app_id, launch_icon_shown));
    }

    WebAppProvider* provider = GetProviderForProfile(profile);
    base::flat_map<webapps::AppId, AppState> app_state;
    if (provider) {
      WebAppRegistrar& registrar = provider->registrar_unsafe();
      auto app_ids = registrar.GetAppIds();
      for (const auto& app_id : app_ids) {
        std::string manifest_launcher_icon_filename;
        std::vector<apps::IconInfo> icon_infos =
            registrar.GetAppIconInfos(app_id);
        for (const auto& info : icon_infos) {
          int icon_size = info.square_size_px.value_or(-1);
          if (icon_size == kLauncherIconSize) {
            manifest_launcher_icon_filename = info.url.ExtractFileName();
          }
        }
        auto state = AppState(
            app_id, registrar.GetAppShortName(app_id),
            registrar.GetAppScope(app_id),
            ConvertOsLoginMode(registrar.GetAppRunOnOsLoginMode(app_id).value),
            registrar.GetAppEffectiveDisplayMode(app_id),
            registrar.GetAppUserDisplayMode(app_id),
            manifest_launcher_icon_filename,
            registrar.IsInstallState(
                app_id, {web_app::proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                         web_app::proto::INSTALLED_WITH_OS_INTEGRATION}),
            IsShortcutAndIconCreated(profile, registrar.GetAppShortName(app_id),
                                     app_id));
#if !BUILDFLAG(IS_CHROMEOS)
        if (registrar.IsInstallState(
                app_id, {web_app::proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                         web_app::proto::INSTALLED_WITH_OS_INTEGRATION})) {
          CheckAppSettingsAppState(profile->GetOriginalProfile(), state);
        }
#endif
        app_state.emplace(app_id, state);
      }
    }

    profile_state_map.emplace(
        profile, ProfileState(std::move(browser_state), std::move(app_state)));
  }
  return std::make_unique<StateSnapshot>(std::move(profile_state_map));
}

Profile* WebAppIntegrationTestDriver::GetOrCreateProfile(
    ProfileName profile_name) {
  const char* profile_name_str = nullptr;
  switch (profile_name) {
    case ProfileName::kDefault:
      profile_name_str = "Default";
      break;
    case ProfileName::kProfile2:
      profile_name_str = "Profile2";
      break;
  }
  CHECK(profile_name_str);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath profile_path = user_data_dir.AppendASCII(profile_name_str);
  return g_browser_process->profile_manager()->GetProfile(profile_path);
}

content::WebContents* WebAppIntegrationTestDriver::GetCurrentTab(
    Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

GURL WebAppIntegrationTestDriver::GetInScopeURL(Site site) {
  return GetUrlForSite(site);
}

base::FilePath WebAppIntegrationTestDriver::GetShortcutPath(
    base::FilePath shortcut_dir,
    const std::string& app_name,
    const webapps::AppId& app_id) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  return override_registration_->test_override().GetShortcutPath(
      profile(), shortcut_dir, app_id, app_name);
#else
  return base::FilePath();
#endif
}

void WebAppIntegrationTestDriver::InstallPolicyAppInternal(
    Site site,
    base::Value default_launch_container,
    const bool create_shortcut,
    const bool install_as_shortcut) {
  // Many CUJs rely on operating on an opened window / tab after installation,
  // and this state is true for all installations except for policy install. To
  // help keep CUJs combined for all installs, do a navigation here.
  MaybeNavigateTabbedBrowserInScope(site);
  GURL url = GetUrlForSite(site);
  WebAppTestInstallWithOsHooksObserver observer(profile());
  observer.BeginListening();
  {
    base::Value::Dict item;
    item.Set(kUrlKey, url.spec());
    item.Set(kDefaultLaunchContainerKey, std::move(default_launch_container));
    item.Set(kCreateDesktopShortcutKey, create_shortcut);
    item.Set(kInstallAsShortcut, install_as_shortcut);
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kWebAppInstallForceList);
    update->Append(std::move(item));
  }
  active_app_id_ = observer.Wait();
  apps::AppReadinessWaiter(profile(), active_app_id_).Await();
}

void WebAppIntegrationTestDriver::ApplyRunOnOsLoginPolicy(Site site,
                                                          const char* policy) {
  base::RunLoop run_loop;
  WebAppProvider::GetForTest(profile())
      ->policy_manager()
      .SetRefreshPolicySettingsCompletedCallbackForTesting(
          run_loop.QuitClosure());
  GURL url = GetUrlForSite(site);
  {
    ScopedListPrefUpdate update(profile()->GetPrefs(), prefs::kWebAppSettings);
    base::Value::List& update_list = update.Get();
    update_list.EraseIf([&](const base::Value& item) {
      return *item.GetDict().FindString(kManifestId) == url.spec();
    });

    base::Value::Dict dict_item;
    dict_item.Set(kManifestId, url.spec());
    dict_item.Set(kRunOnOsLogin, policy);

    update_list.Append(std::move(dict_item));
  }
  run_loop.Run();
}

void WebAppIntegrationTestDriver::UninstallPolicyAppById(
    Profile* profile,
    const webapps::AppId& id) {
  base::RunLoop run_loop;
  apps::AppReadinessWaiter app_registration_waiter(
      profile, id, apps::Readiness::kUninstalledByNonUser);
  WebAppInstallManagerObserverAdapter observer(profile);
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& app_id) {
        if (id == app_id) {
          run_loop.Quit();
        }
      }));
  // If there are still install sources, the app might not be fully uninstalled,
  // so this will listen for the removal of the policy install source.
  observer.SetWebAppSourceRemovedDelegate(
      base::BindLambdaForTesting([&](const webapps::AppId& app_id) {
        if (id == app_id) {
          run_loop.Quit();
        }
      }));

  WebAppProvider* provider = WebAppProvider::GetForTest(profile);
  const WebApp* web_app = provider->registrar_unsafe().GetAppById(id);
  ASSERT_TRUE(web_app);

  base::flat_set<GURL> install_urls;
  {
    auto policy_config_it = web_app->management_to_external_config_map().find(
        WebAppManagement::kPolicy);
    ASSERT_NE(policy_config_it,
              web_app->management_to_external_config_map().end());
    ASSERT_FALSE(policy_config_it->second.install_urls.empty());
    install_urls = policy_config_it->second.install_urls;
  }

  {
    ScopedListPrefUpdate update(profile->GetPrefs(),
                                prefs::kWebAppInstallForceList);
    size_t removed_count = update->EraseIf([&](const base::Value& item) {
      const base::Value* url_value = item.GetDict().Find(kUrlKey);
      return url_value && install_urls.contains(GURL(url_value->GetString()));
    });
    ASSERT_GT(removed_count, 0U);
  }
  run_loop.Run();
  const WebApp* app = provider->registrar_unsafe().GetAppById(id);
  // If the app was fully uninstalled, wait for the change to propagate through
  // App Service.
  if (app == nullptr) {
    app_registration_waiter.Await();

    // Ensure the completion of any additional sub app uninstalls that were
    // triggered.
    provider->command_manager().AwaitAllCommandsCompleteForTesting();
  }
  if (app == nullptr && active_app_id_ == id) {
    active_app_id_.clear();
  }
}

void WebAppIntegrationTestDriver::ForceUpdateManifestContents(
    Site site,
    const GURL& app_url_with_manifest_param) {
  auto app_id = GetAppIdBySiteMode(site);
  active_app_id_ = app_id;
  // Manifest updates must occur as the first navigation after a webapp is
  // installed, otherwise the throttle is tripped.
  ASSERT_FALSE(provider()->manifest_update_manager().IsUpdateConsumed(
      app_id, base::Time::Now()));
  ASSERT_FALSE(
      provider()->manifest_update_manager().IsUpdateCommandPending(app_id));
  NavigateTabbedBrowserToSite(app_url_with_manifest_param,
                              NavigationMode::kCurrentTab);
  is_performing_manifest_update_ = true;
}

void WebAppIntegrationTestDriver::MaybeNavigateTabbedBrowserInScope(Site site) {
  auto browser_url = GetCurrentTab(browser())->GetURL();
  auto dest_url = GetInScopeURL(site);
  if (browser_url.is_empty() || browser_url != dest_url) {
    NavigateTabbedBrowserToSite(dest_url, NavigationMode::kCurrentTab);
  }
}

void WebAppIntegrationTestDriver::NavigateTabbedBrowserToSite(
    const GURL& url,
    NavigationMode mode) {
  ASSERT_TRUE(browser());
  content::TestNavigationObserver url_observer(url);
  if (mode == NavigationMode::kNewTab) {
    url_observer.StartWatchingNewWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  } else {
    url_observer.WatchExistingWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }
  url_observer.Wait();
}

Browser* WebAppIntegrationTestDriver::GetAppBrowserForSite(
    Site site,
    bool launch_if_not_open) {
  StateSnapshot* state = after_state_change_action_state_
                             ? after_state_change_action_state_.get()
                             : before_state_change_action_state_.get();
  CHECK(state);
  std::optional<AppState> app_state = GetAppBySiteMode(state, profile(), site);
  EXPECT_TRUE(app_state) << "Could not find installed app for site "
                         << static_cast<int>(site);
  if (!app_state) {
    return nullptr;
  }

  auto profile_state = GetStateForProfile(state, profile());
  EXPECT_TRUE(profile_state);
  if (!profile_state) {
    return nullptr;
  }
  for (const auto& browser_state_pair : profile_state->browsers) {
    if (browser_state_pair.second.app_id == app_state->id) {
      return browser_state_pair.second.browser;
    }
  }
  if (!launch_if_not_open) {
    return nullptr;
  }
  Browser* browser = LaunchWebAppBrowserAndWait(profile(), app_state->id);
  provider()->manifest_update_manager().ResetManifestThrottleForTesting(
      GetAppIdBySiteMode(site));
  return browser;
}

bool WebAppIntegrationTestDriver::IsShortcutAndIconCreated(
    Profile* profile,
    const std::string& name,
    const webapps::AppId& id) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  bool is_shortcut_and_icon_correct = false;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  bool is_shortcut_correct =
      override_registration_->test_override().IsShortcutCreated(profile, id,
                                                                name);
  is_shortcut_and_icon_correct =
      is_shortcut_correct && DoIconColorsMatch(profile, name, id);
#elif BUILDFLAG(IS_CHROMEOS)
  is_shortcut_and_icon_correct = DoIconColorsMatch(profile, name, id);
#endif
  return is_shortcut_and_icon_correct;
}

bool WebAppIntegrationTestDriver::DoIconColorsMatch(Profile* profile,
                                                    const std::string& name,
                                                    const webapps::AppId& id) {
  bool do_icon_colors_match = false;
#if BUILDFLAG(IS_WIN)
  SkColor expected_icon_pixel_color =
      GetSiteConfigurationFromAppName(name).icon_color;
  std::optional<SkColor> shortcut_pixel_color_desktop =
      override_registration_->test_override().GetShortcutIconTopLeftColor(
          profile, override_registration_->test_override().desktop(), id, name);
  std::optional<SkColor> shortcut_pixel_color_application_menu =
      override_registration_->test_override().GetShortcutIconTopLeftColor(
          profile, override_registration_->test_override().application_menu(),
          id, name);
  if (shortcut_pixel_color_desktop.has_value() &&
      shortcut_pixel_color_application_menu.has_value()) {
    do_icon_colors_match =
        (expected_icon_pixel_color == shortcut_pixel_color_desktop.value() &&
         expected_icon_pixel_color ==
             shortcut_pixel_color_application_menu.value());
  }
#elif BUILDFLAG(IS_MAC)
  SkColor expected_icon_pixel_color =
      GetSiteConfigurationFromAppName(name).icon_color;
  std::optional<SkColor> shortcut_pixel_color_apps_folder =
      override_registration_->test_override().GetShortcutIconTopLeftColor(
          profile, override_registration_->test_override().chrome_apps_folder(),
          id, name);
  if (shortcut_pixel_color_apps_folder.has_value()) {
    do_icon_colors_match =
        (expected_icon_pixel_color == shortcut_pixel_color_apps_folder.value());
  }
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  SkColor expected_icon_pixel_color =
      GetSiteConfigurationFromAppName(name).icon_color;
  std::optional<SkColor> actual_color_install_icon_size =
      override_registration_->test_override().GetShortcutIconTopLeftColor(
          profile, base::FilePath(), id, name, kInstallIconSize);

  std::optional<SkColor> actual_color_launcher_icon_size =
      override_registration_->test_override().GetShortcutIconTopLeftColor(
          profile, base::FilePath(), id, name, kLauncherIconSize);
  if (actual_color_install_icon_size.has_value() &&
      actual_color_launcher_icon_size.has_value()) {
    do_icon_colors_match =
        (expected_icon_pixel_color == actual_color_install_icon_size.value() &&
         expected_icon_pixel_color == actual_color_launcher_icon_size.value());
  }
#endif
  return do_icon_colors_match;
}

void WebAppIntegrationTestDriver::SetFileHandlingEnabled(Site site,
                                                         bool enabled) {
#if !BUILDFLAG(IS_CHROMEOS)
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  ASSERT_TRUE(provider()->registrar_unsafe().GetAppById(app_id))
      << "No app installed for site: " << static_cast<int>(site);
  auto app_management_page_handler = CreateAppManagementPageHandler(profile());
  app_management_page_handler.SetFileHandlingEnabled(app_id, enabled);
#endif
}

void WebAppIntegrationTestDriver::LaunchFile(Site site,
                                             FilesOptions files_options) {
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  std::vector<base::FilePath> file_paths = GetTestFilePaths(files_options);
#if BUILDFLAG(IS_MAC)
  std::vector<GURL> urls;
  urls.reserve(file_paths.size());
  for (const base::FilePath& path : file_paths) {
    urls.push_back(net::FilePathToFileURL(path));
  }

  LaunchFromAppShim(site, urls, /*wait_for_complete_launch=*/false);
#else
  StartupBrowserCreator browser_creator;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, app_id);
  command_line.AppendSwitchASCII(switches::kTestType, "browser");
  for (auto file_path : file_paths) {
    command_line.AppendArgPath(file_path);
  }
  browser_creator.Start(command_line, profile()->GetPath(),
                        {profile(), StartupProfileMode::kBrowserWindow}, {});
  content::RunAllTasksUntilIdle();
#endif
}

void WebAppIntegrationTestDriver::LaunchAppStartupBrowserCreator(
    const webapps::AppId& app_id) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, app_id);
  command_line.AppendSwitchASCII(switches::kTestType, "browser");
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));
  content::RunAllTasksUntilIdle();
}

#if BUILDFLAG(IS_MAC)
class AppShimLaunchWaiter : public apps::AppShimManager::AppShimObserver {
 public:
  explicit AppShimLaunchWaiter(bool wait_for_complete_launch)
      : wait_for_complete_launch_(wait_for_complete_launch) {}

  base::WeakPtr<AppShimLaunchWaiter> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  bool did_launch() const { return did_launch_; }

  void OnLaunchStarted(base::Process process) {
    if (!process.IsValid()) {
      LOG(INFO) << "App Shim quit before finishing launch";
      loop_.Quit();
      return;
    }
    expected_pid_ = process.Pid();
    LOG(INFO) << "Waiting for App Shim with PID " << *expected_pid_;
    MaybeQuitLoop();
  }

  void Wait() { loop_.Run(); }

  void OnShimTerminated() {
    LOG(INFO) << "App Shim terminated while launching";
    loop_.Quit();
  }

  void OnShimProcessConnected(base::ProcessId pid) override {
    LOG(INFO) << "Got App Shim connection from " << pid;
    if (!wait_for_complete_launch_) {
      launched_shims_.insert(pid);
      did_launch_ = true;
      MaybeQuitLoop();
    }
  }

  void OnShimProcessConnectedAndAllLaunchesDone(
      base::ProcessId pid,
      chrome::mojom::AppShimLaunchResult result) override {
    LOG(INFO) << "Finished launching App Shim " << pid << " with result "
              << result;
    did_launch_ = true;
    launched_shims_.insert(pid);
    MaybeQuitLoop();
  }

  void OnShimReopen(base::ProcessId pid) override {
    LOG(INFO) << "Reopened App Shim " << pid;
    did_launch_ = true;
    launched_shims_.insert(pid);
    MaybeQuitLoop();
  }

  void OnShimOpenedURLs(base::ProcessId pid) override {
    LOG(INFO) << "App Shim opened URLs " << pid;
    did_launch_ = true;
    launched_shims_.insert(pid);
    MaybeQuitLoop();
  }

 private:
  void MaybeQuitLoop() {
    if (expected_pid_.has_value() &&
        launched_shims_.find(*expected_pid_) != launched_shims_.end()) {
      loop_.Quit();
    }
  }

  bool wait_for_complete_launch_ = true;

  base::RunLoop loop_;
  std::set<base::ProcessId> launched_shims_;
  std::optional<base::ProcessId> expected_pid_;
  bool did_launch_ = false;
  base::WeakPtrFactory<AppShimLaunchWaiter> weak_factory_{this};
};

bool WebAppIntegrationTestDriver::LaunchFromAppShim(
    Site site,
    const std::vector<GURL>& urls,
    bool wait_for_complete_launch) {
  webapps::AppId app_id = GetAppIdBySiteMode(site);
  std::string app_name = GetSiteConfiguration(site).app_name;
  base::FilePath app_path = GetShortcutPath(
      override_registration_->test_override().chrome_apps_folder(), app_name,
      app_id);

  base::FilePath chrome_path = ::test::GuessAppBundlePath();
  chrome_path =
      chrome_path.Append("Contents")
          .Append("MacOS")
          .Append(chrome_path.BaseName().RemoveFinalExtension().value());

  AppShimLaunchWaiter launch_waiter(wait_for_complete_launch);
  apps::AppShimManager::Get()->SetAppShimObserverForTesting(&launch_waiter);
  LaunchShimForTesting(app_path, urls,
                       base::BindOnce(&AppShimLaunchWaiter::OnLaunchStarted,
                                      launch_waiter.AsWeakPtr()),
                       base::BindOnce(&AppShimLaunchWaiter::OnShimTerminated,
                                      launch_waiter.AsWeakPtr()),
                       chrome_path);
  launch_waiter.Wait();

  apps::AppShimManager::Get()->SetAppShimObserverForTesting(nullptr);

  return launch_waiter.did_launch();
}
#endif

Browser* WebAppIntegrationTestDriver::browser() {
  Browser* browser =
      chrome::FindTabbedBrowser(profile(), /*match_original_profiles=*/false);
  CHECK(browser);
  if (!browser->tab_strip_model()->count()) {
    delegate_->AddBlankTabAndShow(browser);
  }
  return browser;
}

Profile* WebAppIntegrationTestDriver::profile() {
  if (!active_profile_) {
    active_profile_ = delegate_->GetDefaultProfile();
  }
  return active_profile_;
}

std::vector<Profile*> WebAppIntegrationTestDriver::GetAllProfiles() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  size_t profile_count = profiles.size();
  for (size_t i = 0; i < profile_count; ++i) {
    std::vector<Profile*> otr_profiles =
        profiles[i]->GetAllOffTheRecordProfiles();
    profiles.insert(profiles.end(), otr_profiles.begin(), otr_profiles.end());
  }
  return profiles;
}

PageActionIconView* WebAppIntegrationTestDriver::pwa_install_view() {
  PageActionIconView* pwa_install_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kPwaInstall);
  CHECK(pwa_install_view);
  return pwa_install_view;
}

IntentChipButton* WebAppIntegrationTestDriver::intent_chip_view() {
  IntentChipButton* intent_chip_button = GetIntentPickerIcon(browser());
  CHECK(intent_chip_button);
  return intent_chip_button;
}

const net::EmbeddedTestServer&
WebAppIntegrationTestDriver::GetTestServerForSiteMode(Site site) const {
  return *delegate_->EmbeddedTestServer();
}

#if !BUILDFLAG(IS_CHROMEOS)
webapps::AppHomePageHandler
WebAppIntegrationTestDriver::GetTestAppHomePageHandler(
    content::TestWebUI* web_ui) {
  mojo::PendingReceiver<app_home::mojom::Page> page;
  mojo::Remote<app_home::mojom::PageHandler> page_handler;
  return webapps::AppHomePageHandler(web_ui, profile(),
                                     page_handler.BindNewPipeAndPassReceiver(),
                                     page.InitWithNewPipeAndPassRemote());
}
#endif

WebAppIntegrationTest::WebAppIntegrationTest() : helper_(this) {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  enabled_features.push_back(blink::features::kDesktopPWAsSubApps);
  enabled_features.push_back(blink::features::kDesktopPWAsTabStrip);
  enabled_features.push_back(features::kDesktopPWAsTabStripSettings);
  enabled_features.push_back(features::kIsolatedWebAppDevMode);
  enabled_features.push_back(features::kIsolatedWebApps);
  enabled_features.push_back(features::kPwaUpdateDialogForIcon);
  enabled_features.push_back(features::kRecordWebAppDebugInfo);
  enabled_features.push_back(features::kWebAppDontAddExistingAppsToSync);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // WebAppIntegrationTest runs in Ash only when Lacros is disabled.
  // If Lacros is enabled, WebAppIntegrationTest runs in Lacros with crosapi
  // enabled.
  base::Extend(disabled_features, ash::standalone_browser::GetFeatureRefs());
#endif
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40236806): Update test driver to work with new UI.
  enabled_features.push_back(apps::features::kLinkCapturingUiUpdate);
#else
  // TODO(b/313492499): Update test driver to work with new intent picker UI.
  enabled_features.push_back(features::kPwaNavigationCapturing);
#endif  // BUILDFLAG(IS_CHROMEOS)
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

WebAppIntegrationTest::~WebAppIntegrationTest() = default;

void WebAppIntegrationTest::SetUp() {
  helper_.SetUp();
  InProcessBrowserTest::SetUp();
}

void WebAppIntegrationTest::SetUpOnMainThread() {
  helper_.SetUpOnMainThread();
}
void WebAppIntegrationTest::TearDownOnMainThread() {
  helper_.TearDownOnMainThread();
}

void WebAppIntegrationTest::SetUpCommandLine(base::CommandLine* command_line) {
  ASSERT_TRUE(embedded_test_server()->Start());
}

Browser* WebAppIntegrationTest::CreateBrowser(Profile* profile) {
  return InProcessBrowserTest::CreateBrowser(profile);
}

void WebAppIntegrationTest::CloseBrowserSynchronously(Browser* browser) {
  InProcessBrowserTest::CloseBrowserSynchronously(browser);
}

void WebAppIntegrationTest::AddBlankTabAndShow(Browser* browser) {
  InProcessBrowserTest::AddBlankTabAndShow(browser);
}

const net::EmbeddedTestServer* WebAppIntegrationTest::EmbeddedTestServer()
    const {
  return embedded_test_server();
}

Profile* WebAppIntegrationTest::GetDefaultProfile() {
  return browser()->profile();
}

bool WebAppIntegrationTest::IsSyncTest() {
  return false;
}

void WebAppIntegrationTest::SyncTurnOff() {
  NOTREACHED();
}
void WebAppIntegrationTest::SyncTurnOn() {
  NOTREACHED();
}
void WebAppIntegrationTest::SyncSignOut(Profile*) {
  NOTREACHED();
}
void WebAppIntegrationTest::SyncSignIn(Profile*) {
  NOTREACHED();
}
void WebAppIntegrationTest::AwaitWebAppQuiescence() {
  NOTREACHED();
}
Profile* WebAppIntegrationTest::GetProfileClient(ProfileClient client) {
  NOTREACHED();
}

}  // namespace web_app::integration_tests
