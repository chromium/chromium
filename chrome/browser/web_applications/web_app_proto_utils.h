// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTO_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTO_UTILS_H_

#include <optional>
#include <vector>

#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_tab_strip.pb.h"
#include "chrome/browser/web_applications/proto/web_app_url_pattern.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "third_party/blink/public/common/safe_url_pattern.h"

namespace apps {
struct IconInfo;
}

namespace web_app {

enum class RunOnOsLoginMode;

using RepeatedIconInfosProto =
    const ::google::protobuf::RepeatedPtrField<::sync_pb::WebAppIconInfo>;

using RepeatedImageResourceProto =
    const ::google::protobuf::RepeatedPtrField<content::proto::ImageResource>;

std::optional<std::vector<apps::IconInfo>> ParseAppIconInfos(
    const char* container_name_for_logging,
    const RepeatedIconInfosProto& manifest_icons_proto);

std::optional<std::vector<blink::Manifest::ImageResource>>
ParseAppImageResource(const char* container_name_for_logging,
                      const RepeatedImageResourceProto& manifest_icons_proto);

// Use the given |icon_info| to populate a |WebAppIconInfo| sync proto.
sync_pb::WebAppIconInfo AppIconInfoToSyncProto(const apps::IconInfo& icon_info);

// Use the given |image_resource| to populate a |ImageResource| proto.
content::proto::ImageResource AppImageResourceToProto(
    const blink::Manifest::ImageResource& image_resource);

RunOnOsLoginMode ToRunOnOsLoginMode(WebAppProto::RunOnOsLoginMode mode);

WebAppProto::RunOnOsLoginMode ToWebAppProtoRunOnOsLoginMode(
    RunOnOsLoginMode mode);

std::optional<blink::SafeUrlPattern> ToUrlPattern(
    const proto::UrlPattern& proto_url_pattern);

proto::UrlPattern ToUrlPatternProto(const blink::SafeUrlPattern& url_pattern);

std::optional<TabStrip> ProtoToTabStrip(proto::TabStrip tab_strip_proto);

// Compute the relative manifest ID path as used in the sync proto.
std::string RelativeManifestIdPath(webapps::ManifestId manifest_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTO_UTILS_H_
