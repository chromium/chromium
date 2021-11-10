// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"

namespace web_app {
SystemWebAppDelegate::SystemWebAppDelegate(
    const SystemAppType type,
    const std::string& internal_name,
    const GURL& install_url,
    Profile* profile,
    const OriginTrialsMap& origin_trials_map)
    : type_(type),
      internal_name_(internal_name),
      install_url_(install_url),
      profile_(profile),
      origin_trials_map_(origin_trials_map) {}

SystemWebAppDelegate::~SystemWebAppDelegate() = default;

}  // namespace web_app
