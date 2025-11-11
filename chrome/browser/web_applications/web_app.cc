// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_value_list.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-data-view.h"
#include "chrome/browser/web_applications/proto/web_app.equal.h"
#include "chrome/browser/web_applications/proto/web_app.ostream.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app.to_value.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.to_value.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.equal.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.to_value.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/url_pattern_with_regex_matcher.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_scope.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/web_app_specifics.equal.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/protocol/web_app_specifics.to_value.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/webapps/isolated_web_apps/scheme.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

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

base::Value::Dict ImageResourceDebugDict(
    const blink::Manifest::ImageResource& icon) {
  const auto kPurposeStrings =
      std::to_array<const char*>({"Any", "Monochrome", "Maskable"});

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

  if (std::holds_alternative<TabStrip::Visibility>(tab_strip->home_tab)) {
    result.Set(
        "home_tab",
        base::ToString(std::get<TabStrip::Visibility>(tab_strip->home_tab)));
  } else {
    base::Value::Dict home_tab_json;
    const blink::Manifest::HomeTabParams& home_tab_params =
        std::get<blink::Manifest::HomeTabParams>(tab_strip->home_tab);

    base::Value::List icons_json;
    std::optional<std::vector<blink::Manifest::ImageResource>> icons =
        home_tab_params.icons;

    for (auto& icon : *icons) {
      icons_json.Append(ImageResourceDebugDict(icon));
    }

    home_tab_json.Set("icons", std::move(icons_json));
    home_tab_json.Set("scope_patterns",
                      base::ToValueList(home_tab_params.scope_patterns,
                                        UrlPatternDebugValue));
    result.Set("home_tab", std::move(home_tab_json));
  }
  return base::Value(std::move(result));
}

base::Value RelatedApplicationsToDebugValue(
    const std::vector<blink::Manifest::RelatedApplication>&
        related_applications) {
  base::Value::List related_applications_json;
  for (const auto& related_application : related_applications) {
    base::Value::Dict related_application_json;
    related_application_json.Set("platform",
                                 related_application.platform.value());
    if (related_application.url.is_valid()) {
      related_application_json.Set("url", related_application.url.spec());
    }
    if (related_application.id.has_value()) {
      related_application_json.Set("id", related_application.id.value());
    }
    related_applications_json.Append(std::move(related_application_json));
  }
  return base::Value(std::move(related_applications_json));
}

void CheckValidPendingUpdateInfo(
    const std::optional<proto::PendingUpdateInfo>& pending_update_info) {
  if (pending_update_info.has_value()) {
    CHECK(pending_update_info->has_name() ||
          (!pending_update_info->trusted_icons().empty() &&
           !pending_update_info->manifest_icons().empty()));
    if (!pending_update_info->trusted_icons().empty() &&
        !pending_update_info->manifest_icons().empty()) {
      for (const auto& icon : pending_update_info->trusted_icons()) {
        CHECK(icon.has_url() && icon.has_purpose());
      }
      for (const auto& icon : pending_update_info->manifest_icons()) {
        CHECK(icon.has_url() && icon.has_purpose());
      }
      CHECK(!pending_update_info->downloaded_manifest_icons().empty() &&
            !pending_update_info->downloaded_trusted_icons().empty());
    }
    CHECK(pending_update_info->has_was_ignored());
  }
}

}  // namespace

WebApp::CachedDerivedData::CachedDerivedData() = default;
WebApp::CachedDerivedData::~CachedDerivedData() = default;
WebApp::CachedDerivedData::CachedDerivedData(CachedDerivedData&&) = default;
WebApp::CachedDerivedData& WebApp::CachedDerivedData::operator=(
    CachedDerivedData&&) = default;
WebApp::CachedDerivedData::CachedDerivedData(const CachedDerivedData&) {}
WebApp::CachedDerivedData& WebApp::CachedDerivedData::operator=(
    const CachedDerivedData&) {
  return *this;
}

