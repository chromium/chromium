// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_H_

#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}  // namespace content

class EnclaveManager;

class ChangePinController {
 public:
  static ChangePinController* ForWebContents(
      content::WebContents* web_contents);

  virtual ~ChangePinController();

  // Checks whether changing PIN flow is available. Changing the PIN is only
  // possible when the `EnclaveManager` is ready and has a wrapped PIN.
  virtual bool IsChangePinFlowAvailable();

  // Starts the change PIN flow. Returns true if the flow has started.
  virtual bool StartChangePin();

  static void set_instance_for_testing(ChangePinController* controller) {
    instance_for_testing_ = controller;
  }

 private:
  // Owned by the test:
  static ChangePinController* instance_for_testing_;
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_H_
