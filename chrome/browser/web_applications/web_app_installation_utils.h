// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALLATION_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALLATION_UTILS_H_

struct WebApplicationInfo;

namespace web_app {

class WebApp;

// Updates |web_app| using |web_app_info|
void SetWebAppManifestFields(const WebApplicationInfo& web_app_info,
                             WebApp& web_app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALLATION_UTILS_H_
