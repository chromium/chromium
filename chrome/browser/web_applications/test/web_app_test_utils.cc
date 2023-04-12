// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test_utils.h"

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <random>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/sync/base/time.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/manifest/capture_links.mojom-shared.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"
#include "url/origin.h"

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
      permissions_policy[i].allowed_origins.emplace_back(
          origin,
          /*has_subdomain_wildcard=*/false);
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

std::vector<ScopeExtensionInfo> CreateRandomScopeExtensions(
    uint32_t suffix,
    RandomHelper& random) {
  std::vector<ScopeExtensionInfo> scope_extensions;

  for (unsigned int i = 0; i < 3; ++i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);

    ScopeExtensionInfo scope_extension;
    scope_extension.origin =
        url::Origin::Create(GURL("https://app-" + suffix_str + ".com/"));
    scope_extension.has_origin_wildcard = random.next_bool();
    scope_extensions.push_back(std::move(scope_extension));
  }

  return scope_extensions;
}

std::vector<WebAppShortcutsMenuItemInfo> CreateRandomShortcutsMenuItemInfos(
    const GURL& scope,
    int num,
    RandomHelper& random) {
  const uint32_t suffix = random.next_uint();
  std::vector<WebAppShortcutsMenuItemInfo> shortcuts_menu_item_infos;
  for (int i = num - 1; i >= 0; --i) {
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
    int num,
    RandomHelper& random) {
  std::vector<IconSizes> results;
  for (int i = 0; i < num; ++i) {
    IconSizes result;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_any;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_maskable;
    std::vector<SquareSizePx> shortcuts_menu_icon_sizes_monochrome;
    for (int j = 0; j < i; ++j) {
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

std::vector<blink::Manifest::ImageResource> CreateRandomHomeTabIcons(
    RandomHelper& random) {
  std::vector<blink::Manifest::ImageResource> icons;

  for (int i = random.next_uint(4) + 1; i >= 0; --i) {
    blink::Manifest::ImageResource icon;

    int mime_type = random.next_uint(3);
    switch (mime_type) {
      case 0:
        icon.src = GURL("https://example.com/image" + base::NumberToString(i) +
                        ".png");
        icon.type = base::UTF8ToUTF16(std::string("image/png"));
        break;
      case 1:
        icon.src = GURL("https://example.com/image" + base::NumberToString(i) +
                        ".svg");
        icon.type = base::UTF8ToUTF16(std::string("image/svg+xml"));
        break;
      case 2:
        icon.src = GURL("https://example.com/image" + base::NumberToString(i) +
                        ".webp");
        icon.type = base::UTF8ToUTF16(std::string("image/webp"));
        break;
    }

    // Icon sizes can be non square
    std::vector<gfx::Size> sizes;
    for (int j = random.next_uint(3) + 1; j > 0; --j) {
      sizes.emplace_back(j * random.next_uint(200), j * random.next_uint(200));
    }
    icon.sizes = std::move(sizes);

    std::vector<blink::mojom::ManifestImageResource_Purpose> purposes = {
        blink::mojom::ManifestImageResource_Purpose::ANY,
        blink::mojom::ManifestImageResource_Purpose::MASKABLE,
        blink::mojom::ManifestImageResource_Purpose::MONOCHROME};

    std::vector<blink::mojom::ManifestImageResource_Purpose> purpose;

    for (int j = random.next_uint(purposes.size()); j >= 0; --j) {
      unsigned index = random.next_uint(purposes.size());
      purpose.push_back(purposes[index]);
      purposes.erase(purposes.begin() + index);
    }
    icon.purpose = std::move(purpose);
    icons.push_back(std::move(icon));
  }
  return icons;
}

proto::WebAppOsIntegrationState GenerateRandomWebAppOsIntegrationState(
    RandomHelper& random,
    WebApp& app) {
  proto::WebAppOsIntegrationState state;

  // Randomly fill shortcuts data.
  auto* shortcuts = state.mutable_shortcut();
  shortcuts->set_title(app.untranslated_name());
  shortcuts->set_description(app.untranslated_description());
  auto* first_shortcut = shortcuts->add_icon_data_any();
  first_shortcut->set_icon_size(32);
  first_shortcut->set_timestamp(syncer::TimeToProtoTime(
      base::Time::UnixEpoch() + base::Milliseconds(random.next_uint())));

  // Randomly fill protocols_handled.
  auto* protocols_handled = state.mutable_protocols_handled();
  int num_protocols = random.next_uint(/*bound=*/3);
  for (int i = 0; i < num_protocols; i++) {
    auto* protocol = protocols_handled->add_protocols();
    protocol->set_protocol(base::StrCat({"web+test", base::NumberToString(i)}));
    protocol->set_url(
        base::StrCat({app.start_url().spec(), base::NumberToString(i)}));
  }

  // Randomly fill run_on_os_login.
  const proto::RunOnOsLoginMode run_on_os_login_modes[3] = {
      proto::RunOnOsLoginMode::NOT_RUN, proto::RunOnOsLoginMode::WINDOWED,
      proto::RunOnOsLoginMode::MINIMIZED};
  state.mutable_run_on_os_login()->set_run_on_os_login_mode(
      run_on_os_login_modes[random.next_uint(/*bound=*/3)]);

  // Randomly fill uninstallation registration logic.
  state.mutable_uninstall_registration()->set_registered_with_os(
      random.next_bool());

  // Randomly fill shortcuts menu information.
  auto* shortcut_menus = state.mutable_shortcut_menus();
  int count_shortcut_menu_items = random.next_uint(/*bound=*/2);
  for (int i = 0; i < count_shortcut_menu_items; i++) {
    auto* menu_info = shortcut_menus->add_shortcut_menu_info();
    menu_info->set_shortcut_name(
        base::StrCat({"shortcut_name", base::NumberToString(i)}));
    menu_info->set_shortcut_launch_url(
        base::StrCat({app.scope().spec(), base::NumberToString(i)}));

    auto* data_any = menu_info->add_icon_data_any();
    data_any->set_icon_size(16 * random.next_uint(/*bound=*/4));
    data_any->set_timestamp(syncer::TimeToProtoTime(
        base::Time::UnixEpoch() + base::Milliseconds(random.next_uint())));

    auto* data_maskable = menu_info->add_icon_data_maskable();
    data_maskable->set_icon_size(16 * random.next_uint(/*bound=*/4));
    data_maskable->set_timestamp(syncer::TimeToProtoTime(
        base::Time::UnixEpoch() + base::Milliseconds(random.next_uint())));

    auto* data_monochrome = menu_info->add_icon_data_monochrome();
    data_monochrome->set_icon_size(16 * random.next_uint(/*bound=*/4));
    data_monochrome->set_timestamp(syncer::TimeToProtoTime(
        base::Time::UnixEpoch() + base::Milliseconds(random.next_uint())));
  }

  // Randomly fill file handling information.
  auto* file_handling = state.mutable_file_handling();
  int count_file_handlers = random.next_uint(/*bound=*/2);
  for (int i = 0; i < count_file_handlers; i++) {
    auto* file_handlers = file_handling->add_file_handlers();
    int count_accepts = random.next_uint(/*bound=*/2);
    file_handlers->set_action(
        base::StrCat({"https://file.open/", base::NumberToString(i)}));
    file_handlers->set_display_name(
        base::StrCat({"file_type", base::NumberToString(i)}));
    for (int j = 0; j < count_accepts; j++) {
      auto* accept = file_handlers->add_accept();
      accept->set_mimetype(
          base::StrCat({"application/type", base::NumberToString(i)}));
      accept->add_file_extensions(
          base::StrCat({"foo", base::NumberToString(i)}));
      accept->add_file_extensions(
          base::StrCat({"bar", base::NumberToString(i)}));
    }
  }

  return state;
}

}  // namespace

std::string GetExternalPrefMigrationTestName(
    const ::testing::TestParamInfo<ExternalPrefMigrationTestCases>& info) {
  switch (info.param) {
    case ExternalPrefMigrationTestCases::kDisableMigrationReadPref:
      return "DisableMigration_ReadFromPrefs";
    case ExternalPrefMigrationTestCases::kDisableMigrationReadDB:
      return "DisableMigration_ReadFromDB";
    case ExternalPrefMigrationTestCases::kEnableMigrationReadPref:
      return "EnableMigration_ReadFromPrefs";
    case ExternalPrefMigrationTestCases::kEnableMigrationReadDB:
      return "EnableMigration_ReadFromDB";
  }
}

std::string GetOsIntegrationSubManagersTestName(
    const ::testing::TestParamInfo<OsIntegrationSubManagersState>& info) {
  switch (info.param) {
    case OsIntegrationSubManagersState::kSaveStateToDB:
      return "OSIntegrationSubManagers_SaveStateToDB";
    case OsIntegrationSubManagersState::kSaveStateAndExecute:
      return "OSIntegrationSubManagers_SaveStateAndExecute";
    case OsIntegrationSubManagersState::kDisabled:
      return "OSIntegrationSubManagers_Disabled";
  }
}

std::unique_ptr<WebApp> CreateWebApp(const GURL& start_url,
                                     WebAppManagement::Type source_type) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetStartUrl(start_url);
  web_app->AddSource(source_type);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetName("Name");
  web_app->SetIsLocallyInstalled(true);

  return web_app;
}

std::unique_ptr<WebApp> CreateRandomWebApp(const GURL& base_url,
                                           const uint32_t seed,
                                           bool allow_system_source) {
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
  std::vector<WebAppManagement::Type> management_types;

  // Generate all possible permutations of field values in a random way:
  if (allow_system_source && random.next_bool()) {
    app->AddSource(WebAppManagement::kSystem);
    management_types.push_back(WebAppManagement::kSystem);
  }

  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kPolicy);
    management_types.push_back(WebAppManagement::kPolicy);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kWebAppStore);
    management_types.push_back(WebAppManagement::kWebAppStore);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kSync);
    management_types.push_back(WebAppManagement::kSync);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kDefault);
    management_types.push_back(WebAppManagement::kDefault);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kSubApp);
    management_types.push_back(WebAppManagement::kSubApp);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kKiosk);
    management_types.push_back(WebAppManagement::kKiosk);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kCommandLine);
    management_types.push_back(WebAppManagement::kCommandLine);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kOem);
    management_types.push_back(WebAppManagement::kOem);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kOneDriveIntegration);
    management_types.push_back(WebAppManagement::kOneDriveIntegration);
  }

  // Must always be at least one source.
  if (!app->HasAnySources()) {
    app->AddSource(WebAppManagement::kSync);
    management_types.push_back(WebAppManagement::kSync);
  }

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

  const mojom::UserDisplayMode user_display_modes[3] = {
      mojom::UserDisplayMode::kBrowser, mojom::UserDisplayMode::kStandalone,
      mojom::UserDisplayMode::kTabbed};
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
  app->SetScopeExtensions(
      CreateRandomScopeExtensions(random.next_uint(), random));
  if (random.next_bool()) {
    app->SetLockScreenStartUrl(scope.Resolve(
        "lock_screen_start_url" + base::NumberToString(random.next_uint())));
  }
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

  int num_shortcut_menus = static_cast<int>(random.next_uint(4)) + 1;
  app->SetShortcutsMenuItemInfos(
      CreateRandomShortcutsMenuItemInfos(scope, num_shortcut_menus, random));
  app->SetDownloadedShortcutsMenuIconsSizes(
      CreateRandomDownloadedShortcutsMenuIconsSizes(num_shortcut_menus,
                                                    random));
  CHECK_EQ(app->shortcuts_menu_item_infos().size(),
           app->downloaded_shortcuts_menu_icons_sizes().size());
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

  app->SetWindowControlsOverlayEnabled(false);

  WebApp::SyncFallbackData sync_fallback_data;
  sync_fallback_data.name = "Sync" + name;
  sync_fallback_data.theme_color = synced_theme_color;
  sync_fallback_data.scope = app->scope();
  sync_fallback_data.icon_infos = app->manifest_icons();
  app->SetSyncFallbackData(std::move(sync_fallback_data));

  if (random.next_bool()) {
    app->SetLaunchHandler(
        LaunchHandler{random.next_enum<LaunchHandler::ClientMode>()});
  }

  const base::Time manifest_update_time =
      base::Time::UnixEpoch() + base::Milliseconds(random.next_uint());
  app->SetManifestUpdateTime(manifest_update_time);

  if (random.next_bool())
    app->SetParentAppId(base::NumberToString(random.next_uint()));

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
    // Comply with DCHECK that system apps cannot be OEM installed.
    if (app->IsSystemApp())
      chromeos_data->oem_installed = false;
    app->SetWebAppChromeOsData(std::move(chromeos_data));
  }

  WebApp::ExternalConfigMap management_to_external_config;
  for (WebAppManagement::Type type : management_types) {
    if (type == WebAppManagement::kSync)
      continue;
    base::flat_set<GURL> install_urls;
    WebApp::ExternalManagementConfig config;
    if (random.next_bool())
      install_urls.emplace(base_url.Resolve("installer1_" + seed_str + "/"));
    if (random.next_bool())
      install_urls.emplace(base_url.Resolve("installer2_" + seed_str + "/"));
    config.is_placeholder = random.next_bool();
    config.install_urls = install_urls;
    management_to_external_config.insert_or_assign(type, std::move(config));
  }

  app->SetWebAppManagementExternalConfigMap(management_to_external_config);

  app->SetAppSizeInBytes(random.next_uint());
  app->SetDataSizeInBytes(random.next_uint());

  if (random.next_bool()) {
    blink::Manifest::TabStrip tab_strip;

    if (random.next_bool()) {
      blink::Manifest::HomeTabParams home_tab_params;
      if (random.next_bool()) {
        home_tab_params.icons = CreateRandomHomeTabIcons(random);
      }
      tab_strip.home_tab = std::move(home_tab_params);
    } else {
      tab_strip.home_tab =
          random.next_enum<blink::mojom::TabStripMemberVisibility>();
    }

    if (random.next_bool()) {
      blink::Manifest::NewTabButtonParams new_tab_button_params;
      if (random.next_bool()) {
        new_tab_button_params.url = scope.Resolve(
            "new_tab_button_url" + base::NumberToString(random.next_uint()));
      }
      tab_strip.new_tab_button = new_tab_button_params;
    } else {
      tab_strip.new_tab_button =
          random.next_enum<blink::mojom::TabStripMemberVisibility>();
    }
    app->SetTabStrip(std::move(tab_strip));
  }

  app->SetAlwaysShowToolbarInFullscreen(random.next_bool());

  app->SetCurrentOsIntegrationStates(
      GenerateRandomWebAppOsIntegrationState(random, *app));

  if (random.next_bool()) {
    constexpr size_t kNumLocationTypes =
        absl::variant_size<IsolatedWebAppLocation>::value;
    auto path = base::FilePath::FromUTF8Unsafe(seed_str);
    IsolatedWebAppLocation location_types[] = {
        InstalledBundle{.path = path},
        DevModeBundle{.path = path},
        DevModeProxy{.proxy_url = url::Origin::Create(GURL(
                         base::StrCat({"https://proxy-", seed_str, ".com/"})))},
    };
    static_assert(std::size(location_types) == kNumLocationTypes);

    IsolatedWebAppLocation location(
        location_types[random.next_uint(kNumLocationTypes)]);
    app->SetIsolationData(WebApp::IsolationData(location));
  }

  return app;
}

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback), true /*accept*/,
                                std::move(web_app_info)));
}

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
      url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
      base::BindLambdaForTesting(
          [&run_loop, status](content::ServiceWorkerCapability capability) {
            CHECK_EQ(status, capability);
            run_loop.Quit();
          }));
  run_loop.Run();
}

