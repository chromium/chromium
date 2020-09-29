// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <algorithm>
#include <ios>
#include <ostream>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/web_applications/components/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "components/sync/base/time.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/gfx/color_utils.h"

namespace {

std::string ColorToString(base::Optional<SkColor> color) {
  return color.has_value() ? color_utils::SkColorToRgbaString(color.value())
                           : "none";
}

}  // namespace

namespace web_app {

WebApp::WebApp(const AppId& app_id)
    : app_id_(app_id),
      display_mode_(DisplayMode::kUndefined),
      user_display_mode_(DisplayMode::kUndefined),
      chromeos_data_(IsChromeOs() ? base::make_optional<WebAppChromeOsData>()
                                  : base::nullopt) {}

WebApp::~WebApp() = default;

WebApp::WebApp(const WebApp& web_app) = default;

WebApp& WebApp::operator=(WebApp&& web_app) = default;

const SortedSizesPx& WebApp::downloaded_icon_sizes(IconPurpose purpose) const {
  switch (purpose) {
    case IconPurpose::ANY:
      return downloaded_icon_sizes_any_;
    case IconPurpose::MONOCHROME:
      // TODO (crbug.com/1114638): Download monochrome icons.
      NOTREACHED();
      return downloaded_icon_sizes_monochrome_;
    case IconPurpose::MASKABLE:
      return downloaded_icon_sizes_maskable_;
  }
}

void WebApp::AddSource(Source::Type source) {
  sources_[source] = true;
}

void WebApp::RemoveSource(Source::Type source) {
  sources_[source] = false;
}

bool WebApp::HasAnySources() const {
  return sources_.any();
}

bool WebApp::HasOnlySource(Source::Type source) const {
  Sources specified_sources;
  specified_sources[source] = true;
  return HasAnySpecifiedSourcesAndNoOtherSources(specified_sources);
}

bool WebApp::IsSynced() const {
  return sources_[Source::kSync];
}

bool WebApp::IsDefaultApp() const {
  return sources_[Source::kDefault];
}

bool WebApp::IsPolicyInstalledApp() const {
  return sources_[Source::kPolicy];
}

bool WebApp::IsSystemApp() const {
  return sources_[Source::kSystem];
}

bool WebApp::CanUserUninstallExternalApp() const {
  Sources specified_sources;
  specified_sources[Source::kDefault] = true;
  specified_sources[Source::kSync] = true;
  specified_sources[Source::kWebAppStore] = true;
  return HasAnySpecifiedSourcesAndNoOtherSources(specified_sources);
}

bool WebApp::HasAnySpecifiedSourcesAndNoOtherSources(
    Sources specified_sources) const {
  bool has_any_specified_sources = (sources_ & specified_sources).any();
  bool has_no_other_sources = (sources_ & ~specified_sources).none();
  return has_any_specified_sources && has_no_other_sources;
}

bool WebApp::WasInstalledByUser() const {
  return sources_[Source::kSync] || sources_[Source::kWebAppStore];
}

Source::Type WebApp::GetHighestPrioritySource() const {
  // Enumerators in Source enum are declaretd in the order of priority.
  // Top priority sources are declared first.
  for (int i = Source::kMinValue; i <= Source::kMaxValue; ++i) {
    auto source = static_cast<Source::Type>(i);
    if (sources_[source])
      return source;
  }

  NOTREACHED();
  return Source::kMaxValue;
}

void WebApp::SetName(const std::string& name) {
  name_ = name;
}

void WebApp::SetDescription(const std::string& description) {
  description_ = description;
}

void WebApp::SetStartUrl(const GURL& start_url) {
  DCHECK(!start_url.is_empty() && start_url.is_valid());
  start_url_ = start_url;
}

void WebApp::SetScope(const GURL& scope) {
  DCHECK(scope.is_empty() || scope.is_valid());
  scope_ = scope;
}

void WebApp::SetThemeColor(base::Optional<SkColor> theme_color) {
  theme_color_ = theme_color;
}

void WebApp::SetBackgroundColor(base::Optional<SkColor> background_color) {
  background_color_ = background_color;
}

void WebApp::SetDisplayMode(DisplayMode display_mode) {
  DCHECK_NE(DisplayMode::kUndefined, display_mode);
  display_mode_ = display_mode;
}

void WebApp::SetUserDisplayMode(DisplayMode user_display_mode) {
  switch (user_display_mode) {
    case DisplayMode::kBrowser:
      user_display_mode_ = DisplayMode::kBrowser;
      break;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
      NOTREACHED();
      FALLTHROUGH;
    case DisplayMode::kStandalone:
      user_display_mode_ = DisplayMode::kStandalone;
      break;
  }
}

void WebApp::SetDisplayModeOverride(
    std::vector<DisplayMode> display_mode_override) {
  display_mode_override_ = std::move(display_mode_override);
}

void WebApp::SetUserPageOrdinal(syncer::StringOrdinal page_ordinal) {
  user_page_ordinal_ = std::move(page_ordinal);
}

void WebApp::SetUserLaunchOrdinal(syncer::StringOrdinal launch_ordinal) {
  user_launch_ordinal_ = std::move(launch_ordinal);
}

void WebApp::SetWebAppChromeOsData(
    base::Optional<WebAppChromeOsData> chromeos_data) {
  chromeos_data_ = std::move(chromeos_data);
}

void WebApp::SetIsLocallyInstalled(bool is_locally_installed) {
  is_locally_installed_ = is_locally_installed;
}

void WebApp::SetIsInSyncInstall(bool is_in_sync_install) {
  is_in_sync_install_ = is_in_sync_install;
}

void WebApp::SetIconInfos(std::vector<WebApplicationIconInfo> icon_infos) {
  icon_infos_ = std::move(icon_infos);
}

void WebApp::SetDownloadedIconSizes(IconPurpose purpose, SortedSizesPx sizes) {
  switch (purpose) {
    case IconPurpose::ANY:
      downloaded_icon_sizes_any_ = std::move(sizes);
      break;
    case IconPurpose::MONOCHROME:
      // TODO (crbug.com/1114638): Add monochrome icons support.
      NOTREACHED();
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

void WebApp::SetShareTarget(base::Optional<apps::ShareTarget> share_target) {
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

void WebApp::SetShortcutsMenuItemInfos(
    std::vector<WebApplicationShortcutsMenuItemInfo>
        shortcuts_menu_item_infos) {
  shortcuts_menu_item_infos_ = std::move(shortcuts_menu_item_infos);
}

void WebApp::SetDownloadedShortcutsMenuIconsSizes(
    std::vector<std::vector<SquareSizePx>> sizes) {
  downloaded_shortcuts_menu_icons_sizes_ = std::move(sizes);
}

void WebApp::SetLastLaunchTime(const base::Time& time) {
  last_launch_time_ = time;
}

void WebApp::SetInstallTime(const base::Time& time) {
  install_time_ = time;
}

void WebApp::SetRunOnOsLoginMode(RunOnOsLoginMode mode) {
  run_on_os_login_mode_ = mode;
}

void WebApp::SetSyncFallbackData(SyncFallbackData sync_fallback_data) {
  sync_fallback_data_ = std::move(sync_fallback_data);
}

void WebApp::SetLaunchQueryParams(
    base::Optional<std::string> launch_query_params) {
  launch_query_params_ = std::move(launch_query_params);
}

WebApp::SyncFallbackData::SyncFallbackData() = default;

WebApp::SyncFallbackData::~SyncFallbackData() = default;

WebApp::SyncFallbackData::SyncFallbackData(
    const SyncFallbackData& sync_fallback_data) = default;

WebApp::SyncFallbackData& WebApp::SyncFallbackData::operator=(
    SyncFallbackData&& sync_fallback_data) = default;

std::ostream& operator<<(std::ostream& out,
                         const WebApp::SyncFallbackData& sync_fallback_data) {
  out << "    theme_color: " << ColorToString(sync_fallback_data.theme_color)
      << std::endl
      << "    name: " << sync_fallback_data.name << std::endl
      << "    scope: " << sync_fallback_data.scope << std::endl;
  for (const WebApplicationIconInfo& icon : sync_fallback_data.icon_infos)
    out << "    icon_info: " << icon << std::endl;
  return out;
}

std::ostream& operator<<(std::ostream& out, const WebApp& app) {
  out << "app_id: " << app.app_id_ << std::endl
      << "  name: " << app.name_ << std::endl
      << "  start_url: " << app.start_url_ << std::endl
      << "  launch_query_params: "
      << (app.launch_query_params_ ? *app.launch_query_params_ : std::string())
      << std::endl
      << "  scope: " << app.scope_ << std::endl
      << "  theme_color: " << ColorToString(app.theme_color_) << std::endl
      << "  background_color: " << ColorToString(app.background_color_)
      << std::endl
      << "  display_mode: " << blink::DisplayModeToString(app.display_mode_)
      << std::endl
      << "  display_override: " << app.display_mode_override_.size()
      << std::endl;
  for (const DisplayMode& mode : app.display_mode_override_)
    out << "    " << blink::DisplayModeToString(mode) << std::endl;
  out << "  user_display_mode: "
      << blink::DisplayModeToString(app.user_display_mode_) << std::endl
      << "  user_page_ordinal: " << app.user_page_ordinal_.ToDebugString()
      << std::endl
      << "  user_launch_ordinal: " << app.user_launch_ordinal_.ToDebugString()
      << std::endl
      << "  sources: " << app.sources_.to_string() << std::endl
      << "  is_locally_installed: " << app.is_locally_installed_ << std::endl
      << "  is_in_sync_install: " << app.is_in_sync_install_ << std::endl
      << "  sync_fallback_data: " << std::endl
      << app.sync_fallback_data_ << "  description: " << app.description_
      << std::endl
      << "  last_launch_time: " << app.last_launch_time_ << std::endl
      << "  install_time: " << app.install_time_ << std::endl
      << "  is_generated_icon: " << app.is_generated_icon_ << std::endl
      << "  run_on_os_login_mode: "
      << RunOnOsLoginModeToString(app.run_on_os_login_mode_) << std::endl;
  for (const WebApplicationIconInfo& icon : app.icon_infos_)
    out << "  icon_info: " << icon << std::endl;
  for (SquareSizePx size : app.downloaded_icon_sizes_any_)
    out << "  downloaded_icon_sizes_any_: " << size << std::endl;
  for (SquareSizePx size : app.downloaded_icon_sizes_maskable_)
    out << "  downloaded_icon_sizes_maskable_: " << size << std::endl;
  for (const apps::FileHandler& file_handler : app.file_handlers_)
    out << "  file_handler: " << file_handler << std::endl;
  if (app.share_target_)
    out << "  share_target: " << *app.share_target_ << std::endl;
  for (const std::string& additional_search_term : app.additional_search_terms_)
    out << "  additional_search_term: " << additional_search_term << std::endl;
  for (const apps::ProtocolHandlerInfo& protocol_handler :
       app.protocol_handlers_) {
    out << "  protocol_handler: " << protocol_handler << std::endl;
  }

  out << " chromeos_data: " << app.chromeos_data_.has_value() << std::endl;
  if (app.chromeos_data_.has_value())
    out << app.chromeos_data_.value();

  return out;
}

bool operator==(const WebApp::SyncFallbackData& sync_fallback_data1,
                const WebApp::SyncFallbackData& sync_fallback_data2) {
  return std::tie(sync_fallback_data1.name, sync_fallback_data1.theme_color,
                  sync_fallback_data1.scope, sync_fallback_data1.icon_infos) ==
         std::tie(sync_fallback_data2.name, sync_fallback_data2.theme_color,
                  sync_fallback_data2.scope, sync_fallback_data2.icon_infos);
}

bool operator!=(const WebApp::SyncFallbackData& sync_fallback_data1,
                const WebApp::SyncFallbackData& sync_fallback_data2) {
  return !(sync_fallback_data1 == sync_fallback_data2);
}

bool operator==(const WebApp& app1, const WebApp& app2) {
  return std::tie(app1.app_id_, app1.sources_, app1.name_, app1.start_url_,
                  app1.launch_query_params_, app1.description_, app1.scope_,
                  app1.theme_color_, app1.background_color_, app1.icon_infos_,
                  app1.downloaded_icon_sizes_any_,
                  app1.downloaded_icon_sizes_maskable_, app1.is_generated_icon_,
                  app1.display_mode_, app1.display_mode_override_,
                  app1.user_display_mode_, app1.user_page_ordinal_,
                  app1.user_launch_ordinal_, app1.chromeos_data_,
                  app1.is_locally_installed_, app1.is_in_sync_install_,
                  app1.file_handlers_, app1.share_target_,
                  app1.additional_search_terms_, app1.protocol_handlers_,
                  app1.sync_fallback_data_, app1.last_launch_time_,
                  app1.install_time_, app1.run_on_os_login_mode_) ==
         std::tie(app2.app_id_, app2.sources_, app2.name_, app2.start_url_,
                  app2.launch_query_params_, app2.description_, app2.scope_,
                  app2.theme_color_, app2.background_color_, app2.icon_infos_,
                  app2.downloaded_icon_sizes_any_,
                  app2.downloaded_icon_sizes_maskable_, app2.is_generated_icon_,
                  app2.display_mode_, app2.display_mode_override_,
                  app2.user_display_mode_, app2.user_page_ordinal_,
                  app2.user_launch_ordinal_, app2.chromeos_data_,
                  app2.is_locally_installed_, app2.is_in_sync_install_,
                  app2.file_handlers_, app2.share_target_,
                  app2.additional_search_terms_, app2.protocol_handlers_,
                  app2.sync_fallback_data_, app2.last_launch_time_,
                  app2.install_time_, app2.run_on_os_login_mode_);
}

bool operator!=(const WebApp& app1, const WebApp& app2) {
  return !(app1 == app2);
}

}  // namespace web_app
