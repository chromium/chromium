// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ANDROID_WEBAPPS_UTILS_H_
#define COMPONENTS_WEBAPPS_ANDROID_WEBAPPS_UTILS_H_

class GURL;

namespace content {
class BrowserContext;
}

namespace webapps {

class WebappsUtils {
 public:
  WebappsUtils() = delete;
  WebappsUtils& operator=(const WebappsUtils&) = delete;
  WebappsUtils(const WebappsUtils&) = delete;

  // Returns true if there is an installed WebAPK which can handle |url|.
  static bool IsWebApkInstalled(content::BrowserContext* browser_context,
                                const GURL& url);
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_ANDROID_WEBAPPS_UTILS_H_
