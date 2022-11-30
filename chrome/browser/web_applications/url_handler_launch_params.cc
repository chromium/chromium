// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/url_handler_launch_params.h"

#include "base/check.h"

namespace web_app {

UrlHandlerLaunchParams::UrlHandlerLaunchParams() = default;

UrlHandlerLaunchParams::UrlHandlerLaunchParams(
    const base::FilePath& profile_path,
    const AppId& app_id,
    const GURL& url,
    const UrlHandlerSavedChoice saved_choice,
    const base::Time& saved_choice_timestamp)
    : profile_path(profile_path),
      app_id(app_id),
      url(url),
      saved_choice(saved_choice),
      saved_choice_timestamp(saved_choice_timestamp) {
  DCHECK(!profile_path.empty());
  DCHECK(!app_id.empty());
  DCHECK(url.is_valid());
}

UrlHandlerLaunchParams::UrlHandlerLaunchParams(
    const UrlHandlerLaunchParams& other) = default;

UrlHandlerLaunchParams::~UrlHandlerLaunchParams() = default;

bool operator==(const UrlHandlerLaunchParams& launch_params1,
                const UrlHandlerLaunchParams& launch_params2) {
  return launch_params1.profile_path == launch_params2.profile_path &&
         launch_params1.app_id == launch_params2.app_id &&
         launch_params1.url == launch_params2.url &&
         launch_params1.saved_choice == launch_params2.saved_choice &&
         launch_params1.saved_choice_timestamp ==
             launch_params2.saved_choice_timestamp;
}

}  // namespace web_app
