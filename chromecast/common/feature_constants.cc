// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/feature_constants.h"

namespace chromecast {
namespace feature {

const char kEnableTrackControlAppRendererFeatureUse[] =
    "track_control_renderer_feature_use";
const char kEnablePlayready[] = "playready";
const char kEnableDevMode[] = "dev_mode";
const char kDevModeOrigin[] = "dev_mode_origin";
const char kEnableAccessibilityControls[] = "accessibility_controls";
const char kEnableSystemGestures[] = "system_gestures";
const char kEnableWindowControls[] = "enable_window_controls";
const char kEnableSettingsUiMojo[] = "enable_settings_ui_mojo";
const char kDisableBackgroundTabTimerThrottle[] =
    "disable_background_tab_timer_throttle";
const char kDisableBackgroundSuspend[] = "disable_background_suspend";
const char kEnableAssistantMessagePipe[] = "enable_assistant_message_pipe";
const char kEnableDemoStandaloneMode[] = "enable_demo_standalone_mode";

const char kKeyAppId[] = "app_id";
const char kKeyAllowInsecureContent[] = "allow_insecure_content";

}  // namespace feature
}  // namespace chromecast
