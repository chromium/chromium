// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}  // namespace content

class EnclaveManager;

class ChangePinController {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using PinAvailableCallback = base::OnceCallback<void(bool)>;

  static ChangePinController* ForWebContents(
      content::WebContents* web_contents);

  virtual ~ChangePinController();

  // Checks whether changing PIN flow is available. Changing the PIN is only
  // possible when the `EnclaveManager` is ready and has a wrapped PIN. If
  // `EnclaveManager` is not ready, it will be loaded.
  virtual void IsChangePinFlowAvailable(PinAvailableCallback callback) = 0;

  // Starts the change PIN flow. The callback is run once the flow is completed.
  virtual void StartChangePin(SuccessCallback callback) = 0;

  static void set_instance_for_testing(ChangePinController* controller) {
    instance_for_testing_ = controller;
  }

 private:
  // Owned by the test:
  static ChangePinController* instance_for_testing_;
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_H_
