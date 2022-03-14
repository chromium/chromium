// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test_utils.h"

#include <random>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "url/gurl.h"

namespace {

std::vector<std::string> features = {
    "default_on_feature", "default_self_feature", "default_disabled_feature"};

}  // namespace

namespace web_app {
namespace test {

namespace {

class RandomHelper {
 public:
  explicit RandomHelper(const uint32_t seed)
      :  // Seed of 0 and 1 generate the same sequence, so skip 0.
        generator_(seed + 1),
        distribution_(0u, UINT32_MAX) {}

  uint32_t next_uint() { return distribution_(generator_); }

  // Return an unsigned int between 0 (inclusive) and bound (exclusive).
  uint32_t next_uint(uint32_t bound) { return next_uint() % bound; }

  bool next_bool() { return next_uint() & 1u; }

  template <typename T>
  T next_enum() {
    constexpr uint32_t min = static_cast<uint32_t>(T::kMinValue);
    constexpr uint32_t max = static_cast<uint32_t>(T::kMaxValue);
    static_assert(min <= max, "min cannot be greater than max");
    return static_cast<T>(min + next_uint(max - min));
  }

 private:
  std::default_random_engine generator_;
  std::uniform_int_distribution<uint32_t> distribution_;
};

apps::FileHandlers CreateRandomFileHandlers(uint32_t suffix) {
  apps::FileHandlers file_handlers;

  for (unsigned int i = 0; i < 5; ++i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);

    apps::FileHandler::AcceptEntry accept_entry1;
    accept_entry1.mime_type = "application/" + suffix_str + "+foo";
    accept_entry1.file_extensions.insert("." + suffix_str + "a");
    accept_entry1.file_extensions.insert("." + suffix_str + "b");

    apps::FileHandler::AcceptEntry accept_entry2;
    accept_entry2.mime_type = "application/" + suffix_str + "+bar";
    accept_entry2.file_extensions.insert("." + suffix_str + "a");
    accept_entry2.file_extensions.insert("." + suffix_str + "b");

    apps::FileHandler file_handler;
    file_handler.action = GURL("https://example.com/open-" + suffix_str);
    file_handler.accept.push_back(std::move(accept_entry1));
    file_handler.accept.push_back(std::move(accept_entry2));
    file_handler.downloaded_icons.emplace_back(
        GURL("https://example.com/image.png"), 16);
    file_handler.downloaded_icons.emplace_back(
        GURL("https://example.com/image2.png"), 48);
    file_handler.display_name = base::ASCIIToUTF16(suffix_str) + u" file";

    file_handlers.push_back(std::move(file_handler));
  }

  return file_handlers;
}

apps::ShareTarget CreateRandomShareTarget(uint32_t suffix) {
  apps::ShareTarget share_target;
  share_target.action =
      GURL("https://example.com/path/target/" + base::NumberToString(suffix));
  share_target.method = (suffix % 2 == 0) ? apps::ShareTarget::Method::kPost
                                          : apps::ShareTarget::Method::kGet;
  share_target.enctype = (suffix / 2 % 2 == 0)
                             ? apps::ShareTarget::Enctype::kMultipartFormData
                             : apps::ShareTarget::Enctype::kFormUrlEncoded;

  if (suffix % 3 != 0)
    share_target.params.title = "title" + base::NumberToString(suffix);
  if (suffix % 3 != 1)
    share_target.params.text = "text" + base::NumberToString(suffix);
  if (suffix % 3 != 2)
    share_target.params.url = "url" + base::NumberToString(suffix);

  for (uint32_t index = 0; index < suffix % 5; ++index) {
    apps::ShareTarget::Files files;
    files.name = "files" + base::NumberToString(index);
    files.accept.push_back(".extension" + base::NumberToString(index));
    files.accept.push_back("type/subtype" + base::NumberToString(index));
    share_target.params.files.push_back(files);
  }

  return share_target;
}

blink::ParsedPermissionsPolicy CreateRandomPermissionsPolicy(
    RandomHelper& random) {
  const int num_permissions_policy_declarations =
      random.next_uint(features.size());

  std::vector<std::string> available_features = features;

  const auto suffix = random.next_uint();
  std::default_random_engine rng;
  std::shuffle(available_features.begin(), available_features.end(), rng);

  blink::ParsedPermissionsPolicy permissions_policy(
      num_permissions_policy_declarations);
  const auto& feature_name_map = blink::GetPermissionsPolicyNameToFeatureMap();
  for (int i = 0; i < num_permissions_policy_declarations; ++i) {
    permissions_policy[i].feature = feature_name_map.begin()->second;
    for (unsigned int j = 0; j < 5; ++j) {
      std::string suffix_str =
          base::NumberToString(suffix) + base::NumberToString(j);

      const auto origin =
          url::Origin::Create(GURL("https://app-" + suffix_str + ".com/"));
      permissions_policy[i].allowed_origins.push_back(origin);
    }
  }
  return permissions_policy;
}

std::vector<apps::ProtocolHandlerInfo> CreateRandomProtocolHandlers(
    uint32_t suffix) {
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers;

  for (unsigned int i = 0; i < 5; ++i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);

    apps::ProtocolHandlerInfo protocol_handler;
    protocol_handler.protocol = "web+test" + suffix_str;
    protocol_handler.url = GURL("https://example.com/").Resolve(suffix_str);

    protocol_handlers.push_back(std::move(protocol_handler));
  }

