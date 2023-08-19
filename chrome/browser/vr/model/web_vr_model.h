// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_WEB_VR_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_WEB_VR_MODEL_H_

#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

// As we wait for WebVR frames, we may pass through the following states.
enum WebVrState {
  // We are not awaiting a WebVR frame.
  kWebVrNoTimeoutPending = 0,
  kWebVrAwaitingFirstFrame,
  // We are awaiting a WebVR frame, and we will soon exceed the amount of time
  // that we're willing to wait. In this state, it could be appropriate to show
  // an affordance to the user to let them know that WebVR is delayed (eg, this
  // would be when we might show a spinner or progress bar).
  kWebVrTimeoutImminent,
  // In this case the time allotted for waiting for the first WebVR frame has
  // been entirely exceeded. This would, for example, be an appropriate time to
  // show "sad tab" UI to allow the user to bail on the WebVR content.
  kWebVrTimedOut,
  // We've received our first WebVR frame and are in WebVR presentation mode.
  kWebVrPresenting,
};

// Type of permission prompt visible out-of-headset on a desktop display that
// the user may want to respond to. Currently this can't differentiate between
// specific permission requests, in the future we may add kPromptBluetooth etc.
enum class ExternalPromptNotificationType {
  kPromptNone = 0,
  kPromptGenericPermission,
};

struct VR_BASE_EXPORT WebVrModel {
  WebVrState state = kWebVrNoTimeoutPending;
  bool has_received_permissions = false;
  bool IsImmersiveWebXrVisible() const {
    return state == kWebVrPresenting &&
           external_prompt_notification ==
               ExternalPromptNotificationType::kPromptNone;
  }

  // Desktop permission requests.
  ExternalPromptNotificationType external_prompt_notification =
      ExternalPromptNotificationType::kPromptNone;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_WEB_VR_MODEL_H_
