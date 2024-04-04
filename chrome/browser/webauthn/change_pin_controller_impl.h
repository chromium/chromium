// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_

#include <memory>

#include "base/supports_user_data.h"
#include "chrome/browser/webauthn/change_pin_controller.h"

namespace content {
class WebContents;
}  // namespace content

namespace syncer {
class SyncService;
}  // namespace syncer

struct AuthenticatorRequestDialogModel;
class EnclaveManager;

class ChangePinControllerImpl : public ChangePinController,
                                public base::SupportsUserData::Data {
 public:
  explicit ChangePinControllerImpl(content::WebContents* web_contents);
  ChangePinControllerImpl(const ChangePinControllerImpl&) = delete;
  ChangePinControllerImpl& operator=(const ChangePinControllerImpl&) = delete;

  ~ChangePinControllerImpl() override;

  static ChangePinControllerImpl* ForWebContents(
      content::WebContents* web_contents);

  // Checks whether changing PIN flow is available. Changing the PIN is only
  // possible when the `EnclaveManager` is ready and has a wrapped PIN.
  bool IsChangePinFlowAvailable() override;

  // Starts the change PIN flow. Returns true if the flow has started.
  bool StartChangePin() override;

 private:
  const bool enclave_enabled_;
  std::unique_ptr<AuthenticatorRequestDialogModel> model_;
  // EnclaveManager is a KeyedService.
  raw_ptr<EnclaveManager> enclave_manager_ = nullptr;
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_
