// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_

#include <string>

#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// These identifiers serve as stable, semantic identifiers for UI elements.
enum UiElementName {
  kNone = 0,
  kRoot,
  kWebVrRoot,
  kWebVrViewportAwareRoot,
  kAudioCaptureIndicator,
  kVideoCaptureIndicator,
  kScreenCaptureIndicator,
  kLocationAccessIndicator,
  kBluetoothConnectedIndicator,
  kWebVrIndicatorTransience,
  kWebVrIndicatorLayout,
  kWebVrAudioCaptureIndicator,
  kWebVrVideoCaptureIndicator,
  kWebVrScreenCaptureIndicator,
  kWebVrLocationAccessIndicator,
  kWebVrBluetoothConnectedIndicator,
  kWebVrFloor,
  kWebVrTimeoutRoot,
  kWebVrTimeoutSpinner,
  kWebVrBackground,
  kWebVrTimeoutMessage,
  kWebVrTimeoutMessageLayout,
  kWebVrTimeoutMessageIcon,
  kWebVrTimeoutMessageText,
  kWebVrTimeoutMessageButton,
  kWebVrTimeoutMessageButtonText,
  kWebXrExternalPromptNotification,
  kUsbConnectedIndicator,
  kWebXrUsbConnectedIndicator,
  kMidiConnectedIndicator,
  kWebXrMidiConnectedIndicator,

  // This must be last.
  kNumUiElementNames,
};

VR_UI_EXPORT std::string UiElementNameToString(UiElementName name);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_NAME_H_
