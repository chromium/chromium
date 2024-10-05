// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/web_applications/web_app.h"

#include <array>
#include <bitset>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/flat_tree.h"
#include "base/containers/to_value_list.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

namespace web_app {

namespace {

// Converts an optional to a string wrapped in a `Value`, or an empty `Value` if
// absent.
template <typename T>
base::Value OptionalToStringValue(const std::optional<T>& optional) {
  if (optional.has_value()) {
    return base::Value(base::ToString(optional.value()));
  }
  return base::Value();
}

// Converts an optional to a debug `Value`, or an empty `Value` if absent.
template <typename T>
base::Value OptionalAsDebugValue(const std::optional<T>& optional) {
  if (optional.has_value()) {
    return optional.value().AsDebugValue();
  }
  return base::Value();
}

std::string ColorToString(std::optional<SkColor> color) {
  return color.has_value() ? color_utils::SkColorToRgbaString(color.value())
                           : "none";
}

std::string ApiApprovalStateToString(ApiApprovalState state) {
  switch (state) {
    case ApiApprovalState::kRequiresPrompt:
      return "kRequiresPrompt";
    case ApiApprovalState::kAllowed:
      return "kAllowed";
    case ApiApprovalState::kDisallowed:
      return "kDisallowed";
  }
}

std::string GetRunOnOsLoginMode(const proto::RunOnOsLoginMode& mode) {
  switch (mode) {
    case proto::RunOnOsLoginMode::RUN_ON_OS_LOGIN_MODE_UNSPECIFIED:
      return "unspecified";
    case proto::RunOnOsLoginMode::NOT_RUN:
      return "not_run";
    case proto::RunOnOsLoginMode::WINDOWED:
      return "windowed";
    case proto::RunOnOsLoginMode::MINIMIZED:
      return "minimized";
  }
}

base::Value OsStatesDebugValue(
    const proto::WebAppOsIntegrationState& current_states) {
  base::Value::Dict debug_dict;

  if (current_states.has_shortcut()) {
    base::Value::Dict shortcut_data;
    shortcut_data.Set("title", current_states.shortcut().title());
    shortcut_data.Set("description", current_states.shortcut().description());
    base::Value::Dict icon_data;
    for (const auto& data : current_states.shortcut().icon_data_any()) {
      icon_data.Set(base::NumberToString(data.icon_size()),
                    base::ToString(syncer::ProtoTimeToTime(data.timestamp())));
    }
    shortcut_data.Set("icon_size_to_timestamp_map",
                      base::Value(std::move(icon_data)));
    debug_dict.Set("shortcut_descriptions",
                   base::Value(std::move(shortcut_data)));
  }

  if (current_states.has_protocols_handled()) {
    base::Value::Dict protocol_data;
    for (const auto& data : current_states.protocols_handled().protocols()) {
      protocol_data.Set(data.protocol(), data.url());
    }
    debug_dict.Set("protocols_handled", base::Value(std::move(protocol_data)));
  }

  if (current_states.has_run_on_os_login() &&
      current_states.run_on_os_login().has_run_on_os_login_mode()) {
    debug_dict.Set(
        "run_on_os_login",
        GetRunOnOsLoginMode(
            current_states.run_on_os_login().run_on_os_login_mode()));
  }

  if (current_states.has_uninstall_registration()) {
    base::Value::Dict state;
    proto::OsUninstallRegistration os_uninstall =
        current_states.uninstall_registration();
    if (os_uninstall.has_registered_with_os()) {
      state.Set("registered_with_os", os_uninstall.registered_with_os());
    }
    if (os_uninstall.has_display_name()) {
      state.Set("display_name", os_uninstall.display_name());
    }
    debug_dict.Set("uninstall_registration", std::move(state));
  }

  if (current_states.has_shortcut_menus()) {
    base::Value::List shortcut_menus_list;
    for (const auto& shortcut_menu :
         current_states.shortcut_menus().shortcut_menu_info()) {
      base::Value::Dict icon_data_any_dict;
      base::Value::Dict icon_data_maskable_dict;
      base::Value::Dict icon_data_monochrome_dict;
      for (const auto& icon_data_any : shortcut_menu.icon_data_any()) {
        icon_data_any_dict.Set(
            base::NumberToString(icon_data_any.icon_size()),
            base::ToString(syncer::ProtoTimeToTime(icon_data_any.timestamp())));
      }
      for (const auto& icon_data_maskable : shortcut_menu.icon_data_any()) {
        icon_data_maskable_dict.Set(
            base::NumberToString(icon_data_maskable.icon_size()),
            base::ToString(
                syncer::ProtoTimeToTime(icon_data_maskable.timestamp())));
      }
      for (const auto& icon_data_monochrome : shortcut_menu.icon_data_any()) {
        icon_data_monochrome_dict.Set(
            base::NumberToString(icon_data_monochrome.icon_size()),
            base::ToString(
                syncer::ProtoTimeToTime(icon_data_monochrome.timestamp())));
      }
      base::Value::Dict shortcut_menu_dict;
      shortcut_menu_dict.Set("shortcut_name", shortcut_menu.shortcut_name());
      shortcut_menu_dict.Set("shortcut_launch_url",
                             shortcut_menu.shortcut_launch_url());
      shortcut_menu_dict.Set("icon_data_any",
                             base::Value(std::move(icon_data_any_dict)));
      shortcut_menu_dict.Set("icon_data_maskable",
                             base::Value(std::move(icon_data_maskable_dict)));
      shortcut_menu_dict.Set("icon_data_monochrome",
                             base::Value(std::move(icon_data_monochrome_dict)));
      shortcut_menus_list.Append(std::move(shortcut_menu_dict));
    }
    debug_dict.Set("shortcut_menus",
                   base::Value(std::move(shortcut_menus_list)));
  }

  if (current_states.has_file_handling()) {
    base::Value::List file_handlers_list;
    for (const auto& file_handler :
         current_states.file_handling().file_handlers()) {
      base::Value::Dict file_handler_dict;
      file_handler_dict.Set("action", base::Value(file_handler.action()));
      file_handler_dict.Set("display_name", file_handler.display_name());
      base::Value::List accept_list;
      for (const auto& accept : file_handler.accept()) {
        base::Value::Dict accept_dict;
        accept_dict.Set("mimetype", accept.mimetype());
        base::Value::List file_extensions_list;
        for (const auto& file_extension : accept.file_extensions()) {
          file_extensions_list.Append(file_extension);
        }
        accept_dict.Set("file_extensions", std::move(file_extensions_list));
        accept_list.Append(std::move(accept_dict));
      }
      file_handler_dict.Set("accept", std::move(accept_list));
      file_handlers_list.Append(std::move(file_handler_dict));
    }
    debug_dict.Set("file_handling", std::move(file_handlers_list));
  }

  return base::Value(std::move(debug_dict));
}

base::Value::Dict ImageResourceDebugDict(
    const blink::Manifest::ImageResource& icon) {
  const char* const kPurposeStrings[] = {"Any", "Monochrome", "Maskable"};

  base::Value::Dict root;
  root.Set("src", icon.src.spec());
  root.Set("type", icon.type);

  base::Value::List sizes_json;
  for (const auto& size : icon.sizes) {
    std::string size_formatted = base::NumberToString(size.width()) + "x" +
                                 base::NumberToString(size.height());
    sizes_json.Append(base::Value(size_formatted));
  }
  root.Set("sizes", std::move(sizes_json));

  base::Value::List purpose_json;
  for (const auto& purpose : icon.purpose) {
    purpose_json.Append(kPurposeStrings[static_cast<int>(purpose)]);
  }
  root.Set("purpose", std::move(purpose_json));
  return root;
}

base::Value::Dict UrlPatternDebugValue(const blink::SafeUrlPattern& pattern) {
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pathname(pattern.pathname, options, "[^/]+?");

  base::Value::Dict pattern_dict;
  pattern_dict.Set("pathname", pathname.GeneratePatternString());
  return pattern_dict;
}

base::Value OptTabStripToDebugValue(
    std::optional<blink::Manifest::TabStrip> tab_strip) {
  if (!tab_strip.has_value()) {
    return base::Value();
  }

  base::Value::Dict result;

  base::Value::Dict new_tab_button_json;
  new_tab_button_json.Set(
      "url", base::ToString(tab_strip->new_tab_button.url.value_or(GURL(""))));
  result.Set("new_tab_button", std::move(new_tab_button_json));

  if (absl::holds_alternative<TabStrip::Visibility>(tab_strip->home_tab)) {
    result.Set(
        "home_tab",
        base::ToString(absl::get<TabStrip::Visibility>(tab_strip->home_tab)));
  } else {
    base::Value::Dict home_tab_json;
    const blink::Manifest::HomeTabParams& home_tab_params =
        absl::get<blink::Manifest::HomeTabParams>(tab_strip->home_tab);

    base::Value::List icons_json;
    std::optional<std::vector<blink::Manifest::ImageResource>> icons =
        home_tab_params.icons;

    for (auto& icon : *icons) {
      icons_json.Append(ImageResourceDebugDict(icon));
    }

    base::Value::List scope_patterns_json;
    const std::vector<blink::SafeUrlPattern>& scope_patterns =
        home_tab_params.scope_patterns;

    for (const auto& scope_pattern : scope_patterns) {
      scope_patterns_json.Append(UrlPatternDebugValue(scope_pattern));
    }

    home_tab_json.Set("icons", std::move(icons_json));
    home_tab_json.Set("scope_patterns", std::move(scope_patterns_json));
    result.Set("home_tab", std::move(home_tab_json));
  }
  return base::Value(std::move(result));
}

}  // namespace

WebApp::WebApp(const webapps::AppId& app_id)
    : app_id_(app_id),
      chromeos_data_(IsChromeOsDataMandatory()
                         ? std::make_optional<WebAppChromeOsData>()
                         : std::nullopt) {}

WebApp::~WebApp() = default;

WebApp::WebApp(const WebApp& web_app) = default;

WebApp& WebApp::operator=(WebApp&& web_app) = default;

const SortedSizesPx& WebApp::downloaded_icon_sizes(IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::ANY:
      return downloaded_icon_sizes_any_;
    case IconPurpose::MONOCHROME:
      return downloaded_icon_sizes_monochrome_;
    case IconPurpose::MASKABLE:
      return downloaded_icon_sizes_maskable_;
  }
}