WebApp::WebApp(const webapps::AppId& app_id)
    : app_id_(app_id),
      chromeos_data_(IsChromeOsDataMandatory()
                         ? std::make_optional<WebAppChromeOsData>()
                         : std::nullopt) {}

WebApp::WebApp(const webapps::ManifestId& manifest_id,
               const GURL& start_url,
               const GURL& scope,
               std::optional<webapps::AppId> parent_app_id,
               std::optional<webapps::ManifestId> parent_manifest_id)
    : app_id_(GenerateAppIdFromManifestId(manifest_id, parent_manifest_id)),
      start_url_(start_url),
      scope_(scope),
      chromeos_data_(IsChromeOsDataMandatory()
                         ? std::make_optional<WebAppChromeOsData>()
                         : std::nullopt),
      manifest_id_(manifest_id),
      parent_app_id_(parent_app_id) {
  CHECK(manifest_id.is_valid());
  CHECK(start_url.is_valid());
  CHECK(scope.is_valid());
  CHECK(url::IsSameOriginWith(manifest_id_, start_url_))
      << manifest_id_.spec() << " vs " << start_url_.spec();
  CHECK(url::IsSameOriginWith(start_url_, scope_))
      << start_url_.spec() << " vs " << scope_.spec();
  CHECK(!scope_.has_ref() && !scope_.has_query());
  CHECK(!manifest_id_.has_ref());
  CHECK(base::StartsWith(start_url_.spec(), scope_.spec(),
                         base::CompareCase::SENSITIVE))
      << "Start URL " << start_url_ << " must be nested in scope " << scope_;
  if (parent_app_id_.has_value()) {
    CHECK(!parent_app_id_->empty());
  }
  CHECK(!!parent_app_id == !!parent_manifest_id);
  // Ensure sync proto is initialized.
  SetSyncProto(sync_proto_);
}

WebApp::~WebApp() = default;
WebApp::WebApp(const WebApp& web_app) = default;
WebApp::WebApp(WebApp&& web_app) = default;

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

