// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/url_handler_launch_params.h"

#include "base/check.h"

namespace web_app {

UrlHandlerLaunchParams::UrlHandlerLaunchParams(
    const base::FilePath& profile_path,
    const AppId& app_id,
    const GURL& url)
    : profile_path(profile_path), app_id(app_id), url(url) {
  DCHECK(!profile_path.empty());
  DCHECK(!app_id.empty());
  DCHECK(url.is_valid());
}

UrlHandlerLaunchParams::~UrlHandlerLaunchParams() = default;

}  // namespace web_app