webapps::ManifestId WebApp::manifest_id() const {
  // Almost all production use-cases should have the manifest_id set, but in
  // some test it is not. If the manifest id is not set, then fall back to the
  // start_url, as per the algorithm in
  // https://www.w3.org/TR/appmanifest/#id-member.
  if (manifest_id_.is_empty()) {
    CHECK_IS_TEST();
    // This is why the function must return a value instead of a const ref, as
    // this object would be temporary.
    return GenerateManifestIdFromStartUrlOnly(start_url_);
  }
  return manifest_id_;
}

void WebApp::AddSource(WebAppManagement::Type source) {
  sources_.Put(source);
}

void WebApp::RemoveSource(WebAppManagement::Type source) {
  sources_.Remove(source);
  management_to_external_config_map_.erase(source);
}

bool WebApp::HasAnySources() const {
  return !sources_.empty();
}

bool WebApp::HasOnlySource(WebAppManagement::Type source) const {
  WebAppManagementTypes specified_sources;
  specified_sources.Put(source);
  return HasAnySpecifiedSourcesAndNoOtherSources(sources_, specified_sources);
}

WebAppManagementTypes WebApp::GetSources() const {
  return sources_;
}

bool WebApp::IsSynced() const {
  return sources_.Has(WebAppManagement::kSync);
}

