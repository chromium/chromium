// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webapks/webapks_handler.h"

#include <string>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "content/public/browser/web_ui.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "ui/android/color_utils_android.h"
#include "ui/gfx/color_utils.h"

WebApksHandler::WebApksHandler()
    : delegate_(base::BindRepeating(&WebApksHandler::OnWebApkInfoRetrieved,
                                    base::Unretained(this))) {}

WebApksHandler::~WebApksHandler() {}

void WebApksHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestWebApksInfo",
      base::BindRepeating(&WebApksHandler::HandleRequestWebApksInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestWebApkUpdate",
      base::BindRepeating(&WebApksHandler::HandleRequestWebApkUpdate,
                          base::Unretained(this)));
}

void WebApksHandler::HandleRequestWebApksInfo(const base::Value::List& args) {
  AllowJavascript();
  delegate_.RetrieveWebApks();
}

void WebApksHandler::HandleRequestWebApkUpdate(const base::Value::List& args) {
  AllowJavascript();
  for (const auto& val : args) {
    if (val.is_string())
      ShortcutHelper::SetForceWebApkUpdate(val.GetString());
  }
}

void WebApksHandler::OnWebApkInfoRetrieved(const WebApkInfo& webapk_info) {
  if (!IsJavascriptAllowed())
    return;
  base::Value::Dict result;
  result.Set("name", webapk_info.name);
  result.Set("shortName", webapk_info.short_name);
  result.Set("packageName", webapk_info.package_name);
  result.Set("id", webapk_info.id);
  result.Set("shellApkVersion", webapk_info.shell_apk_version);
  result.Set("versionCode", webapk_info.version_code);
  result.Set("uri", webapk_info.uri);
  result.Set("scope", webapk_info.scope);
  result.Set("manifestUrl", webapk_info.manifest_url);
  result.Set("manifestStartUrl", webapk_info.manifest_start_url);
  result.Set("manifestId", webapk_info.manifest_id);
  result.Set("displayMode", blink::DisplayModeToString(webapk_info.display));
  result.Set("orientation", blink::WebScreenOrientationLockTypeToString(
                                webapk_info.orientation));
  result.Set("themeColor",
             ui::OptionalSkColorToString(webapk_info.theme_color));
  result.Set("backgroundColor",
             ui::OptionalSkColorToString(webapk_info.background_color));
  result.Set("darkThemeColor",
             ui::OptionalSkColorToString(webapk_info.dark_theme_color));
  result.Set("darkBackgroundColor",
             ui::OptionalSkColorToString(webapk_info.dark_background_color));
  result.Set(
      "lastUpdateCheckTimeMs",
      webapk_info.last_update_check_time.InMillisecondsFSinceUnixEpoch());
  result.Set(
      "lastUpdateCompletionTimeMs",
      webapk_info.last_update_completion_time.InMillisecondsFSinceUnixEpoch());
  result.Set("relaxUpdates", webapk_info.relax_updates);
  result.Set("backingBrowser", webapk_info.backing_browser_package_name);
  result.Set("isBackingBrowser", webapk_info.is_backing_browser);
  result.Set("updateStatus", webapk_info.is_backing_browser
                                 ? webapk_info.update_status
                                 : "Current browser doesn't own this WebAPK.");
  FireWebUIListener("web-apk-info", result);
}
