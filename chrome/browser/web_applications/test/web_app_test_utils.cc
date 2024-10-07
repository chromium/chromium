// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/web_applications/test/web_app_test_utils.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/clamped_math.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_isolation_data.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/base32/base32.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/sync/base/time.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/manifest/capture_links.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

std::vector<std::string> test_features = {
    "default_on_feature", "default_self_feature", "default_disabled_feature"};

constexpr std::string_view kEd25519PublicKeyBase64 =
    "3QrKLntnXWmBkrbLLoobC0uh7u0ahMcKQSqsbX4q7MM=";
constexpr std::string_view kEd25519SignatureHex =
    "94FBD9E3E0C0A4CD475067F1DC315E9EA30A5010350A9D3A4392CA9E0EBA854315372D7A6B"
    "48B6C4DDF9A5FAAE4E159D3B80632413DA2850B54A4D5D98EAE906";

constexpr std::string_view kEcdsaP256PublicKeyBase64 =
    "AxyCBzvQfu1yaF01392k1gu2qCtT1uA2+WfIEhlyJB5S";
constexpr std::string_view kEcdsaP256SHA256SignatureHex =
    "3044022007381524F538B04F99CCC62703F06C87F66EF41BDA18A22D8E57952AA23E53A6"
    "022063C7F81D3A44798CB95823FA38FC23B15E0483744657FF49E1E83AB8C06B63C2";

}  // namespace

namespace web_app {
namespace test {

namespace {

class RandomHelper {
 public:
  explicit RandomHelper(const uint32_t seed, bool non_zero)
      :  // Seed of 0 and 1 generate the same sequence, so skip 0.
        generator_(seed + 1),
        distribution_(0u, UINT32_MAX),
        non_zero_(non_zero) {}

  uint32_t next_uint() {
    return std::max(distribution_(generator_),
                    static_cast<uint32_t>(non_zero_));
  }

  // Return an unsigned int between 0 (inclusive) and bound (exclusive).
  uint32_t next_uint(uint32_t bound) {
    return bound <= 1 ? 0
                      : std::max(next_uint() % bound,
                                 static_cast<uint32_t>(non_zero_));
  }

  bool next_bool() { return non_zero_ || next_uint() & 1u; }

  base::Time next_time() {
    return base::Time::UnixEpoch() + base::Milliseconds(next_uint());
  }

  int64_t next_proto_time() { return syncer::TimeToProtoTime(next_time()); }

  template <typename T, auto min = T::kMinValue, auto max = T::kMaxValue>
  T next_enum() {
    constexpr uint32_t min_u32 = static_cast<uint32_t>(min);
    constexpr uint32_t max_u32 = static_cast<uint32_t>(max);
    static_assert(min_u32 <= max_u32, "min cannot be greater than max");
    return static_cast<T>(min_u32 + next_uint(max_u32 - min_u32));
  }

 private:
  std::default_random_engine generator_;
  std::uniform_int_distribution<uint32_t> distribution_;
  bool non_zero_;
};

#define NEXT_PROTO_ENUM(random_helper, T, skip_zero)         \
  static_cast<T>(static_cast<uint32_t>(skip_zero) +          \
                 random_helper.next_uint(T##_MAX - T##_MIN - \
                                         static_cast<uint32_t>(skip_zero)))

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
      random.next_uint(test_features.size());

