// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/geometry/size.h"

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

content::proto::ImageResource_Purpose
ManifestImageResourcePurposeToImageResoucePurposeProto(
    blink::mojom::ManifestImageResource_Purpose purpose) {
  switch (purpose) {
    case blink::mojom::ManifestImageResource_Purpose::ANY:
      return content::proto::ImageResource_Purpose_ANY;
    case blink::mojom::ManifestImageResource_Purpose::MONOCHROME:
      return content::proto::ImageResource_Purpose_MONOCHROME;
    case blink::mojom::ManifestImageResource_Purpose::MASKABLE:
      return content::proto::ImageResource_Purpose_MASKABLE;
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

absl::optional<std::vector<blink::Manifest::ImageResource>>
ParseAppImageResource(const char* container_name_for_logging,
                      const RepeatedImageResourceProto& manifest_icons_proto) {
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  for (const content::proto::ImageResource& image_resource_proto :
       manifest_icons_proto) {
    blink::Manifest::ImageResource image_resource;

    if (!image_resource_proto.has_src()) {
      DLOG(ERROR) << container_name_for_logging
                  << " ImageResource has missing url";
      return absl::nullopt;
    }
    image_resource.src = GURL(image_resource_proto.src());

    if (!image_resource.src.is_valid()) {
      DLOG(ERROR) << container_name_for_logging
                  << " ImageResource has invalid url: "
                  << image_resource.src.possibly_invalid_spec();
      return absl::nullopt;
    }

    if (image_resource_proto.has_type()) {
      image_resource.type = base::ASCIIToUTF16(image_resource_proto.type());
    }

    if (!image_resource_proto.sizes().empty()) {
      std::vector<gfx::Size> sizes;
      for (const auto& size_proto : image_resource_proto.sizes()) {
        sizes.emplace_back(size_proto.width(), size_proto.height());
      }
      image_resource.sizes = std::move(sizes);
    }

    std::vector<blink::mojom::ManifestImageResource_Purpose> purpose;
    if (!image_resource_proto.purpose().empty()) {
      for (const auto& purpose_proto : image_resource_proto.purpose()) {
        switch (purpose_proto) {
          case content::proto::ImageResource_Purpose_ANY:
            purpose.push_back(blink::mojom::ManifestImageResource_Purpose::ANY);
            break;
          case content::proto::ImageResource_Purpose_MASKABLE:
            purpose.push_back(
                blink::mojom::ManifestImageResource_Purpose::MASKABLE);
            break;
          case content::proto::ImageResource_Purpose_MONOCHROME:
            purpose.push_back(
                blink::mojom::ManifestImageResource_Purpose::MONOCHROME);
            break;
        }
      }
    } else {
      purpose.push_back(blink::mojom::ManifestImageResource_Purpose::ANY);
    }
    image_resource.purpose = std::move(purpose);
    manifest_icons.push_back(std::move(image_resource));
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
          app.user_display_mode().value_or(mojom::UserDisplayMode::kBrowser)));
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

content::proto::ImageResource AppImageResourceToProto(
    const blink::Manifest::ImageResource& image_resource) {
  content::proto::ImageResource image_resource_proto;
  DCHECK(!image_resource.src.is_empty());

  image_resource_proto.set_src(image_resource.src.spec());

  if (!image_resource.type.empty()) {
    image_resource_proto.set_type(base::UTF16ToASCII(image_resource.type));
  }

  for (const auto& size : image_resource.sizes) {
    content::proto::ImageResource::Size size_proto;
    size_proto.set_width(size.width());
    size_proto.set_height(size.height());
    *(image_resource_proto.add_sizes()) = size_proto;
  }

  for (const auto& purpose : image_resource.purpose) {
    image_resource_proto.add_purpose(
        ManifestImageResourcePurposeToImageResoucePurposeProto(purpose));
  }
  return image_resource_proto;
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
