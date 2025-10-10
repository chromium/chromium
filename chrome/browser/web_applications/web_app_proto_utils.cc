// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_proto_utils.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_tab_strip.pb.h"
#include "chrome/browser/web_applications/proto/web_app_url_pattern.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/common/web_app_id.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/liburlpattern/part.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web_app {

namespace {

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

proto::UrlPatternPart::Modifier UrlPatternModifierToProto(
    liburlpattern::Modifier modifier) {
  switch (modifier) {
    case liburlpattern::Modifier::kZeroOrMore:
      return proto::UrlPatternPart::MODIFIER_ZERO_OR_MORE;
    case liburlpattern::Modifier::kOptional:
      return proto::UrlPatternPart::MODIFIER_OPTIONAL;
    case liburlpattern::Modifier::kOneOrMore:
      return proto::UrlPatternPart::MODIFIER_ONE_OR_MORE;
    case liburlpattern::Modifier::kNone:
      return proto::UrlPatternPart::MODIFIER_NONE;
  }
}

std::optional<liburlpattern::Modifier> ProtoToUrlPatternModifier(
    proto::UrlPatternPart::Modifier modifier) {
  switch (modifier) {
    case proto::UrlPatternPart::MODIFIER_UNSPECIFIED:
      return std::nullopt;
    case proto::UrlPatternPart::MODIFIER_ZERO_OR_MORE:
      return liburlpattern::Modifier::kZeroOrMore;
    case proto::UrlPatternPart::MODIFIER_OPTIONAL:
      return liburlpattern::Modifier::kOptional;
    case proto::UrlPatternPart::MODIFIER_ONE_OR_MORE:
      return liburlpattern::Modifier::kOneOrMore;
    case proto::UrlPatternPart::MODIFIER_NONE:
      return liburlpattern::Modifier::kNone;
  }
}

proto::UrlPatternPart::PartType UrlPatternPartTypeToProto(
    liburlpattern::PartType part_type) {
  switch (part_type) {
    case liburlpattern::PartType::kRegex:
      NOTREACHED();
    case liburlpattern::PartType::kFullWildcard:
      return proto::UrlPatternPart::PART_TYPE_FULL_WILDCARD;
    case liburlpattern::PartType::kSegmentWildcard:
      return proto::UrlPatternPart::PART_TYPE_SEGMENT_WILDCARD;
    case liburlpattern::PartType::kFixed:
      return proto::UrlPatternPart::PART_TYPE_FIXED;
  }
}

std::optional<liburlpattern::PartType> ProtoToUrlPatternPartType(
    proto::UrlPatternPart::PartType part_type) {
  switch (part_type) {
    case proto::UrlPatternPart::PART_TYPE_UNSPECIFIED:
      return std::nullopt;
    case proto::UrlPatternPart::PART_TYPE_FULL_WILDCARD:
      return liburlpattern::PartType::kFullWildcard;
    case proto::UrlPatternPart::PART_TYPE_SEGMENT_WILDCARD:
      return liburlpattern::PartType::kSegmentWildcard;
    case proto::UrlPatternPart::PART_TYPE_FIXED:
      return liburlpattern::PartType::kFixed;
  }
}

TabStrip::Visibility ProtoToTabStripVisibility(
    proto::TabStrip::Visibility visibility) {
  switch (visibility) {
    case proto::TabStrip::VISIBILITY_AUTO:
      return TabStrip::Visibility::kAuto;
    case proto::TabStrip::VISIBILITY_ABSENT:
      return TabStrip::Visibility::kAbsent;
  }
}

}  // namespace

std::optional<apps::IconInfo::Purpose> SyncPurposeToIconInfoPurpose(
    sync_pb::WebAppIconInfo_Purpose purpose) {
  switch (purpose) {
    // Treat UNSPECIFIED purpose as invalid. It means a new purpose was added
    // that this client does not understand.
    case sync_pb::WebAppIconInfo_Purpose_UNSPECIFIED:
      return std::nullopt;
    case sync_pb::WebAppIconInfo_Purpose_ANY:
      return apps::IconInfo::Purpose::kAny;
    case sync_pb::WebAppIconInfo_Purpose_MASKABLE:
      return apps::IconInfo::Purpose::kMaskable;
    case sync_pb::WebAppIconInfo_Purpose_MONOCHROME:
      return apps::IconInfo::Purpose::kMonochrome;
  }
}

