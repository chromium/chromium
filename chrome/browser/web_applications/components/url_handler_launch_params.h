// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_LAUNCH_PARAMS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_LAUNCH_PARAMS_H_

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "url/gurl.h"

namespace web_app {

// |UrlHandlerLaunchParams| contains a profile path, an AppId and the
// launch URL needed to launch a web app through commandline arguments.
struct UrlHandlerLaunchParams {
  UrlHandlerLaunchParams(const base::FilePath& profile_path,
                         const AppId& app_id,
                         const GURL& url);

  ~UrlHandlerLaunchParams();

  base::FilePath profile_path;
  AppId app_id;
  GURL url;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_URL_HANDLER_LAUNCH_PARAMS_H_
