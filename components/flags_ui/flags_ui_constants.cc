// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/flags_ui_constants.h"

#include "build/build_config.h"

namespace flags_ui {

// Message handlers.
const char kEnableExperimentalFeature[] = "enableExperimentalFeature";
const char kRequestExperimentalFeatures[] = "requestExperimentalFeatures";
const char kSetOriginListFlag[] = "setOriginListFlag";
const char kSetStringFlag[] = "setStringFlag";
const char kResetAllFlags[] = "resetAllFlags";
#if BUILDFLAG(IS_CHROMEOS)
const char kCrosUrlFlagsRedirect[] = "crosUrlFlagsRedirect";
#endif
const char kRestartBrowser[] = "restartBrowser";

// Other values.
const char kFlagsRestartButton[] = "flagsRestartButton";
const char kFlagsRestartNotice[] = "flagsRestartNotice";
const char kNeedsRestart[] = "needsRestart";
const char kShowBetaChannelPromotion[] = "showBetaChannelPromotion";
const char kShowDevChannelPromotion[] = "showDevChannelPromotion";
const char kShowOwnerWarning[] = "showOwnerWarning";
const char kSupportedFeatures[] = "supportedFeatures";
const char kUnsupportedFeatures[] = "unsupportedFeatures";
const char kVersion[] = "version";
const char kShowSystemFlagsLink[] = "showSystemFlagsLink";

}  // namespace flags_ui
