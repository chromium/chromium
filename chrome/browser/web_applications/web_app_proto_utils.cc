// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

namespace {

absl::optional<apps::IconInfo::Purpose> SyncPurposeToIconInfoPurpose(
    sync_pb::WebAppIconInfo_Purpose purpose) {
  switch (purpose) {
    // Treat UNSPECIFIED purpose as invalid. It means a new purpose was added
    // that this client does not understand.
    case sync_pb::WebAppIconInfo_Purpose_UNSPECIFIED:
      return absl::nullopt;
    case sync_pb::WebAppIconInfo_Purpose_ANY:
      return apps::IconInfo::Purpose::kAny;
    case sync_pb::WebAppIconInfo_Purpose_MASKABLE:
      return apps::IconInfo::Purpose::kMaskable;
    case sync_pb::WebAppIconInfo_Purpose_MONOCHROME:
      return apps::IconInfo::Purpose::kMonochrome;
  }
}

sync_pb::WebAppIconInfo_Purpose IconInfoPurposeToSyncPurpose(
    apps::IconInfo::Purpose purpose) {
  switch (purpose) {
    case apps::IconInfo::Purpose::kAny:
      return sync_pb::WebAppIconInfo_Purpose_ANY;
    case apps::IconInfo::Purpose::kMonochrome:
      return sync_pb::WebAppIconInfo_Purpose_MONOCHROME;
    case apps::IconInfo::Purpose::kMaskable:
      return sync_pb::WebAppIconInfo_Purpose_MASKABLE;
  }
}

}  // namespace

absl::optional<std::vector<apps::IconInfo>> ParseAppIconInfos(
    const char* container_name_for_logging,
    const RepeatedIconInfosProto& manifest_icons_proto) {
  std::vector<apps::IconInfo> manifest_icons;
  for (const sync_pb::WebAppIconInfo& icon_info_proto : manifest_icons_proto) {
    apps::IconInfo icon_info;

    if (icon_info_proto.has_size_in_px())
      icon_info.square_size_px = icon_info_proto.size_in_px();

    if (!icon_info_proto.has_url()) {
      DLOG(ERROR) << container_name_for_logging << " IconInfo has missing url";
      return absl::nullopt;
    }
    icon_info.url = GURL(icon_info_proto.url());
    if (!icon_info.url.is_valid()) {
      DLOG(ERROR) << container_name_for_logging << " IconInfo has invalid url: "
                  << icon_info.url.possibly_invalid_spec();
      return absl::nullopt;
    }

    if (icon_info_proto.has_purpose()) {
      absl::optional<apps::IconInfo::Purpose> opt_purpose =
          SyncPurposeToIconInfoPurpose(icon_info_proto.purpose());
      if (!opt_purpose.has_value())
        return absl::nullopt;
      icon_info.purpose = opt_purpose.value();
    } else {
      // Treat unset purpose as ANY so that old data without the field is
      // interpreted correctly.
      icon_info.purpose = apps::IconInfo::Purpose::kAny;
    }

    manifest_icons.push_back(std::move(icon_info));
  }
  return manifest_icons;
}

sync_pb::WebAppSpecifics WebAppToSyncProto(const WebApp& app) {
  DCHECK(!app.start_url().is_empty());
  DCHECK(app.start_url().is_valid());

  sync_pb::WebAppSpecifics sync_proto;
  if (app.manifest_id().has_value())
    sync_proto.set_manifest_id(app.manifest_id().value());
  sync_proto.set_start_url(app.start_url().spec());
  sync_proto.set_user_display_mode(
      ConvertUserDisplayModeToWebAppSpecificsUserDisplayMode(
          app.user_display_mode().value_or(UserDisplayMode::kBrowser)));
  sync_proto.set_name(app.sync_fallback_data().name);
  if (app.sync_fallback_data().theme_color.has_value())
    sync_proto.set_theme_color(app.sync_fallback_data().theme_color.value());
  if (app.user_page_ordinal().IsValid()) {
    sync_proto.set_user_page_ordinal(app.user_page_ordinal().ToInternalValue());
  }
  if (app.user_launch_ordinal().IsValid()) {
    sync_proto.set_user_launch_ordinal(
        app.user_launch_ordinal().ToInternalValue());
  }
  if (app.sync_fallback_data().scope.is_valid())
    sync_proto.set_scope(app.sync_fallback_data().scope.spec());
  for (const apps::IconInfo& icon_info : app.sync_fallback_data().icon_infos) {
    *(sync_proto.add_icon_infos()) = AppIconInfoToSyncProto(icon_info);
  }
  return sync_proto;
}

