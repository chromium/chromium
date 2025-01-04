// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element_name.h"

#include <array>

#include "base/check_op.h"

namespace vr {

namespace {

// LINT.IfChange(UiElementName)
static std::array<const char*, kNumUiElementNames> g_ui_element_name_strings = {
    "kNone",
    "kRoot",
    "kWebVrRoot",
    "kWebVrViewportAwareRoot",
    "kAudioCaptureIndicator",
    "kVideoCaptureIndicator",
    "kScreenCaptureIndicator",
    "kLocationAccessIndicator",
    "kBluetoothConnectedIndicator",
    "kWebVrIndicatorTransience",
    "kWebVrIndicatorLayout",
    "kWebVrAudioCaptureIndicator",
    "kWebVrVideoCaptureIndicator",
    "kWebVrScreenCaptureIndicator",
    "kWebVrLocationAccessIndicator",
    "kWebVrBluetoothConnectedIndicator",
    "kWebVrFloor",
    "kWebVrTimeoutRoot",
    "kWebVrTimeoutSpinner",
    "kWebVrBackground",
    "kWebVrTimeoutMessage",
    "kWebVrTimeoutMessageLayout",
    "kWebVrTimeoutMessageIcon",
    "kWebVrTimeoutMessageText",
    "kWebVrTimeoutMessageButton",
    "kWebVrTimeoutMessageButtonText",
    "kWebXrExternalPromptNotification",
    "kUsbConnectedIndicator",
    "kWebXrUsbConnectedIndicator",
    "kMidiConnectedIndicator",
    "kWebXrMidiConnectedIndicator",
};
// LINT.ThenChange(//chrome/browser/vr/elements/ui_element_name.h:UiElementName)

}  // namespace

std::string UiElementNameToString(UiElementName name) {
  CHECK_GT(kNumUiElementNames, name);
  return g_ui_element_name_strings[name];
}

}  // namespace vr