bool WebApp::IsPreinstalledApp() const {
  return sources_.Has(WebAppManagement::kDefault);
}

bool WebApp::IsPolicyInstalledApp() const {
  return sources_.Has(WebAppManagement::kPolicy);
}

bool WebApp::IsIwaPolicyInstalledApp() const {
  return sources_.Has(WebAppManagement::kIwaPolicy);
}

bool WebApp::IsIwaShimlessRmaApp() const {
  return sources_.Has(WebAppManagement::kIwaShimlessRma);
}

bool WebApp::IsSystemApp() const {
  return sources_.Has(WebAppManagement::kSystem);
}

bool WebApp::IsWebAppStoreInstalledApp() const {
  return sources_.Has(WebAppManagement::kWebAppStore);
}

bool WebApp::IsSubAppInstalledApp() const {
  return sources_.Has(WebAppManagement::kSubApp);
}

bool WebApp::IsKioskInstalledApp() const {
  return sources_.Has(WebAppManagement::kKiosk);
}

bool WebApp::CanUserUninstallWebApp() const {
  return web_app::CanUserUninstallWebApp(app_id_, sources_);
}

bool WebApp::WasInstalledByUser() const {
  return sources_.Has(WebAppManagement::kSync) ||
         sources_.Has(WebAppManagement::kUserInstalled) ||
         sources_.Has(WebAppManagement::kWebAppStore) ||
         sources_.Has(WebAppManagement::kOneDriveIntegration) ||
         sources_.Has(WebAppManagement::kIwaUserInstalled);
}

WebAppManagement::Type WebApp::GetHighestPrioritySource() const {
  // `WebAppManagementTypes` is iterated in order of priority.
  // Top priority sources are iterated first.
  for (WebAppManagement::Type source : WebAppManagementTypes::All()) {
    if (sources_.Has(source)) {
      return source;
    }
  }

  DUMP_WILL_BE_NOTREACHED();
  return WebAppManagement::kMaxValue;
}

void WebApp::SetName(const std::string& name) {
  name_ = name;
}

void WebApp::SetDescription(const std::string& description) {
  description_ = description;
}

void WebApp::SetStartUrl(const GURL& start_url) {
  CHECK(start_url.is_valid());
  if (manifest_id_.is_empty()) {
    manifest_id_ = GenerateManifestIdFromStartUrlOnly(start_url);
  }
  CHECK(url::Origin::Create(manifest_id())
            .IsSameOriginWith(url::Origin::Create(start_url)))
      << manifest_id().spec() << " " << start_url.spec();
  start_url_ = start_url;

  // Ensure sync proto is initialized and remains consistent. Logic in
  // `SetSyncProto` will populate an unset `start_url` on the proto.
  sync_proto_.clear_start_url();
  SetSyncProto(sync_proto_);
}

void WebApp::SetScope(const GURL& scope) {
  // TODO(crbug.com/339718933): Remove this after shortcut apps are fully
  // removed.
  if (scope.is_empty()) {
    scope_ = scope;
    return;
  }
  CHECK(scope.is_valid());
  // Ensure that the scope can never include queries or fragments, as per spec.
  GURL::Replacements scope_replacements;
  scope_replacements.ClearRef();
  scope_replacements.ClearQuery();
  scope_ = scope.ReplaceComponents(scope_replacements);
}

void WebApp::SetThemeColor(std::optional<SkColor> theme_color) {
  theme_color_ = theme_color;
}

void WebApp::SetDarkModeThemeColor(
    std::optional<SkColor> dark_mode_theme_color) {
  dark_mode_theme_color_ = dark_mode_theme_color;
}

void WebApp::SetBackgroundColor(std::optional<SkColor> background_color) {
  background_color_ = background_color;
}

void WebApp::SetDarkModeBackgroundColor(
    std::optional<SkColor> dark_mode_background_color) {
  dark_mode_background_color_ = dark_mode_background_color;
}

void WebApp::SetDisplayMode(DisplayMode display_mode) {
  DCHECK_NE(DisplayMode::kUndefined, display_mode);
  display_mode_ = display_mode;
}

void WebApp::SetUserDisplayMode(mojom::UserDisplayMode user_display_mode) {
  sync_pb::WebAppSpecifics_UserDisplayMode sync_udm =
      ToWebAppSpecificsUserDisplayMode(user_display_mode);
  SetPlatformSpecificUserDisplayMode(sync_udm, &sync_proto_);
}

void WebApp::SetDisplayModeOverride(
    std::vector<DisplayMode> display_mode_override) {
  display_mode_override_ = std::move(display_mode_override);
}