sync_pb::WebAppIconInfo AppIconInfoToSyncProto(
    const apps::IconInfo& icon_info) {
  sync_pb::WebAppIconInfo icon_info_proto;
  if (icon_info.square_size_px.has_value())
    icon_info_proto.set_size_in_px(icon_info.square_size_px.value());
  DCHECK(!icon_info.url.is_empty());
  icon_info_proto.set_url(icon_info.url.spec());
  icon_info_proto.set_purpose(IconInfoPurposeToSyncPurpose(icon_info.purpose));
  return icon_info_proto;
}

absl::optional<WebApp::SyncFallbackData> ParseSyncFallbackDataStruct(
    const sync_pb::WebAppSpecifics& sync_proto) {
  WebApp::SyncFallbackData parsed_sync_fallback_data;

  parsed_sync_fallback_data.name = sync_proto.name();

  if (sync_proto.has_theme_color())
    parsed_sync_fallback_data.theme_color = sync_proto.theme_color();

  if (sync_proto.has_scope()) {
    parsed_sync_fallback_data.scope = GURL(sync_proto.scope());
    if (!parsed_sync_fallback_data.scope.is_valid()) {
      DLOG(ERROR) << "WebAppSpecifics scope has invalid url: "
                  << parsed_sync_fallback_data.scope.possibly_invalid_spec();
      return absl::nullopt;
    }
  }

  absl::optional<std::vector<apps::IconInfo>> parsed_icon_infos =
      ParseAppIconInfos("WebAppSpecifics", sync_proto.icon_infos());
  if (!parsed_icon_infos)
    return absl::nullopt;

  parsed_sync_fallback_data.icon_infos = std::move(parsed_icon_infos.value());

  return parsed_sync_fallback_data;
}

::sync_pb::WebAppSpecifics::UserDisplayMode ToWebAppSpecificsUserDisplayMode(
    DisplayMode user_display_mode) {
  switch (user_display_mode) {
    case DisplayMode::kBrowser:
      return ::sync_pb::WebAppSpecifics::BROWSER;
    case DisplayMode::kTabbed:
      return ::sync_pb::WebAppSpecifics::TABBED;
    case DisplayMode::kUndefined:
    case DisplayMode::kMinimalUi:
    case DisplayMode::kFullscreen:
    case DisplayMode::kWindowControlsOverlay:
    case DisplayMode::kBorderless:
      NOTREACHED();
      [[fallthrough]];
    case DisplayMode::kStandalone:
      return ::sync_pb::WebAppSpecifics::STANDALONE;
  }
}

RunOnOsLoginMode ToRunOnOsLoginMode(WebAppProto::RunOnOsLoginMode mode) {
  switch (mode) {
    case WebAppProto::MINIMIZED:
      return RunOnOsLoginMode::kMinimized;
    case WebAppProto::WINDOWED:
      return RunOnOsLoginMode::kWindowed;
    case WebAppProto::NOT_RUN:
    default:
      return RunOnOsLoginMode::kNotRun;
  }
}

WebAppProto::RunOnOsLoginMode ToWebAppProtoRunOnOsLoginMode(
    RunOnOsLoginMode mode) {
  switch (mode) {
    case RunOnOsLoginMode::kMinimized:
      return WebAppProto::MINIMIZED;
    case RunOnOsLoginMode::kWindowed:
      return WebAppProto::WINDOWED;
    case RunOnOsLoginMode::kNotRun:
      return WebAppProto::NOT_RUN;
  }
}

}  // namespace web_app
