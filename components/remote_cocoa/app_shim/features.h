// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_FEATURES_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_FEATURES_H_

#include "base/feature_list.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

namespace remote_cocoa::features {
REMOTE_COCOA_APP_SHIM_EXPORT
BASE_DECLARE_FEATURE(kImmersiveFullscreenSpaceSwitchMitigation);
REMOTE_COCOA_APP_SHIM_EXPORT
BASE_DECLARE_FEATURE(kImmersiveFullscreenOverlayWindowDebug);
REMOTE_COCOA_APP_SHIM_EXPORT
BASE_DECLARE_FEATURE(kFullscreenAlwaysShowTrafficLights);
}  // namespace remote_cocoa::features

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_FEATURES_H_