void WebApp::SetWebAppChromeOsData(
    std::optional<WebAppChromeOsData> chromeos_data) {
  chromeos_data_ = std::move(chromeos_data);
}

void WebApp::SetInstallState(proto::InstallState install_state) {
  install_state_ = install_state;
}

void WebApp::SetIsFromSyncAndPendingInstallation(
    bool is_from_sync_and_pending_installation) {
  is_from_sync_and_pending_installation_ =
      is_from_sync_and_pending_installation;
}

void WebApp::SetIsUninstalling(bool is_uninstalling) {
  is_uninstalling_ = is_uninstalling;
}

void WebApp::SetManifestIcons(std::vector<apps::IconInfo> manifest_icons) {
  manifest_icons_ = std::move(manifest_icons);
}

void WebApp::SetDownloadedIconSizes(IconPurpose purpose, SortedSizesPx sizes) {
  switch (purpose) {
    case IconPurpose::ANY:
      downloaded_icon_sizes_any_ = std::move(sizes);
      break;
    case IconPurpose::MONOCHROME:
      downloaded_icon_sizes_monochrome_ = std::move(sizes);
      break;
    case IconPurpose::MASKABLE:
      downloaded_icon_sizes_maskable_ = std::move(sizes);
      break;
  }
}

void WebApp::SetIsGeneratedIcon(bool is_generated_icon) {
  is_generated_icon_ = is_generated_icon;
}

void WebApp::SetFileHandlers(apps::FileHandlers file_handlers) {
  file_handlers_ = std::move(file_handlers);
}

void WebApp::SetFileHandlerApprovalState(ApiApprovalState approval_state) {
  file_handler_approval_state_ = approval_state;
}

void WebApp::SetShareTarget(std::optional<apps::ShareTarget> share_target) {
  share_target_ = std::move(share_target);
}

void WebApp::SetAdditionalSearchTerms(
    std::vector<std::string> additional_search_terms) {
  additional_search_terms_ = std::move(additional_search_terms);
}

void WebApp::SetProtocolHandlers(
    std::vector<apps::ProtocolHandlerInfo> handlers) {
  protocol_handlers_ = std::move(handlers);
}

void WebApp::SetAllowedLaunchProtocols(
    base::flat_set<std::string> allowed_launch_protocols) {
  allowed_launch_protocols_ = std::move(allowed_launch_protocols);
}

void WebApp::SetDisallowedLaunchProtocols(
    base::flat_set<std::string> disallowed_launch_protocols) {
  disallowed_launch_protocols_ = std::move(disallowed_launch_protocols);
}

void WebApp::SetUrlHandlers(apps::UrlHandlers url_handlers) {
  url_handlers_ = std::move(url_handlers);
}

void WebApp::SetScopeExtensions(
    base::flat_set<ScopeExtensionInfo> scope_extensions) {
  scope_extensions_ = std::move(scope_extensions);
}

void WebApp::SetValidatedScopeExtensions(
    base::flat_set<ScopeExtensionInfo> validated_scope_extensions) {
  validated_scope_extensions_ = std::move(validated_scope_extensions);
}

void WebApp::SetLockScreenStartUrl(const GURL& lock_screen_start_url) {
  DCHECK(lock_screen_start_url.is_empty() || lock_screen_start_url.is_valid());
  lock_screen_start_url_ = lock_screen_start_url;
}

void WebApp::SetNoteTakingNewNoteUrl(const GURL& note_taking_new_note_url) {
  DCHECK(note_taking_new_note_url.is_empty() ||
         note_taking_new_note_url.is_valid());
  note_taking_new_note_url_ = note_taking_new_note_url;
}

void WebApp::SetShortcutsMenuInfo(
    std::vector<WebAppShortcutsMenuItemInfo> shortcuts_menu_item_infos) {
  shortcuts_menu_item_infos_ = std::move(shortcuts_menu_item_infos);
}

void WebApp::SetLastBadgingTime(const base::Time& time) {
  last_badging_time_ = time;
}

void WebApp::SetLastLaunchTime(const base::Time& time) {
  last_launch_time_ = time;
}

void WebApp::SetFirstInstallTime(const base::Time& time) {
  first_install_time_ = time;
}

void WebApp::SetManifestUpdateTime(const base::Time& time) {
  manifest_update_time_ = time;
}

void WebApp::SetRunOnOsLoginMode(RunOnOsLoginMode mode) {
  run_on_os_login_mode_ = mode;
}

void WebApp::SetSyncProto(sync_pb::WebAppSpecifics sync_proto) {
  // Populate sync_proto's start_url from this WebApp if missing.
  if (!start_url().is_empty()) {
    CHECK(start_url().is_valid(), base::NotFatalUntil::M126);
    // Note: sync data may have a start_url that does not match the `WebApp`
    // start_url, but it does not update the app (matching pre-M125 behaviour).
    if (!sync_proto.has_start_url()) {
      sync_proto.set_start_url(start_url().spec());
    }
  }

  // Sync data must never be set on an app with mismatching manifest_id.
  CHECK(manifest_id().is_valid(), base::NotFatalUntil::M126);
  std::string relative_manifest_id_path = RelativeManifestIdPath(manifest_id());
  if (sync_proto.has_relative_manifest_id()) {
    CHECK_EQ(sync_proto.relative_manifest_id(), relative_manifest_id_path,
             base::NotFatalUntil::M127);
  } else {
    sync_proto.set_relative_manifest_id(relative_manifest_id_path);
  }

  // Clear any invalid less-important fields.
  if (sync_proto.has_scope() && !GURL(sync_proto.scope()).is_valid()) {
    DLOG(ERROR) << "SetSyncProto: scope has invalid url: "
                << sync_proto.scope();
    sync_proto.clear_scope();
  }
  if (!ParseAppIconInfos("SetSyncProto", sync_proto.icon_infos()).has_value()) {
    sync_proto.clear_icon_infos();
  }
  if (sync_proto.has_user_launch_ordinal() &&
      !syncer::StringOrdinal(sync_proto.user_launch_ordinal()).IsValid()) {
    sync_proto.clear_user_launch_ordinal();
  }
  if (sync_proto.has_user_page_ordinal() &&
      !syncer::StringOrdinal(sync_proto.user_page_ordinal()).IsValid()) {
    sync_proto.clear_user_page_ordinal();
  }

  sync_proto_ = std::move(sync_proto);
}