WebAppScope WebApp::GetScope() const {
  return WebAppScope(app_id_, scope_, validated_scope_extensions_,
                     base::PassKey<WebApp>());
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

const SortedSizesPx& WebApp::stored_trusted_icon_sizes(
    IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::ANY:
      return stored_trusted_icon_sizes_any_;
    case IconPurpose::MASKABLE:
      return stored_trusted_icon_sizes_maskable_;
    case IconPurpose::MONOCHROME:
      NOTREACHED();
  }
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

bool WebApp::WasInstalledByTrustedSources() const {
  // WebAppChromeOsData::oem_installed is not included here as
  // we would like to keep WebAppManagement::kOem and
  // WebAppChromeOsData::oem_installed separate.
  // WebAppChromeOsData::oem_installed will be migrated to
  // WebAppManagement::kOem eventually.
  return sources_.Has(WebAppManagement::kDefault) ||
         sources_.Has(WebAppManagement::kPolicy) ||
         sources_.Has(WebAppManagement::kKiosk) ||
         sources_.Has(WebAppManagement::kOem) ||
         sources_.Has(WebAppManagement::kApsDefault);
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
  CHECK(url::IsSameOriginWith(manifest_id(), start_url))
      << manifest_id().spec() << " " << start_url.spec();
  start_url_ = start_url;

  // Ensure sync proto is initialized and remains consistent. Logic in
  // `SetSyncProto` will populate an unset `start_url` on the proto.
  sync_proto_.clear_start_url();
  SetSyncProto(sync_proto_);
  // Ensure that scope is always set.
  if (scope_.is_empty()) {
    scope_ = start_url_.GetWithoutFilename();
  }
}

void WebApp::SetScope(const GURL& scope) {
  GURL scope_for_app = scope;
  // If the given scope is empty, populate the scope from the `start_url_`.
  if (scope.is_empty()) {
    CHECK(start_url_.is_valid());
    scope_for_app = start_url_.GetWithoutFilename();
  }
  CHECK(scope_for_app.is_valid());
  // Ensure that the scope can never include queries or fragments, as per spec.
  GURL::Replacements scope_replacements;
  scope_replacements.ClearRef();
  scope_replacements.ClearQuery();
  scope_ = scope_for_app.ReplaceComponents(scope_replacements);
  // Post-migration check: Scope should never be empty after setting.
  CHECK(!scope_.is_empty());
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

void WebApp::SetBorderlessUrlPatterns(
    std::vector<blink::SafeUrlPattern> borderless_url_patterns) {
  borderless_url_patterns_ = std::move(borderless_url_patterns);
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
    CHECK(start_url().is_valid());
    // Note: sync data may have a start_url that does not match the `WebApp`
    // start_url, but it does not update the app (matching pre-M125 behaviour).
    if (!sync_proto.has_start_url()) {
      sync_proto.set_start_url(start_url().spec());
    }
  }

  // Sync data must never be set on an app with mismatching manifest_id.
  CHECK(manifest_id().is_valid());
  std::string relative_manifest_id_path = RelativeManifestIdPath(manifest_id());
  if (sync_proto.has_relative_manifest_id()) {
    CHECK_EQ(sync_proto.relative_manifest_id(), relative_manifest_id_path);
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
  if (!ParseAppIconInfos("SetSyncProtoTrustedIcons", sync_proto.trusted_icons())
           .has_value()) {
    sync_proto.clear_trusted_icons();
  }

  sync_proto_ = std::move(sync_proto);
}

void WebApp::SetLaunchQueryParams(
    std::optional<std::string> launch_query_params) {
  launch_query_params_ = std::move(launch_query_params);
}

void WebApp::SetManifestUrl(const GURL& manifest_url) {
  CHECK(manifest_url.is_valid() || manifest_url.is_empty(),
        base::NotFatalUntil::M138);
  manifest_url_ = manifest_url;
}

void WebApp::SetManifestId(const webapps::ManifestId& manifest_id) {
  CHECK(manifest_id.is_valid());
  CHECK(start_url_.is_empty() || url::IsSameOriginWith(start_url_, manifest_id))
      << start_url_.spec() << " vs " << manifest_id.spec();
  CHECK(!manifest_id.has_ref());
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
    network::ParsedPermissionsPolicy permissions_policy) {
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
  cached_derived_data_.home_tab_scope.reset();
}

void WebApp::SetCurrentOsIntegrationStates(
    proto::os_state::WebAppOsIntegration current_os_integration_states) {
  current_os_integration_states_ = std::move(current_os_integration_states);
}

void WebApp::SetIsolationData(IsolationData isolation_data) {
  CHECK(manifest_id_.is_valid()
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
        && manifest_id_.SchemeIs(webapps::kIsolatedAppScheme))
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
      ;
  if (isolation_data.pending_update_info().has_value()) {
    DCHECK_EQ(isolation_data.location().dev_mode(),
              isolation_data.pending_update_info()->location.dev_mode())
        << "IsolationData dev_mode mismatch between current location and "
           "pending update location.";
  }
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

void WebApp::SetWasShortcutApp(bool was_shortcut_app) {
  was_shortcut_app_ = was_shortcut_app;
}

void WebApp::SetDiyAppIconsMaskedOnMac(bool diy_app_icons_masked_on_mac) {
  diy_app_icons_masked_on_mac_ = diy_app_icons_masked_on_mac;
}

void WebApp::SetRelatedApplications(
    std::vector<blink::Manifest::RelatedApplication> related_applications) {
  related_applications_ = std::move(related_applications);
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
  if (!management_to_external_config_map_.count(type)) {
    return false;
  }

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
    std::optional<proto::GeneratedIconFix> generated_icon_fix) {
  CHECK(!generated_icon_fix.has_value() ||
        generated_icon_fix_util::IsValid(*generated_icon_fix));
  generated_icon_fix_ = generated_icon_fix;
}

void WebApp::SetPendingUpdateInfo(
    std::optional<proto::PendingUpdateInfo> pending_update_info) {
  CheckValidPendingUpdateInfo(pending_update_info);
  pending_update_info_ = std::move(pending_update_info);
}

void WebApp::SetTrustedIcons(std::vector<apps::IconInfo> trusted_icons) {
  trusted_icons_ = std::move(trusted_icons);
}

void WebApp::SetStoredTrustedIconSizes(IconPurpose purpose,
                                       SortedSizesPx sizes) {
  switch (purpose) {
    case IconPurpose::ANY:
      stored_trusted_icon_sizes_any_ = std::move(sizes);
      break;
    case IconPurpose::MASKABLE:
      stored_trusted_icon_sizes_maskable_ = std::move(sizes);
      break;
    case IconPurpose::MONOCHROME:
      NOTREACHED();
  }
}

void WebApp::SetInstalledBy(InstalledByPassKey,
                            std::deque<AppInstalledBy> installed_by) {
  for (const AppInstalledBy& data : installed_by) {
    CHECK(data.requesting_url().is_valid());
  }
  installed_by_ = std::move(installed_by);
}

constexpr int kMaxInstalledBySize = 10;
void WebApp::AddInstalledByInfo(AppInstalledBy installed_by_info) {
  CHECK(installed_by_info.requesting_url().is_valid());

  // Check for duplicates - only compare URLs, not timestamps.
  // Remove duplicate entry so it can be re-added with updated timestamp.
  installed_by_.erase(
      std::remove_if(installed_by_.begin(), installed_by_.end(),
                     [&installed_by_info](const AppInstalledBy& info) {
                       return info.requesting_url() ==
                              installed_by_info.requesting_url();
                     }),
      installed_by_.end());

  // Add the new entry.
  installed_by_.push_back(std::move(installed_by_info));

  // Enforce max 10 entries - remove oldest if exceeding capacity.
  if (installed_by_.size() > kMaxInstalledBySize) {
    installed_by_.pop_front();
  }
  CHECK(installed_by_.size() <= kMaxInstalledBySize);
}

WebApp::ClientData::ClientData() = default;

WebApp::ClientData::~ClientData() = default;

WebApp::ClientData::ClientData(const ClientData& client_data) = default;

base::Value WebApp::ClientData::AsDebugValue() const {
  base::Value::Dict root;
#if BUILDFLAG(IS_CHROMEOS)
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

const std::vector<UrlPatternWithRegexMatcher>& WebApp::GetTabbedModeHomeScope()
    const {
  if (!cached_derived_data_.home_tab_scope.has_value()) {
    cached_derived_data_.home_tab_scope.emplace();
    if (tab_strip_.has_value()) {
      if (const auto* params = std::get_if<blink::Manifest::HomeTabParams>(
              &tab_strip_->home_tab)) {
        for (auto& pattern : params->scope_patterns) {
          cached_derived_data_.home_tab_scope->emplace_back(pattern);
        }
      }
    }
  }
  return *cached_derived_data_.home_tab_scope;
}

const std::optional<proto::GeneratedIconFix>& WebApp::generated_icon_fix()
    const {
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
        app.borderless_url_patterns_,
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
        app.manifest_url_,
        app.manifest_id_,
#if BUILDFLAG(IS_CHROMEOS)
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
        app.is_diy_app_,
        app.was_shortcut_app_,
        app.related_applications_,
        app.diy_app_icons_masked_on_mac_,
        app.pending_update_info_,
        app.trusted_icons_,
        app.stored_trusted_icon_sizes_any_,
        app.stored_trusted_icon_sizes_maskable_,
        app.installed_by_
        // clang-format on
    );
  };
  return AsTuple(*this) == AsTuple(other);
}

base::Value WebApp::AsDebugValueWithOnlyPlatformAgnosticFields() const {
  base::Value::Dict root;

  auto ConvertList = [](const auto& list) {
    base::Value::List list_json;
    for (const auto& item : list) {
      list_json.Append(item);
    }
    return list_json;
  };

  auto ConvertDebugValueList = [](const auto& list) {
    base::Value::List list_json;
    for (const auto& item : list) {
      list_json.Append(item.AsDebugValue());
    }
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

  root.Set("data_size_in_bytes", OptionalToStringValue(data_size_in_bytes_));

  root.Set("dark_mode_background_color",
           ColorToString(dark_mode_background_color_));

  root.Set("dark_mode_theme_color", ColorToString(dark_mode_theme_color_));

  root.Set("disallowed_launch_protocols",
           ConvertList(disallowed_launch_protocols_));

  root.Set("description", description_);

  root.Set("display_mode", blink::DisplayModeToString(display_mode_));

  base::Value::List display_override;
  for (const DisplayMode& mode : display_mode_override_) {
    display_override.Append(blink::DisplayModeToString(mode));
  }
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
  for (const auto& it : management_to_external_config_map_) {
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
    launch_handler_json.Set(
        "client_mode", base::ToString(launch_handler_->parsed_client_mode()));
    launch_handler_json.Set("client_mode_valid_and_specified",
                            launch_handler_->client_mode_valid_and_specified());
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

  root.Set("scope_extensions", ConvertDebugValueList(scope_extensions_));

  root.Set("scope_extensions_validated",
           ConvertDebugValueList(validated_scope_extensions_));

  root.Set("window_controls_overlay_enabled", window_controls_overlay_enabled_);

  root.Set("tab_strip", OptTabStripToDebugValue(tab_strip_));

  root.Set("always_show_toolbar_in_fullscreen",
           always_show_toolbar_in_fullscreen_);

  root.Set("current_os_integration_states",
           proto::os_state::ToValue(current_os_integration_states_));

  root.Set("isolation_data", OptionalAsDebugValue(isolation_data_));

  root.Set("user_link_capturing_preference",
           base::ToString(user_link_capturing_preference_));

  root.Set("latest_install_time", base::ToString(latest_install_time_));

  proto::MaybeToValue(generated_icon_fix_, "generated_icon_fix", root);

  root.Set("supported_links_offer_ignore_count",
           supported_links_offer_ignore_count_);
  root.Set("supported_links_offer_dismiss_count",
           supported_links_offer_dismiss_count_);

  root.Set("is_diy_app", is_diy_app_);

  root.Set("was_shortcut_app", was_shortcut_app_);

  root.Set("diy_app_icons_masked_on_mac", diy_app_icons_masked_on_mac_);

  root.Set("related_applications",
           RelatedApplicationsToDebugValue(related_applications_));

  proto::MaybeToValue(pending_update_info_, "pending_update_info", root);

  root.Set("trusted_icons", ConvertDebugValueList(trusted_icons_));

  base::Value::List installed_by_list;
  for (const auto& installed_by_data : installed_by_) {
    installed_by_list.Append(installed_by_data.InstalledByToDebugValue());
  }
  root.Set("installed_by", std::move(installed_by_list));

  base::Value::Dict stored_trusted_icon_sizes_json;
  for (IconPurpose purpose : kIconPurposes) {
    // There can never be trusted monochrome icons.
    if (purpose == IconPurpose::MONOCHROME) {
      continue;
    }
    stored_trusted_icon_sizes_json.Set(
        base::ToString(purpose),
        ConvertList(stored_trusted_icon_sizes(purpose)));
  }
  root.Set("stored_trusted_icon_sizes",
           std::move(stored_trusted_icon_sizes_json));

  root.Set("borderless_url_patterns",
           base::ToValueList(borderless_url_patterns_, UrlPatternDebugValue));

  return base::Value(std::move(root));
}

base::Value WebApp::AsDebugValue() const {
  base::Value value = AsDebugValueWithOnlyPlatformAgnosticFields();
  auto& root = value.GetDict();

  root.Set("chromeos_data", OptionalAsDebugValue(chromeos_data_));

  root.Set("client_data", client_data_.AsDebugValue());

  // The user_display_mode getter CHECK fails if sync_proto_ isn't initialized.
  if (sync_proto_.has_start_url()) {
    root.Set("user_display_mode", base::ToString(user_display_mode()));
  }

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

std::vector<std::string> GetSerializedAllowedOrigins(
    const network::ParsedPermissionsPolicyDeclaration
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