  return protocol_handlers;
}

std::vector<apps::UrlHandlerInfo> CreateRandomUrlHandlers(uint32_t suffix) {
  std::vector<apps::UrlHandlerInfo> url_handlers;

  for (unsigned int i = 0; i < 3; ++i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);

    apps::UrlHandlerInfo url_handler;
    url_handler.origin =
        url::Origin::Create(GURL("https://app-" + suffix_str + ".com/"));
    url_handler.has_origin_wildcard = true;
    url_handlers.push_back(std::move(url_handler));
  }

  return url_handlers;
}

std::vector<WebAppShortcutsMenuItemInfo> CreateRandomShortcutsMenuItemInfos(
    const GURL& scope,
    RandomHelper& random) {
  const uint32_t suffix = random.next_uint();
  std::vector<WebAppShortcutsMenuItemInfo> shortcuts_menu_item_infos;
  for (int i = random.next_uint(4) + 1; i >= 0; --i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);
    WebAppShortcutsMenuItemInfo shortcut_info;
    shortcut_info.url = scope.Resolve("shortcut" + suffix_str);
    shortcut_info.name = base::UTF8ToUTF16("shortcut" + suffix_str);

    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_icons_any;
    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_icons_maskable;
    std::vector<WebAppShortcutsMenuItemInfo::Icon> shortcut_icons_monochrome;

    for (int j = random.next_uint(4) + 1; j >= 0; --j) {
      std::string icon_suffix_str = suffix_str + base::NumberToString(j);
      WebAppShortcutsMenuItemInfo::Icon shortcut_icon;
      shortcut_icon.url = scope.Resolve("/shortcuts/icon" + icon_suffix_str);
      // Within each shortcut_icons_*, square_size_px must be unique.
      shortcut_icon.square_size_px = (j * 10) + random.next_uint(10);
      int icon_purpose = random.next_uint(3);
      switch (icon_purpose) {
        case 0:
          shortcut_icons_any.push_back(std::move(shortcut_icon));
          break;
        case 1:
          shortcut_icons_maskable.push_back(std::move(shortcut_icon));
          break;
        case 2:
          shortcut_icons_monochrome.push_back(std::move(shortcut_icon));
          break;
      }
    }

    shortcut_info.SetShortcutIconInfosForPurpose(IconPurpose::ANY,
                                                 std::move(shortcut_icons_any));
    shortcut_info.SetShortcutIconInfosForPurpose(
        IconPurpose::MASKABLE, std::move(shortcut_icons_maskable));
    shortcut_info.SetShortcutIconInfosForPurpose(
        IconPurpose::MONOCHROME, std::move(shortcut_icons_monochrome));

    shortcuts_menu_item_infos.push_back(std::move(shortcut_info));
  }
  return shortcuts_menu_item_infos;
}