void WebApp::SetCaptureLinks(blink::mojom::CaptureLinks capture_links) {
  capture_links_ = capture_links;
}

void WebApp::SetLaunchQueryParams(
    std::optional<std::string> launch_query_params) {
  launch_query_params_ = std::move(launch_query_params);
}

void WebApp::SetManifestUrl(const GURL& manifest_url) {
  manifest_url_ = manifest_url;
}

void WebApp::SetManifestId(const webapps::ManifestId& manifest_id) {
  CHECK(manifest_id.is_valid());
  CHECK(start_url_.is_empty() ||
        url::Origin::Create(start_url_)
            .IsSameOriginWith(url::Origin::Create(manifest_id)))
      << start_url_.spec() << " vs " << manifest_id.spec();
  CHECK(!manifest_id.has_ref(), base::NotFatalUntil::M127);
  manifest_id_ = manifest_id;

  // Ensure sync proto is initialized and remains consistent. Logic in
  // `SetSyncProto` will populate an unset `relative_manifest_id` on the proto.
  sync_proto_.clear_relative_manifest_id();
  SetSyncProto(sync_proto_);
}

void WebApp::SetWindowControlsOverlayEnabled(bool enabled) {
  window_controls_overlay_enabled_ = enabled;
}

void WebApp::SetLaunchHandler(std::optional<LaunchHandler> launch_handler) {
  launch_handler_ = std::move(launch_handler);
}

void WebApp::SetParentAppId(
    const std::optional<webapps::AppId>& parent_app_id) {
  parent_app_id_ = parent_app_id;
}

void WebApp::SetPermissionsPolicy(
    blink::ParsedPermissionsPolicy permissions_policy) {
  permissions_policy_ = std::move(permissions_policy);
}

void WebApp::SetLatestInstallSource(
    std::optional<webapps::WebappInstallSource> latest_install_source) {
  latest_install_source_ = latest_install_source;
}

void WebApp::SetAppSizeInBytes(std::optional<int64_t> app_size_in_bytes) {
  app_size_in_bytes_ = app_size_in_bytes;
}

void WebApp::SetDataSizeInBytes(std::optional<int64_t> data_size_in_bytes) {
  data_size_in_bytes_ = data_size_in_bytes;
}

void WebApp::SetWebAppManagementExternalConfigMap(
    ExternalConfigMap management_to_external_config_map) {
  management_to_external_config_map_ =
      std::move(management_to_external_config_map);
}

void WebApp::SetTabStrip(std::optional<blink::Manifest::TabStrip> tab_strip) {
  tab_strip_ = std::move(tab_strip);
}

void WebApp::SetCurrentOsIntegrationStates(
    proto::WebAppOsIntegrationState current_os_integration_states) {
  current_os_integration_states_ = std::move(current_os_integration_states);
}

void WebApp::SetIsolationData(IsolationData isolation_data) {
  isolation_data_ = isolation_data;
}

void WebApp::SetLinkCapturingUserPreference(
    proto::LinkCapturingUserPreference user_link_capturing_preference) {
  user_link_capturing_preference_ = user_link_capturing_preference;
}

void WebApp::SetSupportedLinksOfferIgnoreCount(int ignore_count) {
  supported_links_offer_ignore_count_ = ignore_count;
}

void WebApp::SetSupportedLinksOfferDismissCount(int dismiss_count) {
  supported_links_offer_dismiss_count_ = dismiss_count;
}

void WebApp::SetIsDiyApp(bool is_diy_app) {
  is_diy_app_ = is_diy_app;
}

void WebApp::AddPlaceholderInfoToManagementExternalConfigMap(
    WebAppManagement::Type type,
    bool is_placeholder) {
  DCHECK_NE(type, WebAppManagement::Type::kSync);
  DCHECK_NE(type, WebAppManagement::Type::kUserInstalled);
  CHECK(!WebAppManagement::IsIwaType(type)) << type;
  management_to_external_config_map_[type].is_placeholder = is_placeholder;
}

void WebApp::AddInstallURLToManagementExternalConfigMap(
    WebAppManagement::Type type,
    GURL install_url) {
  DCHECK_NE(type, WebAppManagement::Type::kSync);
  DCHECK_NE(type, WebAppManagement::Type::kUserInstalled);
  CHECK(!WebAppManagement::IsIwaType(type)) << type;
  DCHECK(install_url.is_valid());
  management_to_external_config_map_[type].install_urls.emplace(
      std::move(install_url));
}