std::optional<std::vector<apps::IconInfo>> ParseAppIconInfos(
    const char* container_name_for_logging,
    const RepeatedIconInfosProto& manifest_icons_proto) {
  std::vector<apps::IconInfo> manifest_icons;
  for (const sync_pb::WebAppIconInfo& icon_info_proto : manifest_icons_proto) {
    apps::IconInfo icon_info;

    if (icon_info_proto.has_size_in_px()) {
      icon_info.square_size_px = icon_info_proto.size_in_px();
    }

    if (!icon_info_proto.has_url()) {
      DLOG(ERROR) << container_name_for_logging << " IconInfo has missing url";
      return std::nullopt;
    }
    icon_info.url = GURL(icon_info_proto.url());
    if (!icon_info.url.is_valid()) {
      DLOG(ERROR) << container_name_for_logging << " IconInfo has invalid url: "
                  << icon_info.url.possibly_invalid_spec();
      return std::nullopt;
    }

    if (icon_info_proto.has_purpose()) {
      std::optional<apps::IconInfo::Purpose> opt_purpose =
          SyncPurposeToIconInfoPurpose(icon_info_proto.purpose());
      if (!opt_purpose.has_value()) {
        return std::nullopt;
      }
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

std::optional<std::vector<blink::Manifest::ImageResource>>
ParseAppImageResource(const char* container_name_for_logging,
                      const RepeatedImageResourceProto& manifest_icons_proto) {
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  for (const content::proto::ImageResource& image_resource_proto :
       manifest_icons_proto) {
    blink::Manifest::ImageResource image_resource;

    if (!image_resource_proto.has_src()) {
      DLOG(ERROR) << container_name_for_logging
                  << " ImageResource has missing url";
      return std::nullopt;
    }
    image_resource.src = GURL(image_resource_proto.src());

    if (!image_resource.src.is_valid()) {
      DLOG(ERROR) << container_name_for_logging
                  << " ImageResource has invalid url: "
                  << image_resource.src.possibly_invalid_spec();
      return std::nullopt;
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

sync_pb::WebAppIconInfo AppIconInfoToSyncProto(
    const apps::IconInfo& icon_info) {
  sync_pb::WebAppIconInfo icon_info_proto;
  if (icon_info.square_size_px.has_value()) {
    icon_info_proto.set_size_in_px(icon_info.square_size_px.value());
  }
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

RunOnOsLoginMode ToRunOnOsLoginMode(proto::WebApp::RunOnOsLoginMode mode) {
  switch (mode) {
    case proto::WebApp::RUN_ON_OS_LOGIN_MODE_MINIMIZED:
      return RunOnOsLoginMode::kMinimized;
    case proto::WebApp::RUN_ON_OS_LOGIN_MODE_WINDOWED:
      return RunOnOsLoginMode::kWindowed;
    case proto::WebApp::RUN_ON_OS_LOGIN_MODE_NOT_RUN:
    default:
      return RunOnOsLoginMode::kNotRun;
  }
}

proto::WebApp::RunOnOsLoginMode ToWebAppProtoRunOnOsLoginMode(
    RunOnOsLoginMode mode) {
  switch (mode) {
    case RunOnOsLoginMode::kMinimized:
      return proto::WebApp::RUN_ON_OS_LOGIN_MODE_MINIMIZED;
    case RunOnOsLoginMode::kWindowed:
      return proto::WebApp::RUN_ON_OS_LOGIN_MODE_WINDOWED;
    case RunOnOsLoginMode::kNotRun:
      return proto::WebApp::RUN_ON_OS_LOGIN_MODE_NOT_RUN;
  }
}

std::optional<blink::SafeUrlPattern> ToUrlPattern(
    const proto::UrlPattern& proto_url_pattern) {
  blink::SafeUrlPattern url_pattern;

  for (const proto::UrlPatternPart& proto_part : proto_url_pattern.pathname()) {
    liburlpattern::Part part;

    if (!proto_part.has_part_type()) {
      DLOG(ERROR) << "WebApp UrlPattern Part has missing type";
      continue;
    }
    std::optional<liburlpattern::PartType> opt_part_type =
        ProtoToUrlPatternPartType(proto_part.part_type());
    if (!opt_part_type.has_value()) {
      return std::nullopt;
    }
    part.type = opt_part_type.value();

    if (!proto_part.has_value()) {
      DLOG(ERROR) << "WebApp UrlPattern Part has missing value";
      continue;
    }
    part.value = proto_part.value();

    if (!proto_part.has_modifier()) {
      DLOG(ERROR) << "WebApp UrlPattern Part has missing type";
      continue;
    }

    std::optional<liburlpattern::Modifier> opt_modifier =
        ProtoToUrlPatternModifier(proto_part.modifier());
    if (!opt_modifier.has_value()) {
      return std::nullopt;
    }
    part.modifier = opt_modifier.value();

    if (proto_part.has_name()) {
      part.name = proto_part.name();
    }

    if (proto_part.has_prefix()) {
      part.prefix = proto_part.prefix();
    }

    if (proto_part.has_suffix()) {
      part.suffix = proto_part.suffix();
    }

    url_pattern.pathname.push_back(std::move(part));
  }
  return url_pattern;
}

std::optional<std::vector<blink::SafeUrlPattern>> ToUrlPatterns(
    const RepeatedUrlPatternProto& proto_url_patterns) {
  std::vector<blink::SafeUrlPattern> result;
  for (const auto& proto_pattern : proto_url_patterns) {
    auto url_pattern = ToUrlPattern(proto_pattern);
    if (!url_pattern.has_value()) {
      return std::nullopt;
    }
    result.push_back(url_pattern.value());
  }
  return result;
}

proto::UrlPattern ToUrlPatternProto(const blink::SafeUrlPattern& url_pattern) {
  proto::UrlPattern url_pattern_proto;
  for (const auto& part : url_pattern.pathname) {
    proto::UrlPatternPart* url_pattern_part_proto =
        url_pattern_proto.add_pathname();

    url_pattern_part_proto->set_name(part.name);
    url_pattern_part_proto->set_prefix(part.prefix);
    url_pattern_part_proto->set_value(part.value);
    url_pattern_part_proto->set_suffix(part.suffix);

    url_pattern_part_proto->set_part_type(UrlPatternPartTypeToProto(part.type));
    url_pattern_part_proto->set_modifier(
        UrlPatternModifierToProto(part.modifier));
  }
  return url_pattern_proto;
}

std::optional<TabStrip> ProtoToTabStrip(proto::TabStrip tab_strip_proto) {
  TabStrip tab_strip;
  if (tab_strip_proto.has_home_tab_visibility()) {
    tab_strip.home_tab =
        ProtoToTabStripVisibility(tab_strip_proto.home_tab_visibility());
  } else {
    std::optional<std::vector<blink::Manifest::ImageResource>> icons =
        ParseAppImageResource("WebApp",
                              tab_strip_proto.home_tab_params().icons());
    blink::Manifest::HomeTabParams home_tab_params;
    if (!icons->empty()) {
      home_tab_params.icons = std::move(*icons);
    }

    std::optional<std::vector<blink::SafeUrlPattern>> scope_patterns =
        ToUrlPatterns(tab_strip_proto.home_tab_params().scope_patterns());
    if (!scope_patterns.has_value()) {
      return std::nullopt;
    }
    home_tab_params.scope_patterns = std::move(scope_patterns.value());

    tab_strip.home_tab = std::move(home_tab_params);
  }

  blink::Manifest::NewTabButtonParams new_tab_button_params;
  if (tab_strip_proto.new_tab_button_params().has_url()) {
    new_tab_button_params.url =
        GURL(tab_strip_proto.new_tab_button_params().url());
  }
  tab_strip.new_tab_button = new_tab_button_params;

  return tab_strip;
}

std::string RelativeManifestIdPath(webapps::ManifestId manifest_id) {
  CHECK(manifest_id.is_valid());
  // The relative id does not include the initial '/' character.
  std::string relative_manifest_id_path = manifest_id.PathForRequest();
  if (relative_manifest_id_path.starts_with("/")) {
    relative_manifest_id_path = relative_manifest_id_path.substr(1);
  }
  return relative_manifest_id_path;
}

}  // namespace web_app
