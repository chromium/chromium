// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_launch_params.h"

namespace web_app {

WebAppLaunchParams::WebAppLaunchParams() = default;

WebAppLaunchParams::WebAppLaunchParams(const WebAppLaunchParams&) = default;

WebAppLaunchParams::WebAppLaunchParams(WebAppLaunchParams&&) noexcept = default;

WebAppLaunchParams::~WebAppLaunchParams() = default;

WebAppLaunchParams& WebAppLaunchParams::operator=(const WebAppLaunchParams&) =
    default;

WebAppLaunchParams& WebAppLaunchParams::operator=(
    WebAppLaunchParams&&) noexcept = default;

}  // namespace web_app