void WebApp::AddPolicyIdToManagementExternalConfigMap(
    WebAppManagement::Type type,
    std::string policy_id) {
  DCHECK_NE(type, WebAppManagement::Type::kSync);
  DCHECK_NE(type, WebAppManagement::Type::kUserInstalled);
  CHECK(!WebAppManagement::IsIwaType(type)) << type;
  DCHECK(!policy_id.empty());
  management_to_external_config_map_[type].additional_policy_ids.emplace(
      std::move(policy_id));
}

void WebApp::AddExternalSourceInformation(WebAppManagement::Type type,
                                          GURL install_url,
                                          bool is_placeholder) {
  AddInstallURLToManagementExternalConfigMap(type, std::move(install_url));
  AddPlaceholderInfoToManagementExternalConfigMap(type, is_placeholder);
}

bool WebApp::RemoveInstallUrlForSource(WebAppManagement::Type type,
                                       const GURL& install_url) {
  if (!management_to_external_config_map_.count(type))
    return false;

  bool removed =
      management_to_external_config_map_[type].install_urls.erase(install_url);
  if (management_to_external_config_map_[type].install_urls.empty()) {
    management_to_external_config_map_.erase(type);
  }
  return removed;
}

void WebApp::SetAlwaysShowToolbarInFullscreen(bool show) {
  always_show_toolbar_in_fullscreen_ = show;
}

void WebApp::SetLatestInstallTime(const base::Time& latest_install_time) {
  latest_install_time_ = latest_install_time;
}

void WebApp::SetGeneratedIconFix(
    std::optional<GeneratedIconFix> generated_icon_fix) {
  CHECK(!generated_icon_fix.has_value() ||
        generated_icon_fix_util::IsValid(*generated_icon_fix));
  generated_icon_fix_ = generated_icon_fix;
}

WebApp::ClientData::ClientData() = default;

WebApp::ClientData::~ClientData() = default;

WebApp::ClientData::ClientData(const ClientData& client_data) = default;

base::Value WebApp::ClientData::AsDebugValue() const {
  base::Value::Dict root;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  root.Set("system_web_app_data", OptionalAsDebugValue(system_web_app_data));
#endif
  return base::Value(std::move(root));
}

WebApp::ExternalManagementConfig::ExternalManagementConfig() = default;
WebApp::ExternalManagementConfig::ExternalManagementConfig(
    bool is_placeholder,
    const base::flat_set<GURL>& install_urls,
    const base::flat_set<std::string>& additional_policy_ids)
    : is_placeholder(is_placeholder),
      install_urls(install_urls),
      additional_policy_ids(additional_policy_ids) {}

WebApp::ExternalManagementConfig::~ExternalManagementConfig() = default;

WebApp::ExternalManagementConfig::ExternalManagementConfig(
    const ExternalManagementConfig& external_management_config) = default;
WebApp::ExternalManagementConfig& WebApp::ExternalManagementConfig::operator=(
    const ExternalManagementConfig& external_management_config) = default;
WebApp::ExternalManagementConfig& WebApp::ExternalManagementConfig::operator=(
    ExternalManagementConfig&& external_management_config) = default;

base::Value::Dict WebApp::ExternalManagementConfig::AsDebugValue() const {
  base::Value::Dict root;
  base::Value::List urls;
  for (const auto& install_url : install_urls) {
    urls.Append(install_url.spec());
  }
  base::Value::List policy_ids;
  for (const auto& policy_id : additional_policy_ids) {
    policy_ids.Append(policy_id);
  }
  root.Set("install_urls", std::move(urls));
  root.Set("additional_policy_ids", std::move(policy_ids));
  root.Set("is_placeholder", is_placeholder);
  return root;
}

const std::optional<GeneratedIconFix>& WebApp::generated_icon_fix() const {
  CHECK(!generated_icon_fix_.has_value() ||
        generated_icon_fix_util::IsValid(generated_icon_fix_.value()));
  return generated_icon_fix_;
}

bool WebApp::operator==(const WebApp& other) const {
  auto AsTuple = [](const WebApp& app) {
    // Keep in order declared in web_app.h.
    return std::tie(
        // Disable clang-format so diffs are clearer when fields are added.
        // clang-format off
        app.app_id_,
        app.sources_,
        app.name_,
        app.description_,
        app.start_url_,
        app.launch_query_params_,
        app.scope_,
        app.theme_color_,
        app.dark_mode_theme_color_,
        app.background_color_,
        app.dark_mode_background_color_,
        app.display_mode_,
        app.display_mode_override_,
        app.chromeos_data_,
        app.install_state_,
        app.is_from_sync_and_pending_installation_,
        app.is_uninstalling_,
        app.manifest_icons_,
        app.downloaded_icon_sizes_any_,
        app.downloaded_icon_sizes_monochrome_,
        app.downloaded_icon_sizes_maskable_,
        app.is_generated_icon_,
        app.shortcuts_menu_item_infos_,
        app.file_handlers_,
        app.share_target_,
        app.additional_search_terms_,
        app.protocol_handlers_,
        app.allowed_launch_protocols_,
        app.disallowed_launch_protocols_,
        app.url_handlers_,
        app.scope_extensions_,
        app.validated_scope_extensions_,
        app.lock_screen_start_url_,
        app.note_taking_new_note_url_,
        app.last_badging_time_,
        app.last_launch_time_,
        app.first_install_time_,
        app.manifest_update_time_,
        app.run_on_os_login_mode_,
        app.sync_proto_,
        app.capture_links_,
        app.manifest_url_,
        app.manifest_id_,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        app.client_data_.system_web_app_data,
#endif
        app.file_handler_approval_state_,
        app.window_controls_overlay_enabled_,
        app.launch_handler_,
        app.parent_app_id_,
        app.permissions_policy_,
        app.latest_install_source_,
        app.app_size_in_bytes_,
        app.data_size_in_bytes_,
        app.management_to_external_config_map_,
        app.tab_strip_,
        app.always_show_toolbar_in_fullscreen_,
        app.current_os_integration_states_,
        app.isolation_data_,
        app.user_link_capturing_preference_,
        app.latest_install_time_,
        app.generated_icon_fix_,
        app.supported_links_offer_ignore_count_,
        app.supported_links_offer_dismiss_count_,
        app.is_diy_app_
        // clang-format on
    );
  };
  return AsTuple(*this) == AsTuple(other);
}