std::vector<IconSizes> CreateRandomDownloadedShortcutsMenuIconsSizes(
    RandomHelper& random) {
  std::vector<IconSizes> results;
  for (unsigned int i = 0; i < 3; ++i) {
    IconSizes result;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_any;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_maskable;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_monochrome;
    for (unsigned int j = 0; j < i; ++j) {
      shortcuts_menu_icon_sizes_any.push_back(random.next_uint(256) + 1);
      shortcuts_menu_icon_sizes_maskable.push_back(random.next_uint(256) + 1);
      shortcuts_menu_icon_sizes_monochrome.push_back(random.next_uint(256) + 1);
    }
    result.SetSizesForPurpose(IconPurpose::ANY,
                              std::move(shortcuts_menu_icon_sizes_any));
    result.SetSizesForPurpose(IconPurpose::MASKABLE,
                              std::move(shortcuts_menu_icon_sizes_maskable));
    result.SetSizesForPurpose(IconPurpose::MONOCHROME,
                              std::move(shortcuts_menu_icon_sizes_monochrome));
    results.push_back(std::move(result));
  }
  return results;
}

}  // namespace

std::unique_ptr<WebApp> CreateWebApp(const GURL& start_url,
                                     Source::Type source_type) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetStartUrl(start_url);
  web_app->AddSource(source_type);
  web_app->SetUserDisplayMode(DisplayMode::kStandalone);
  web_app->SetName("Name");

  return web_app;
}

