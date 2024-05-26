// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_

#include <memory>

#include "base/supports_user_data.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/change_pin_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace syncer {
class SyncService;
}  // namespace syncer

struct AuthenticatorRequestDialogModel;
class EnclaveManager;

// ChangePinControllerImpl controls the Google Password Manager PIN flow.
// It relies heavily on the AuthenticatorRequestDialogModel's state machine.
// The following diagram should help understand the flow. To edit the dialog,
// paste it in asciiflow.com.
//
//         ChangePinController                   AuthenticatorRequestDialogModel
// ┌─────────────────────────────────┐             ┌───────────────────────┐
// │                                 │             │                       │
// │ StartChangePin ─────────────────┼─────────────┼───────────┐           │
// │                                 │             │           │           │
// │                                 │ Cancelled   │           ▼           │
// │ OnRecoverSecurityDomainClosed ◄─┼─────────────┼── kGpmReauthAccount   │
// │                                 │             │           │           │
// │        ┌────────────────────────┼─────────────┼───────────┘           │
// │        │                        │ Success     │                       │
// │        ▼                        │             │                       │
// │ OnReauthComplete ───────────────┼─────────────┼───────────┐           │
// │                                 │             │           │           │
// │                                 │ Cancelled   │           ▼           │
// │ CancelAuthenticatorRequest ◄────┼─────────────┼── kGPMCreatePin*      │
// │                                 │             │           │           │
// │                                 │ PIN entered │           │           │
// │        ┌────────────────────────┼─────────────┼───────────┘           │
// │        │                        │             │                       │
// │        ▼                        │             └───────────────────────┘
// │ OnGPMPinEntered ────────────────┼─────┐         EnclaveManager
// │                                 │     │       ┌────────────────┐
// │                                 │     │       │                │
// │                                 │     │       │                │
// │                                 │     └───────┼─► ChangePIN    │
// │                success/failure  │             │      │         │
// │        ┌────────────────────────┼─────────────┼──────┘         │
// │        ▼                        │             │                │
// │                                 │             │                │
// │ OnGpmPinChanged                 │             │                │
// │                                 │             │                │
// └─────────────────────────────────┘             └────────────────┘
//
// *: this can also be kGPMCreateArbitraryPin when the user switches the step in
// the view.
class ChangePinControllerImpl
    : public ChangePinController,
      public content::WebContentsUserData<ChangePinControllerImpl>,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  ChangePinControllerImpl(const ChangePinControllerImpl&) = delete;
  ChangePinControllerImpl& operator=(const ChangePinControllerImpl&) = delete;

  ~ChangePinControllerImpl() override;

  // Checks whether changing PIN flow is available. Changing the PIN is only
  // possible when the `EnclaveManager` is ready and has a wrapped PIN.
  bool IsChangePinFlowAvailable() override;

  // Starts the change PIN flow. Returns true if the flow has started.
  void StartChangePin(SuccessCallback callback) override;

  // AuthenticatorRequestDialogModel::Observer
  void CancelAuthenticatorRequest() override;
  void OnReauthComplete(std::string rapt) override;
  void OnRecoverSecurityDomainClosed() override;
  void OnGPMPinEntered(const std::u16string& pin) override;
  void OnGPMPinOptionChanged(bool is_arbitrary) override;

 private:
  explicit ChangePinControllerImpl(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChangePinControllerImpl>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  void OnGpmPinChanged(bool success);
  void Reset(bool success);

  const bool enclave_enabled_;
  std::unique_ptr<AuthenticatorRequestDialogModel> model_;
  SuccessCallback notify_pin_change_callback_;
  // EnclaveManager is a KeyedService.
  raw_ptr<EnclaveManager> enclave_manager_ = nullptr;
  // SyncService is a KeyedService.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
  std::optional<std::string> rapt_ = std::nullopt;

  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observation_{this};

  base::WeakPtrFactory<ChangePinControllerImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_