  std::vector<std::string> available_features = test_features;

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
          *blink::OriginWithPossibleWildcards::FromOrigin(origin));
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

ScopeExtensions CreateRandomScopeExtensions(uint32_t suffix,
                                            RandomHelper& random) {
  ScopeExtensions scope_extensions;
  for (unsigned int i = 0; i < 3; ++i) {
    std::string suffix_str =
        base::NumberToString(suffix) + base::NumberToString(i);

    ScopeExtensionInfo scope_extension;
    scope_extension.origin =
        url::Origin::Create(GURL("https://app-" + suffix_str + ".com/"));
    scope_extension.has_origin_wildcard = random.next_bool();
    scope_extensions.insert(std::move(scope_extension));
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
      int x = j * random.next_uint(200);
      int y = j * random.next_uint(200);
      sizes.emplace_back(x, y);
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

std::vector<blink::SafeUrlPattern> CreateRandomScopePatterns(
    RandomHelper& random) {
  std::vector<blink::SafeUrlPattern> scope_patterns;

  for (int i = random.next_uint(4) + 1; i >= 0; --i) {
    blink::SafeUrlPattern url_pattern;

    for (int j = random.next_uint(4) + 1; j >= 0; --j) {
      liburlpattern::Part part;

      std::vector<liburlpattern::PartType> part_types = {
          liburlpattern::PartType::kFixed,
          liburlpattern::PartType::kFullWildcard,
          liburlpattern::PartType::kSegmentWildcard};
      std::vector<liburlpattern::Modifier> modifiers = {
          liburlpattern::Modifier::kZeroOrMore,
          liburlpattern::Modifier::kOptional,
          liburlpattern::Modifier::kOneOrMore, liburlpattern::Modifier::kNone};
      part.type = part_types[random.next_uint(part_types.size())];
      part.value = "value" + base::NumberToString(j);
      if (part.type == liburlpattern::PartType::kFullWildcard ||
          part.type == liburlpattern::PartType::kSegmentWildcard) {
        part.prefix = "prefix" + base::NumberToString(j);
        part.name = "name" + base::NumberToString(j);
        part.suffix = "suffix" + base::NumberToString(j);
      }

      part.modifier = modifiers[random.next_uint(modifiers.size())];

      url_pattern.pathname.push_back(std::move(part));
    }

    scope_patterns.push_back(std::move(url_pattern));
  }
  return scope_patterns;
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
  first_shortcut->set_timestamp(random.next_proto_time());

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
  state.mutable_uninstall_registration()->set_display_name(
      app.untranslated_name());

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
    data_any->set_timestamp(random.next_proto_time());

    auto* data_maskable = menu_info->add_icon_data_maskable();
    data_maskable->set_icon_size(16 * random.next_uint(/*bound=*/4));
    data_maskable->set_timestamp(random.next_proto_time());

    auto* data_monochrome = menu_info->add_icon_data_monochrome();
    data_monochrome->set_icon_size(16 * random.next_uint(/*bound=*/4));
    data_monochrome->set_timestamp(random.next_proto_time());
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

std::optional<IsolatedWebAppIntegrityBlockData> CreateIntegrityBlockData(
    RandomHelper& random) {
  if (!random.next_bool()) {
    return std::nullopt;
  }

  auto signatures = CreateSignatures();

  std::mt19937 rng(random.next_uint());
  base::ranges::shuffle(signatures, rng);

  size_t signatures_count = random.next_uint(signatures.size()) + 1;
  signatures.erase(signatures.begin() + signatures_count, signatures.end());

  return IsolatedWebAppIntegrityBlockData(std::move(signatures));
}

}  // namespace

std::unique_ptr<WebApp> CreateWebApp(const GURL& start_url,
                                     WebAppManagement::Type source_type) {
  const webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);

  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetStartUrl(start_url);
  web_app->AddSource(source_type);
  web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
  web_app->SetName("Name");
  // Adding OS integration to this app introduces too many edge cases in tests.
  // Simply set this to partially installed w/ no os integration, and the
  // correct OS integration state to match that.
  web_app->SetInstallState(
      proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  proto::WebAppOsIntegrationState os_state;
  web_app->SetCurrentOsIntegrationStates(os_state);

  return web_app;
}

std::unique_ptr<WebApp> CreateRandomWebApp(CreateRandomWebAppParams params) {
  RandomHelper random(params.seed, params.non_zero);

  const std::string seed_str = base::NumberToString(params.seed);
  std::optional<std::string> relative_manifest_id;
  if (random.next_bool()) {
    std::string manifest_id_path = "manifest_id_" + seed_str;
    if (random.next_bool()) {
      manifest_id_path += "?query=test";
    }
    if (random.next_bool()) {
      manifest_id_path += "#fragment";
    }
    relative_manifest_id = manifest_id_path;
  }
  std::string scope_path = "scope" + seed_str + "/";
  if (random.next_bool()) {
    scope_path += "?query=test";
  }
  if (random.next_bool()) {
    scope_path += "#fragment";
  }
  const GURL scope(params.base_url.Resolve(scope_path));
  const GURL start_url = scope.Resolve("start" + seed_str);
  const webapps::AppId app_id = GenerateAppId(relative_manifest_id, start_url);

  const std::string name = "Name" + seed_str;
  const std::string description = "Description" + seed_str;
  const std::optional<SkColor> theme_color = random.next_uint();
  std::optional<SkColor> dark_mode_theme_color;
  const std::optional<SkColor> background_color = random.next_uint();
  std::optional<SkColor> dark_mode_background_color;
  const std::optional<SkColor> synced_theme_color = random.next_uint();
  auto app = std::make_unique<WebApp>(app_id);
  std::vector<WebAppManagement::Type> management_types;

  // Generate all possible permutations of field values in a random way:
  if (params.allow_system_source && random.next_bool()) {
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
    app->AddSource(WebAppManagement::kUserInstalled);
    management_types.push_back(WebAppManagement::kUserInstalled);
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
    app->AddSource(WebAppManagement::kIwaShimlessRma);
    management_types.push_back(WebAppManagement::kIwaShimlessRma);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kIwaPolicy);
    management_types.push_back(WebAppManagement::kIwaPolicy);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kIwaUserInstalled);
    management_types.push_back(WebAppManagement::kIwaUserInstalled);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kOem);
    management_types.push_back(WebAppManagement::kOem);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kOneDriveIntegration);
    management_types.push_back(WebAppManagement::kOneDriveIntegration);
  }
  if (random.next_bool()) {
    app->AddSource(WebAppManagement::kApsDefault);
    management_types.push_back(WebAppManagement::kApsDefault);
  }