void SetWebAppSettingsListPref(Profile* profile, const base::StringPiece pref) {
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  DCHECK(result.has_value()) << result.error().message;
  DCHECK(result->is_list());
  profile->GetPrefs()->Set(prefs::kWebAppSettings, std::move(*result));
}

void AddInstallUrlData(PrefService* pref_service,
                       WebAppSyncBridge* sync_bridge,
                       const AppId& app_id,
                       const GURL& url,
                       const ExternalInstallSource& source) {
  ScopedRegistryUpdate update(sync_bridge);
  WebApp* app_to_update = update->UpdateApp(app_id);
  DCHECK(app_to_update);

  // Adding external app data (source and URL) to web_app DB.
  app_to_update->AddInstallURLToManagementExternalConfigMap(
      ConvertExternalInstallSourceToSource(source), url);

  // Add to legacy external pref storage.
  // TODO(crbug.com/1339965): Clean up after external pref migration is
  // complete.
  ExternallyInstalledWebAppPrefs(pref_service).Insert(url, app_id, source);
}

void AddInstallUrlAndPlaceholderData(PrefService* pref_service,
                                     WebAppSyncBridge* sync_bridge,
                                     const AppId& app_id,
                                     const GURL& url,
                                     const ExternalInstallSource& source,
                                     bool is_placeholder) {
  ScopedRegistryUpdate update(sync_bridge);
  ExternallyInstalledWebAppPrefs prefs(pref_service);
  WebApp* app_to_update = update->UpdateApp(app_id);
  DCHECK(app_to_update);

  // Adding install_url, source and placeholder information to the web_app DB.
  app_to_update->AddExternalSourceInformation(
      ConvertExternalInstallSourceToSource(source), url, is_placeholder);

  // Add to legacy external pref storage.
  // TODO(crbug.com/1339965): Clean up after external pref migration is
  // complete.
  prefs.Insert(url, app_id, source);
  prefs.SetIsPlaceholder(url, is_placeholder);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
ScopedSkipMainProfileCheck::ScopedSkipMainProfileCheck() {
  EXPECT_FALSE(IsMainProfileCheckSkippedForTesting());
  SetSkipMainProfileCheckForTesting(/*skip_check=*/true);
}

ScopedSkipMainProfileCheck::~ScopedSkipMainProfileCheck() {
  SetSkipMainProfileCheckForTesting(/*skip_check=*/false);
}
#endif

}  // namespace test
}  // namespace web_app