std::unique_ptr<WebApp> CreateRandomWebApp(const GURL& base_url,
                                           const uint32_t seed) {
  RandomHelper random(seed);

  const std::string seed_str = base::NumberToString(seed);
  absl::optional<std::string> manifest_id;
  if (random.next_bool())
    manifest_id = "manifest_id_" + seed_str;
  const GURL scope = base_url.Resolve("scope" + seed_str + "/");
  const GURL start_url = scope.Resolve("start" + seed_str);
  const AppId app_id = GenerateAppId(manifest_id, start_url);

  const std::string name = "Name" + seed_str;
  const std::string description = "Description" + seed_str;
  const absl::optional<SkColor> theme_color = random.next_uint();
  absl::optional<SkColor> dark_mode_theme_color;
  const absl::optional<SkColor> background_color = random.next_uint();
  absl::optional<SkColor> dark_mode_background_color;
  const absl::optional<SkColor> synced_theme_color = random.next_uint();
  auto app = std::make_unique<WebApp>(app_id);

  // Generate all possible permutations of field values in a random way:
  if (AreSystemWebAppsSupported() && random.next_bool())
    app->AddSource(Source::kSystem);
  if (random.next_bool())
    app->AddSource(Source::kPolicy);
  if (random.next_bool())
    app->AddSource(Source::kWebAppStore);
  if (random.next_bool())
    app->AddSource(Source::kSync);
  if (random.next_bool())
    app->AddSource(Source::kDefault);
  // Must always be at least one source.
  if (!app->HasAnySources())
    app->AddSource(Source::kSync);

  if (random.next_bool()) {
    dark_mode_theme_color = SkColorSetA(random.next_uint(), SK_AlphaOPAQUE);
  }

  if (random.next_bool()) {
    dark_mode_background_color =
        SkColorSetA(random.next_uint(), SK_AlphaOPAQUE);
  }

  app->SetName(name);
  app->SetDescription(description);
  app->SetManifestId(manifest_id);
  app->SetStartUrl(GURL(start_url));
  app->SetScope(GURL(scope));
  app->SetThemeColor(theme_color);
  app->SetDarkModeThemeColor(dark_mode_theme_color);
  app->SetBackgroundColor(background_color);
  app->SetDarkModeBackgroundColor(dark_mode_background_color);
  app->SetIsLocallyInstalled(random.next_bool());
  app->SetIsFromSyncAndPendingInstallation(random.next_bool());

  const DisplayMode user_display_modes[3] = {
      DisplayMode::kBrowser, DisplayMode::kStandalone, DisplayMode::kTabbed};
  app->SetUserDisplayMode(user_display_modes[random.next_uint(3)]);

  const base::Time last_badging_time =
      base::Time::UnixEpoch() + base::Milliseconds(random.next_uint());
  app->SetLastBadgingTime(last_badging_time);

  const base::Time last_launch_time =
      base::Time::UnixEpoch() + base::Milliseconds(random.next_uint());
  app->SetLastLaunchTime(last_launch_time);

  const base::Time install_time =
      base::Time::UnixEpoch() + base::Milliseconds(random.next_uint());
  app->SetInstallTime(install_time);

  const DisplayMode display_modes[4] = {
      DisplayMode::kBrowser, DisplayMode::kMinimalUi, DisplayMode::kStandalone,
      DisplayMode::kFullscreen};
  app->SetDisplayMode(display_modes[random.next_uint(4)]);

  // Add only unique display modes.
  std::set<DisplayMode> display_mode_override;
  int num_display_mode_override_tries = random.next_uint(5);
  for (int i = 0; i < num_display_mode_override_tries; i++)
    display_mode_override.insert(display_modes[random.next_uint(4)]);
  app->SetDisplayModeOverride(std::vector<DisplayMode>(
      display_mode_override.begin(), display_mode_override.end()));

  if (random.next_bool())
    app->SetLaunchQueryParams(base::NumberToString(random.next_uint()));

  app->SetRunOnOsLoginMode(random.next_enum<RunOnOsLoginMode>());
  app->SetRunOnOsLoginOsIntegrationState(RunOnOsLoginMode::kNotRun);

  const SquareSizePx size = 256;
  const int num_icons = random.next_uint(10);
  std::vector<apps::IconInfo> manifest_icons(num_icons);
  for (int i = 0; i < num_icons; i++) {
    apps::IconInfo icon;
    icon.url =
        base_url.Resolve("/icon" + base::NumberToString(random.next_uint()));
    if (random.next_bool())
      icon.square_size_px = size;

    int purpose = random.next_uint(4);
    if (purpose == 0)
      icon.purpose = apps::IconInfo::Purpose::kAny;
    if (purpose == 1)
      icon.purpose = apps::IconInfo::Purpose::kMaskable;
    if (purpose == 2)
      icon.purpose = apps::IconInfo::Purpose::kMonochrome;
    // if (purpose == 3), leave purpose unset. Should default to ANY.

    manifest_icons[i] = icon;
  }
  app->SetManifestIcons(manifest_icons);
  if (random.next_bool())
    app->SetDownloadedIconSizes(IconPurpose::ANY, {size});
  if (random.next_bool())
    app->SetDownloadedIconSizes(IconPurpose::MASKABLE, {size});
  if (random.next_bool())
    app->SetDownloadedIconSizes(IconPurpose::MONOCHROME, {size});
  app->SetIsGeneratedIcon(random.next_bool());

  app->SetFileHandlers(CreateRandomFileHandlers(random.next_uint()));
  if (random.next_bool())
    app->SetShareTarget(CreateRandomShareTarget(random.next_uint()));
  app->SetProtocolHandlers(CreateRandomProtocolHandlers(random.next_uint()));
  app->SetUrlHandlers(CreateRandomUrlHandlers(random.next_uint()));
  if (random.next_bool()) {
    app->SetNoteTakingNewNoteUrl(
        scope.Resolve("new_note" + base::NumberToString(random.next_uint())));
  }
  app->SetCaptureLinks(random.next_enum<blink::mojom::CaptureLinks>());

  const int num_additional_search_terms = random.next_uint(8);
  std::vector<std::string> additional_search_terms(num_additional_search_terms);
  for (int i = 0; i < num_additional_search_terms; ++i) {
    additional_search_terms[i] =
        "Foo_" + seed_str + "_" + base::NumberToString(i);
  }
  app->SetAdditionalSearchTerms(std::move(additional_search_terms));

  app->SetShortcutsMenuItemInfos(
      CreateRandomShortcutsMenuItemInfos(scope, random));
  app->SetDownloadedShortcutsMenuIconsSizes(
      CreateRandomDownloadedShortcutsMenuIconsSizes(random));
  app->SetManifestUrl(base_url.Resolve("/manifest" + seed_str + ".json"));

  const int num_allowed_launch_protocols = random.next_uint(8);
  std::vector<std::string> allowed_launch_protocols(
      num_allowed_launch_protocols);
  for (int i = 0; i < num_allowed_launch_protocols; ++i) {
    allowed_launch_protocols[i] =
        "web+test_" + seed_str + "_" + base::NumberToString(i);
  }
  app->SetAllowedLaunchProtocols(std::move(allowed_launch_protocols));

  const int num_disallowed_launch_protocols = random.next_uint(8);
  std::vector<std::string> disallowed_launch_protocols(
      num_disallowed_launch_protocols);
  for (int i = 0; i < num_disallowed_launch_protocols; ++i) {
    disallowed_launch_protocols[i] =
        "web+disallowed_" + seed_str + "_" + base::NumberToString(i);
  }
  app->SetDisallowedLaunchProtocols(std::move(disallowed_launch_protocols));

  app->SetStorageIsolated(random.next_bool());

  app->SetWindowControlsOverlayEnabled(false);

  WebApp::SyncFallbackData sync_fallback_data;
  sync_fallback_data.name = "Sync" + name;
  sync_fallback_data.theme_color = synced_theme_color;
  sync_fallback_data.scope = app->scope();
  sync_fallback_data.icon_infos = app->manifest_icons();
  app->SetSyncFallbackData(std::move(sync_fallback_data));

  if (random.next_bool()) {
    LaunchHandler launch_handler;
    launch_handler.route_to = random.next_enum<LaunchHandler::RouteTo>();
    launch_handler.navigate_existing_client =
        random.next_enum<LaunchHandler::NavigateExistingClient>();
    app->SetLaunchHandler(launch_handler);
  }

  const base::Time manifest_update_time =
      base::Time::UnixEpoch() + base::Milliseconds(random.next_uint());
  app->SetManifestUpdateTime(manifest_update_time);

  if (random.next_bool())
    app->SetParentAppId(base::NumberToString(random.next_uint()));

  app->SetHandleLinks(random.next_enum<blink::mojom::HandleLinks>());

  if (random.next_bool())
    app->SetPermissionsPolicy(CreateRandomPermissionsPolicy(random));

  uint32_t install_source =
      random.next_uint(static_cast<int>(webapps::WebappInstallSource::COUNT));
  app->SetInstallSourceForMetrics(
      static_cast<webapps::WebappInstallSource>(install_source));

  if (IsChromeOsDataMandatory()) {
    // Use a separate random generator for CrOS so the result is deterministic
    // across cros and non-cros builds.
    RandomHelper cros_random(seed);
    auto chromeos_data = absl::make_optional<WebAppChromeOsData>();
    chromeos_data->show_in_launcher = cros_random.next_bool();
    chromeos_data->show_in_search = cros_random.next_bool();
    chromeos_data->show_in_management = cros_random.next_bool();
    chromeos_data->is_disabled = cros_random.next_bool();
    chromeos_data->oem_installed = cros_random.next_bool();
    app->SetWebAppChromeOsData(std::move(chromeos_data));
  }

  return app;
}

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    ForInstallableSite for_installable_site,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback), true /*accept*/,
                                std::move(web_app_info)));
}

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    ForInstallableSite for_installable_site,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback),
                                false /*accept*/, std::move(web_app_info)));
}

