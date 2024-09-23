// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/vr/elements/ui_element_name.h"

#include "base/check_op.h"

namespace vr {

namespace {

static const char* g_ui_element_name_strings[] = {
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

static_assert(
    kNumUiElementNames == std::size(g_ui_element_name_strings),
    "Mismatch between the kUiElementName enum and the corresponding array "
    "of strings.");

}  // namespace

std::string UiElementNameToString(UiElementName name) {
  DCHECK_GT(kNumUiElementNames, name);
  return g_ui_element_name_strings[name];
}

}  // namespace vr
