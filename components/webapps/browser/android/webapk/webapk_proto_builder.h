// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_PROTO_BUILDER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_PROTO_BUILDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace webapps {

class WebappIcon;

// Populates webapk::WebApk and returns it.
// Must be called on a worker thread because it encodes an SkBitmap.
// The splash icon can be passed either via |icon_url_to_murmur2_hash| or via
// |splash_icon| parameter. |splash_icon| parameter is only used when the
// splash icon URL is unknown.
std::unique_ptr<std::string> BuildProtoInBackground(
    const webapps::ShortcutInfo& shortcut_info,
    const GURL& app_key,
    std::unique_ptr<webapps::WebappIcon> primary_icon,
    std::unique_ptr<webapps::WebappIcon> splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<GURL, std::unique_ptr<WebappIcon>> icons,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    std::vector<WebApkUpdateReason> update_reasons);

// Asynchronously builds the WebAPK proto on a background thread.
// Runs |callback| on the calling thread when complete.
void BuildProto(
    const webapps::ShortcutInfo& shortcut_info,
    const GURL& app_key,
    std::unique_ptr<webapps::WebappIcon> primary_icon,
    std::unique_ptr<webapps::WebappIcon> splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<GURL, std::unique_ptr<WebappIcon>> icons,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    base::OnceCallback<void(std::unique_ptr<std::string>)> callback);

// Builds the WebAPK proto for an update request and stores it to
// |update_request_path|. Returns whether the proto was successfully written to
// disk.
bool StoreUpdateRequestToFileInBackground(
    const base::FilePath& update_request_path,
    const webapps::ShortcutInfo& shortcut_info,
    const GURL& app_key,
    std::unique_ptr<webapps::WebappIcon> primary_icon,
    std::unique_ptr<webapps::WebappIcon> splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<GURL, std::unique_ptr<WebappIcon>> icons,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    std::vector<WebApkUpdateReason> update_reasons);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_PROTO_BUILDER_H_