bool WebApp::operator!=(const WebApp& other) const {
  return !(*this == other);
}

base::Value WebApp::AsDebugValueWithOnlyPlatformAgnosticFields() const {
  base::Value::Dict root;

  auto ConvertList = [](const auto& list) {
    base::Value::List list_json;
    for (const auto& item : list)
      list_json.Append(item);
    return list_json;
  };

  auto ConvertDebugValueList = [](const auto& list) {
    base::Value::List list_json;
    for (const auto& item : list)
      list_json.Append(item.AsDebugValue());
    return list_json;
  };

  auto ConvertOptional = [](const auto& value) {
    return value ? base::Value(*value) : base::Value();
  };

  // Prefix with a ! so these fields appear at the top when serialized.
  root.Set("!app_id", app_id_);

  root.Set("!name", name_);

  root.Set("additional_search_terms", ConvertList(additional_search_terms_));

  root.Set("app_service_icon_url",
           base::StrCat({"chrome://app-icon/", app_id_, "/32"}));

  root.Set("app_size_in_bytes", OptionalToStringValue(app_size_in_bytes_));

  root.Set("allowed_launch_protocols", ConvertList(allowed_launch_protocols_));

  root.Set("background_color", ColorToString(background_color_));

  root.Set("capture_links", base::ToString(capture_links_));

  root.Set("data_size_in_bytes", OptionalToStringValue(data_size_in_bytes_));

  root.Set("dark_mode_background_color",
           ColorToString(dark_mode_background_color_));

  root.Set("dark_mode_theme_color", ColorToString(dark_mode_theme_color_));

  root.Set("disallowed_launch_protocols",
           ConvertList(disallowed_launch_protocols_));

  root.Set("description", description_);

  root.Set("display_mode", blink::DisplayModeToString(display_mode_));

  base::Value::List display_override;
  for (const DisplayMode& mode : display_mode_override_)
    display_override.Append(blink::DisplayModeToString(mode));
  root.Set("display_override", std::move(display_override));

  base::Value::Dict downloaded_icon_sizes_json;
  for (IconPurpose purpose : kIconPurposes) {
    downloaded_icon_sizes_json.Set(base::ToString(purpose),
                                   ConvertList(downloaded_icon_sizes(purpose)));
  }
  root.Set("downloaded_icon_sizes", std::move(downloaded_icon_sizes_json));

  root.Set("file_handler_approval_state",
           ApiApprovalStateToString(file_handler_approval_state_));

  root.Set("file_handlers", ConvertDebugValueList(file_handlers_));

  root.Set("manifest_icons", ConvertDebugValueList(manifest_icons_));

  root.Set("latest_install_source",
           OptionalToStringValue(latest_install_source_));

  base::Value::Dict external_map;
  for (auto it : management_to_external_config_map_) {
    external_map.Set(base::ToString(it.first), it.second.AsDebugValue());
  }

  root.Set("management_type_to_external_configuration_map",
           std::move(external_map));

  root.Set("first_install_time", base::ToString(first_install_time_));

  root.Set("is_generated_icon", is_generated_icon_);

  root.Set("is_from_sync_and_pending_installation",
           is_from_sync_and_pending_installation_);

  root.Set("install_state", base::ToString(install_state_));

  root.Set("is_uninstalling", is_uninstalling_);

  root.Set("last_badging_time", base::ToString(last_badging_time_));

  root.Set("last_launch_time", base::ToString(last_launch_time_));

  if (launch_handler_) {
    base::Value::Dict launch_handler_json;
    launch_handler_json.Set("client_mode",
                            base::ToString(launch_handler_->client_mode));
    root.Set("launch_handler", std::move(launch_handler_json));
  } else {
    root.Set("launch_handler", base::Value());
  }

  root.Set("launch_query_params", ConvertOptional(launch_query_params_));

  root.Set("manifest_update_time", base::ToString(manifest_update_time_));

  root.Set("manifest_url", base::ToString(manifest_url_));

  root.Set("lock_screen_start_url", base::ToString(lock_screen_start_url_));

  root.Set("note_taking_new_note_url",
           base::ToString(note_taking_new_note_url_));

  root.Set("parent_app_id", OptionalToStringValue(parent_app_id_));

  if (!permissions_policy_.empty()) {
    base::Value::List policy_list;
    const auto& feature_to_name_map =
        blink::GetPermissionsPolicyFeatureToNameMap();
    for (const auto& decl : permissions_policy_) {
      base::Value::Dict json_decl;
      const auto& feature_name = feature_to_name_map.find(decl.feature);
      if (feature_name == feature_to_name_map.end()) {
        continue;
      }
      json_decl.Set("feature", feature_name->second);
      base::Value::List allowlist_json;
      for (const auto& allowlist_item : GetSerializedAllowedOrigins(decl)) {
        allowlist_json.Append(allowlist_item);
      }
      json_decl.Set("allowed_origins", std::move(allowlist_json));
      json_decl.Set("matches_all_origins", decl.matches_all_origins);
      json_decl.Set("matches_opaque_src", decl.matches_opaque_src);
      policy_list.Append(std::move(json_decl));
    }
    root.Set("permissions_policy", std::move(policy_list));
  }

  root.Set("protocol_handlers", ConvertDebugValueList(protocol_handlers_));

  root.Set("run_on_os_login_mode", base::ToString(run_on_os_login_mode_));

  root.Set("scope", base::ToString(scope_));

  root.Set("share_target", OptionalAsDebugValue(share_target_));

  root.Set("shortcuts_menu_item_infos",
           ConvertDebugValueList(shortcuts_menu_item_infos_));

  base::Value::List sources;
  for (WebAppManagement::Type source : WebAppManagementTypes::All()) {
    if (sources_.Has(source)) {
      sources.Append(base::ToString(source));
    }
  }
  root.Set("sources", std::move(sources));

  root.Set("start_url", base::ToString(start_url_));

  root.Set("sync_proto", syncer::WebAppSpecificsToValue(sync_proto_));

  root.Set("theme_color", ColorToString(theme_color_));

  root.Set("manifest_id", manifest_id_.spec());

  root.Set("url_handlers", ConvertDebugValueList(url_handlers_));

  root.Set("scope_extensions", ConvertDebugValueList(scope_extensions_));

  root.Set("scope_extensions_validated",
           ConvertDebugValueList(validated_scope_extensions_));

  root.Set("window_controls_overlay_enabled", window_controls_overlay_enabled_);

  root.Set("tab_strip", OptTabStripToDebugValue(tab_strip_));

  root.Set("always_show_toolbar_in_fullscreen",
           always_show_toolbar_in_fullscreen_);

  root.Set("current_os_integration_states",
           OsStatesDebugValue(current_os_integration_states_));

  root.Set("isolation_data", OptionalAsDebugValue(isolation_data_));

  root.Set("user_link_capturing_preference",
           base::ToString(user_link_capturing_preference_));

  root.Set("latest_install_time", base::ToString(latest_install_time_));

  root.Set("generated_icon_fix", generated_icon_fix_util::ToDebugValue(
                                     base::OptionalToPtr(generated_icon_fix_)));

  root.Set("supported_links_offer_ignore_count",
           supported_links_offer_ignore_count_);
  root.Set("supported_links_offer_dismiss_count",
           supported_links_offer_dismiss_count_);

  root.Set("is_diy_app", is_diy_app_);

  return base::Value(std::move(root));
}