  // Must always be at least one source.
  if (!app->HasAnySources()) {
    if (base::FeatureList::IsEnabled(
            features::kWebAppDontAddExistingAppsToSync)) {
      app->AddSource(WebAppManagement::kUserInstalled);
      management_types.push_back(WebAppManagement::kUserInstalled);
    } else {
      app->AddSource(WebAppManagement::kSync);
      management_types.push_back(WebAppManagement::kSync);
    }
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
  if (relative_manifest_id) {
    app->SetManifestId(
        GenerateManifestId(relative_manifest_id.value(), start_url));
  }
  app->SetStartUrl(GURL(start_url));
  app->SetScope(GURL(scope));
  app->SetThemeColor(theme_color);
  app->SetDarkModeThemeColor(dark_mode_theme_color);
  app->SetBackgroundColor(background_color);
  app->SetDarkModeBackgroundColor(dark_mode_background_color);
  app->SetInstallState(random.next_enum<proto::InstallState,
                                        /*min=*/proto::InstallState_MIN,
                                        /*max=*/
                                        proto::InstallState_MAX>());
  app->SetIsFromSyncAndPendingInstallation(random.next_bool());

  const sync_pb::WebAppSpecifics::UserDisplayMode user_display_modes[3] = {
      sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER,
      sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE,
      sync_pb::WebAppSpecifics_UserDisplayMode_TABBED};
  // Explicitly set a UserDisplayMode for each platform (instead of calling
  // `SetUserDisplayMode` which sets the current platform's value only) so the
  // test expectations are consistent across platforms.
  {
    // Copy proto, retaining existing fields (including unknown fields).
    sync_pb::WebAppSpecifics sync_proto = app->sync_proto();
    if (random.next_bool()) {
      sync_proto.set_user_display_mode_default(
          user_display_modes[random.next_uint(3)]);
    }
    // Must have at least one platform's UserDisplayMode set.
    if (!sync_proto.has_user_display_mode_default() || random.next_bool()) {
      sync_proto.set_user_display_mode_cros(
          user_display_modes[random.next_uint(3)]);
    }

    if (random.next_bool()) {
      sync_proto.set_user_launch_ordinal(
          syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue());
    }

    if (random.next_bool()) {
      sync_proto.set_user_page_ordinal(
          syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue());
    }

    app->SetSyncProto(std::move(sync_proto));
  }

  app->SetLastBadgingTime(random.next_time());

  app->SetLastLaunchTime(random.next_time());

  app->SetFirstInstallTime(random.next_time());

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

  const SquareSizePx size = 256;
  const int num_icons = random.next_uint(10);
  std::vector<apps::IconInfo> manifest_icons(num_icons);
  for (int i = 0; i < num_icons; i++) {
    apps::IconInfo icon;
    icon.url = params.base_url.Resolve(
        "/icon" + base::NumberToString(random.next_uint()));
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

  ScopeExtensions validated_scope_extensions;
  base::ranges::copy_if(app->scope_extensions(),
                        std::inserter(validated_scope_extensions,
                                      validated_scope_extensions.begin()),
                        [&random](const ScopeExtensionInfo& extension) {
                          return random.next_bool();
                        });
  app->SetValidatedScopeExtensions(validated_scope_extensions);

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

  int num_shortcut_items = static_cast<int>(random.next_uint(4)) + 1;
  auto item_infos =
      CreateRandomShortcutsMenuItemInfos(scope, num_shortcut_items, random);
  auto icons_sizes =
      CreateRandomDownloadedShortcutsMenuIconsSizes(num_shortcut_items, random);
  CHECK_EQ(item_infos.size(), icons_sizes.size());
  for (int i = 0; i < num_shortcut_items; ++i) {
    item_infos[i].downloaded_icon_sizes = std::move(icons_sizes[i]);
  }
  icons_sizes.clear();
  app->SetShortcutsMenuInfo(std::move(item_infos));

  app->SetManifestUrl(
      params.base_url.Resolve("/manifest" + seed_str + ".json"));

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

  {
    // Copy proto, retaining existing fields (including unknown fields).
    sync_pb::WebAppSpecifics sync_proto = app->sync_proto();
    sync_proto.set_name("Sync" + name);
    if (synced_theme_color.has_value()) {
      sync_proto.set_theme_color(synced_theme_color.value());
    }
    sync_proto.set_scope(app->scope().spec());
    for (const apps::IconInfo& icon_info : app->manifest_icons()) {
      *(sync_proto.add_icon_infos()) = AppIconInfoToSyncProto(icon_info);
    }
    app->SetSyncProto(std::move(sync_proto));
  }

  if (random.next_bool()) {
    app->SetLaunchHandler(
        LaunchHandler{random.next_enum<LaunchHandler::ClientMode>()});
  }

  app->SetManifestUpdateTime(random.next_time());

  if (random.next_bool())
    app->SetParentAppId(base::NumberToString(random.next_uint()));

  if (random.next_bool())
    app->SetPermissionsPolicy(CreateRandomPermissionsPolicy(random));

  uint32_t install_source =
      random.next_uint(static_cast<int>(webapps::WebappInstallSource::COUNT));
  app->SetLatestInstallSource(
      static_cast<webapps::WebappInstallSource>(install_source));

  if (IsChromeOsDataMandatory()) {
    // Use a separate random generator for CrOS so the result is deterministic
    // across cros and non-cros builds.
    RandomHelper cros_random(params.seed, params.non_zero);
    auto chromeos_data = std::make_optional<WebAppChromeOsData>();
    chromeos_data->show_in_launcher = cros_random.next_bool();
    chromeos_data->show_in_search_and_shelf = cros_random.next_bool();
    chromeos_data->show_in_management = cros_random.next_bool();
    chromeos_data->is_disabled = cros_random.next_bool();
    chromeos_data->oem_installed = cros_random.next_bool();
    // Comply with DCHECK that system apps and shimless RMA apps cannot be OEM
    // installed.
    if (app->IsSystemApp() || app->IsIwaShimlessRmaApp()) {
      chromeos_data->oem_installed = false;
    }
    app->SetWebAppChromeOsData(std::move(chromeos_data));
  }

  WebApp::ExternalConfigMap management_to_external_config;
  for (WebAppManagement::Type type : management_types) {
    if (type == WebAppManagement::kSync ||
        type == WebAppManagement::kUserInstalled ||
        WebAppManagement::IsIwaType(type)) {
      continue;
    }
    base::flat_set<GURL> install_urls;
    base::flat_set<std::string> additional_policy_ids;
    WebApp::ExternalManagementConfig config;
    if (random.next_bool()) {
      install_urls.emplace(
          params.base_url.Resolve("installer1_" + seed_str + "/"));
    }
    if (random.next_bool()) {
      install_urls.emplace(
          params.base_url.Resolve("installer2_" + seed_str + "/"));
    }
    if (random.next_bool()) {
      additional_policy_ids.emplace("policy_id_1_" + seed_str);
    }
    if (random.next_bool()) {
      additional_policy_ids.emplace("policy_id_2_" + seed_str);
    }
    config.is_placeholder = random.next_bool();
    config.install_urls = std::move(install_urls);
    config.additional_policy_ids = std::move(additional_policy_ids);
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
      if (random.next_bool()) {
        home_tab_params.scope_patterns = CreateRandomScopePatterns(random);
      }
      tab_strip.home_tab = std::move(home_tab_params);
    } else {
      tab_strip.home_tab =
          random.next_enum<blink::mojom::TabStripMemberVisibility>();
    }

    blink::Manifest::NewTabButtonParams new_tab_button_params;
    if (random.next_bool()) {
      new_tab_button_params.url = scope.Resolve(
          "new_tab_button_url" + base::NumberToString(random.next_uint()));
    }
    tab_strip.new_tab_button = new_tab_button_params;

    app->SetTabStrip(std::move(tab_strip));
  }

  app->SetAlwaysShowToolbarInFullscreen(random.next_bool());

  app->SetCurrentOsIntegrationStates(
      GenerateRandomWebAppOsIntegrationState(random, *app));

  if (random.next_bool()) {
    bool dev_mode = random.next_bool();

    auto get_location_type = [&seed_str, &random,
                              &dev_mode]() -> IsolatedWebAppStorageLocation {
      if (!dev_mode) {
        return IwaStorageOwnedBundle{
            base32::Base32Encode(base::as_byte_span(seed_str),
                                 base32::Base32EncodePolicy::OMIT_PADDING),
            /*dev_mode=*/false};
      } else {
        constexpr size_t kNumLocationTypes =
            absl::variant_size<IsolatedWebAppStorageLocation::Variant>::value;
        std::array<IsolatedWebAppStorageLocation, kNumLocationTypes>
            location_types = {
                IwaStorageOwnedBundle{
                    base32::Base32Encode(
                        base::as_byte_span(seed_str),
                        base32::Base32EncodePolicy::OMIT_PADDING),
                    /*dev_mode=*/true},
                IwaStorageUnownedBundle{
                    base::FilePath::FromUTF8Unsafe(seed_str)},
                IwaStorageProxy{url::Origin::Create(
                    GURL(base::StrCat({"https://proxy-", seed_str, ".com/"})))},
            };
        return location_types.at(random.next_uint(kNumLocationTypes));
      }
    };

    base::Version version = base::Version({
        random.next_uint(),
        random.next_uint(),
        random.next_uint(),
    });

    auto idb = IsolationData::Builder(get_location_type(), version);
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data =
        CreateIntegrityBlockData(random);
    if (integrity_block_data) {
      idb.SetIntegrityBlockData(std::move(*integrity_block_data));
    }

    if (random.next_bool()) {
      idb.SetControlledFramePartitions({"partition_name"});
    }
    if (random.next_bool()) {
      base::Version pending_version = base::Version({
          random.next_uint(),
          random.next_uint(),
          random.next_uint(),
      });
      IsolationData::PendingUpdateInfo pending_update_info(
          get_location_type(), pending_version, integrity_block_data);
      idb.SetPendingUpdateInfo(std::move(pending_update_info));
    }
    if (dev_mode && random.next_bool()) {
      idb.SetUpdateManifestUrl(GURL("https://update-manifest.com"));
    }
    app->SetIsolationData(std::move(idb).Build());
  }

  app->SetLinkCapturingUserPreference(NEXT_PROTO_ENUM(
      random, proto::LinkCapturingUserPreference, /*skip_zero=*/false));

  app->SetLatestInstallTime(random.next_time());

  if (random.next_bool()) {
    GeneratedIconFix generated_icon_fix;
    generated_icon_fix.set_source(
        NEXT_PROTO_ENUM(random, GeneratedIconFixSource, /*skip_zero=*/true));
    generated_icon_fix.set_window_start_time(random.next_proto_time());
    if (random.next_bool()) {
      generated_icon_fix.set_last_attempt_time(random.next_proto_time());
    }
    generated_icon_fix.set_attempt_count(random.next_uint(100));
    app->SetGeneratedIconFix(generated_icon_fix);
  }

  app->SetSupportedLinksOfferIgnoreCount(random.next_uint());
  app->SetSupportedLinksOfferDismissCount(random.next_uint());

  app->SetIsDiyApp(random.next_bool());
  return app;
}

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback), true /*accept*/,
                                std::move(web_app_info)));
}

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback),
                                false /*accept*/, std::move(web_app_info)));
}

