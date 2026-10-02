// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_UTILS_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_UTILS_H_

#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace webapps {
enum class WebApkInstallResult;

class WebappsUtils {
 public:
  WebappsUtils() = delete;
  WebappsUtils& operator=(const WebappsUtils&) = delete;
  WebappsUtils(const WebappsUtils&) = delete;

  // Returns true if there is an installed WebAPK which can handle |url|.
  static bool IsWebApkInstalled(const GURL& url);

  // Returns whether the format of the URLs in the Web Manifest is WebAPK
  // compatible.
  static bool AreWebManifestUrlsWebApkCompatible(
      const blink::mojom::Manifest& manifest);

  // Shows toast notifying user of the result of a WebAPK install if the
  // installation was not successful.
  static void ShowWebApkInstallResultToast(webapps::WebApkInstallResult result);

  // Returns whether installation of auto-minted TWA is enabled.
  static bool IsAutoMintedTwaEnabled();

  // Removes the navigation entries that come before the user entered scope, so
  // that going back does not leave the web app. Does nothing if the current
  // entry is outside scope or if there is nothing to remove.
  static void PrunePreScopeNavigationHistory(content::WebContents* web_contents,
                                             const GURL& scope);
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPPS_UTILS_H_
