// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_

#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/model/web_vr_model.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "components/security_state/core/security_state.h"

namespace vr {

// The browser communicates state changes to the VR UI via this interface.
// A GL thread would also implement this interface to provide a convenient way
// to call these methods from the main thread.
class VR_BASE_EXPORT BrowserUiInterface {
 public:
  virtual ~BrowserUiInterface() {}

  virtual void SetCapturingState(
      const CapturingStateModel& active_capturing,
      const CapturingStateModel& background_capturing,
      const CapturingStateModel& potential_capturing) = 0;

  // Shows (or hides) a notification in-headset that the user should respond to
  // a prompt on a separate display. Only one such notification is displayed at
  // a time. Only displayed on desktop.
  virtual void SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType prompt) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_