// TODO(b/329703817): Make this smarter by waiting for a specific dialog, and
// then triggering accept on the dialog.
webapps::AppId InstallPwaForCurrentUrl(Browser* browser) {
  // Depending on the installability criteria, different dialogs can be used.
  SetAutoAcceptWebAppDialogForTesting(true, true);
  SetAutoAcceptPWAInstallConfirmationForTesting(true);
  SetAutoAcceptDiyAppsInstallDialogForTesting(true);
  WebAppTestInstallWithOsHooksObserver observer(browser->profile());
  observer.BeginListening();
  CHECK(chrome::ExecuteCommand(browser, IDC_INSTALL_PWA));
  webapps::AppId app_id = observer.Wait();
  SetAutoAcceptPWAInstallConfirmationForTesting(false);
  SetAutoAcceptWebAppDialogForTesting(false, false);
  SetAutoAcceptDiyAppsInstallDialogForTesting(false);
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

void SetWebAppSettingsListPref(Profile* profile, std::string_view pref) {
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  DCHECK(result.has_value()) << result.error().message;
  DCHECK(result->is_list());
  profile->GetPrefs()->Set(prefs::kWebAppSettings, std::move(*result));
}

void AddInstallUrlData(PrefService* pref_service,
                       WebAppSyncBridge* sync_bridge,
                       const webapps::AppId& app_id,
                       const GURL& url,
                       const ExternalInstallSource& source) {
  ScopedRegistryUpdate update = sync_bridge->BeginUpdate();
  WebApp* app_to_update = update->UpdateApp(app_id);
  DCHECK(app_to_update);

  // Adding external app data (source and URL) to web_app DB.
  app_to_update->AddInstallURLToManagementExternalConfigMap(
      ConvertExternalInstallSourceToSource(source), url);
}

void AddInstallUrlAndPlaceholderData(PrefService* pref_service,
                                     WebAppSyncBridge* sync_bridge,
                                     const webapps::AppId& app_id,
                                     const GURL& url,
                                     const ExternalInstallSource& source,
                                     bool is_placeholder) {
  ScopedRegistryUpdate update = sync_bridge->BeginUpdate();
  WebApp* app_to_update = update->UpdateApp(app_id);
  DCHECK(app_to_update);

  // Adding install_url, source and placeholder information to the web_app DB.
  app_to_update->AddExternalSourceInformation(
      ConvertExternalInstallSourceToSource(source), url, is_placeholder);
}

void SynchronizeOsIntegration(Profile* profile,
                              const webapps::AppId& app_id,
                              std::optional<SynchronizeOsOptions> options) {
  base::test::TestFuture<void> sync_future;
  WebAppProvider::GetForTest(profile)->scheduler().SynchronizeOsIntegration(
      app_id, sync_future.GetCallback(), options);
  EXPECT_TRUE(sync_future.Wait());
}

std::vector<web_package::SignedWebBundleSignatureInfo> CreateSignatures() {
  std::vector<web_package::SignedWebBundleSignatureInfo> signatures;

  // EcdsaP256SHA256:
  {
    auto public_key = *web_package::EcdsaP256PublicKey::Create(
        *base::Base64Decode(kEcdsaP256PublicKeyBase64));
    std::vector<uint8_t> data;
    CHECK(base::HexStringToBytes(kEcdsaP256SHA256SignatureHex, &data));
    auto signature = *web_package::EcdsaP256SHA256Signature::Create(data);
    signatures.push_back(
        web_package::SignedWebBundleSignatureInfoEcdsaP256SHA256(
            std::move(public_key), std::move(signature)));
  }

  // Ed25519:
  {
    auto public_key = *web_package::Ed25519PublicKey::Create(
        *base::Base64Decode(kEd25519PublicKeyBase64));
    std::vector<uint8_t> data;
    CHECK(base::HexStringToBytes(kEd25519SignatureHex, &data));
    auto signature = *web_package::Ed25519Signature::Create(data);
    signatures.push_back(web_package::SignedWebBundleSignatureInfoEd25519(
        std::move(public_key), std::move(signature)));
  }

  // Unknown:
  signatures.push_back(web_package::SignedWebBundleSignatureInfoUnknown());
  return signatures;
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
