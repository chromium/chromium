// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FLAGS_UI_FLAGS_UI_CONSTANTS_H_
#define COMPONENTS_FLAGS_UI_FLAGS_UI_CONSTANTS_H_

#include "build/build_config.h"

namespace flags_ui {

// Message handlers.
// Must match the constants used in the resource files.
extern const char kEnableExperimentalFeature[];
extern const char kRequestExperimentalFeatures[];
extern const char kSetOriginListFlag[];
extern const char kSetStringFlag[];
extern const char kResetAllFlags[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kCrosUrlFlagsRedirect[];
#endif
extern const char kRestartBrowser[];

// Other values.
// Must match the constants used in the resource files.
extern const char kFlagsRestartButton[];
extern const char kFlagsRestartNotice[];
extern const char kNeedsRestart[];
extern const char kShowBetaChannelPromotion[];
extern const char kShowDevChannelPromotion[];
extern const char kShowOwnerWarning[];
extern const char kSupportedFeatures[];
extern const char kUnsupportedFeatures[];
extern const char kVersion[];
extern const char kShowSystemFlagsLink[];

}  // namespace flags_ui

#endif  // COMPONENTS_FLAGS_UI_FLAGS_UI_CONSTANTS_H_