base::Value WebApp::AsDebugValue() const {
  base::Value value = AsDebugValueWithOnlyPlatformAgnosticFields();
  auto& root = value.GetDict();

  root.Set("chromeos_data", OptionalAsDebugValue(chromeos_data_));

  root.Set("client_data", client_data_.AsDebugValue());

  return value;
}

std::ostream& operator<<(std::ostream& out, const WebApp& app) {
  return out << app.AsDebugValue();
}

std::ostream& operator<<(
    std::ostream& out,
    const WebApp::ExternalManagementConfig& management_config) {
  return out << management_config.AsDebugValue().DebugString();
}

bool operator==(const WebApp::ExternalManagementConfig& management_config1,
                const WebApp::ExternalManagementConfig& management_config2) {
  return std::tie(management_config1.install_urls,
                  management_config1.is_placeholder,
                  management_config1.additional_policy_ids) ==
         std::tie(management_config2.install_urls,
                  management_config2.is_placeholder,
                  management_config2.additional_policy_ids);
}

bool operator!=(const WebApp::ExternalManagementConfig& management_config1,
                const WebApp::ExternalManagementConfig& management_config2) {
  return !(management_config1 == management_config2);
}

namespace proto {

bool operator==(const WebAppOsIntegrationState& os_integration_state1,
                const WebAppOsIntegrationState& os_integration_state2) {
  return os_integration_state1.SerializeAsString() ==
         os_integration_state2.SerializeAsString();
}

bool operator!=(const WebAppOsIntegrationState& os_integration_state1,
                const WebAppOsIntegrationState& os_integration_state2) {
  return !(os_integration_state1 == os_integration_state2);
}

}  // namespace proto

std::vector<std::string> GetSerializedAllowedOrigins(
    const blink::ParsedPermissionsPolicyDeclaration
        permissions_policy_declaration) {
  std::vector<std::string> allowed_origins;
  if (permissions_policy_declaration.self_if_matches) {
    CHECK(!permissions_policy_declaration.self_if_matches->opaque());
    allowed_origins.push_back(
        permissions_policy_declaration.self_if_matches->Serialize());
  }
  for (const auto& origin_with_possible_wildcards :
       permissions_policy_declaration.allowed_origins) {
    allowed_origins.push_back(origin_with_possible_wildcards.Serialize());
  }
  return allowed_origins;
}

}  // namespace web_app

namespace sync_pb {

bool operator==(const WebAppSpecifics& sync_proto1,
                const WebAppSpecifics& sync_proto2) {
  return sync_proto1.SerializeAsString() == sync_proto2.SerializeAsString();
}

bool operator!=(const WebAppSpecifics& sync_proto1,
                const WebAppSpecifics& sync_proto2) {
  return !(sync_proto1 == sync_proto2);
}

}  // namespace sync_pb