AppId InstallPwaForCurrentUrl(Browser* browser) {
  // Depending on the installability criteria, different dialogs can be used.
  chrome::SetAutoAcceptWebAppDialogForTesting(true, true);
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  WebAppTestInstallObserver observer(browser->profile());
  observer.BeginListening();
  CHECK(chrome::ExecuteCommand(browser, IDC_INSTALL_PWA));
  AppId app_id = observer.Wait();
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  chrome::SetAutoAcceptWebAppDialogForTesting(false, false);
  return app_id;
}

void CheckServiceWorkerStatus(const GURL& url,
                              content::StoragePartition* storage_partition,
                              content::ServiceWorkerCapability status) {
  base::RunLoop run_loop;
  content::ServiceWorkerContext* service_worker_context =
      storage_partition->GetServiceWorkerContext();
  service_worker_context->CheckHasServiceWorker(
      url, blink::StorageKey(url::Origin::Create(url)),
      base::BindLambdaForTesting(
          [&run_loop, status](content::ServiceWorkerCapability capability) {
            CHECK_EQ(status, capability);
            run_loop.Quit();
          }));
  run_loop.Run();
}

void SetWebAppSettingsDictPref(Profile* profile, const base::StringPiece pref) {
  base::JSONReader::ValueWithError result =
      base::JSONReader::ReadAndReturnValueWithError(
          pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  DCHECK(result.value && result.value->is_dict()) << result.error_message;
  profile->GetPrefs()->Set(prefs::kWebAppSettings, std::move(*result.value));
}

}  // namespace test
}  // namespace web_app
