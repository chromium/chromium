// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_URL_HANDLER_LAUNCH_PARAMS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_URL_HANDLER_LAUNCH_PARAMS_H_

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "url/gurl.h"

namespace web_app {

// UrlHandlerSavedChoice values represent the possible user preferences for
// similar URL handler launches in the future.
// These values are serialized out to local state prefs. Do not rearrange or
// change the underlying values.
// For matching URL handlers, an in-app choice is preferred to a none choice,
// which is preferred to an in-browser choice. These values are ordered
// intentionally to represent these semantics.
enum class UrlHandlerSavedChoice {
  // Similar matches should open in a normal browser tab without prompting.
  kInBrowser,
  // No preference. User will be prompted again with matches.
  kNone,
  // Similar matches should open in an app window without prompting.
  kInApp,
  kMax = kInApp
};

// |UrlHandlerLaunchParams| contains a profile path, an AppId and the
// launch URL needed to launch a web app through commandline arguments.
// |saved_choice| can be used to determine if a UI prompt needs to be shown to
// the user before launch.
struct UrlHandlerLaunchParams {
  UrlHandlerLaunchParams();
  UrlHandlerLaunchParams(const base::FilePath& profile_path,
                         const AppId& app_id,
                         const GURL& url,
                         UrlHandlerSavedChoice saved_choice,
                         const base::Time& saved_choice_timestamp);

  UrlHandlerLaunchParams(const UrlHandlerLaunchParams& other);

  ~UrlHandlerLaunchParams();

  base::FilePath profile_path;
  AppId app_id;
  GURL url;
  UrlHandlerSavedChoice saved_choice = UrlHandlerSavedChoice::kNone;
  base::Time saved_choice_timestamp;
};

bool operator==(const UrlHandlerLaunchParams& launch_params1,
                const UrlHandlerLaunchParams& launch_params2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_URL_HANDLER_LAUNCH_PARAMS_H_
