// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/change_pin_controller.h"
#include "content/public/browser/document_user_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

struct AuthenticatorRequestDialogModel;
class EnclaveManager;

// ChangePinControllerImpl controls the Google Password Manager PIN flow.
// It relies heavily on the AuthenticatorRequestDialogModel's state machine.
// The following diagram should help understand the flow. To edit the dialog,
// paste it in asciiflow.com.
//
//         ChangePinController                   AuthenticatorRequestDialogModel
// ┌─────────────────────────────────┐             ┌─────────────────────────┐
// │                                 │             │                         │
// │ StartChangePin ─────────────────┼─────────────┼───────────┐             │
// │                                 │             │           │             │
// │                                 │ Cancelled   │           ▼             │
// │ OnRecoverSecurityDomainClosed ◄─┼─────────────┼── kGpmReauthForPinReset │
// │                                 │             │           │             │
// │        ┌────────────────────────┼─────────────┼───────────┘             │
// │        │                        │ Success     │                         │
// │        ▼                        │             │                         │
// │ OnReauthComplete ───────────────┼─────────────┼───────────┐             │
// │                                 │             │           │             │
// │                                 │ Cancelled   │           ▼             │
// │ CancelAuthenticatorRequest ◄────┼─────────────┼── kGPMChangePin*        │
// │                                 │             │           │             │
// │                                 │ PIN entered │           │             │
// │        ┌────────────────────────┼─────────────┼───────────┘             │
// │        │                        │             │                         │
// │        ▼                        │             └─────────────────────────┘
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
// *: this can also be kGPMChangeArbitraryPin when the user switches the step in
// the view.
class ChangePinControllerImpl
    : public ChangePinController,
      public content::DocumentUserData<ChangePinControllerImpl>,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  enum class ChangePinEvent {
    kFlowStartedFromSettings = 0,
    kFlowStartedFromPinDialog = 1,
    kReauthCompleted = 2,
    kReauthCancelled = 3,
    kNewPinEntered = 4,
    kNewPinCancelled = 5,
    kCompletedSuccessfully = 6,
    kFailed = 7,
    kMaxValue = kFailed,
  };

  ChangePinControllerImpl(const ChangePinControllerImpl&) = delete;
  ChangePinControllerImpl& operator=(const ChangePinControllerImpl&) = delete;

  ~ChangePinControllerImpl() override;

  // ChangePinController:
  void IsChangePinFlowAvailable(PinAvailableCallback callback) override;
  void StartChangePin(SuccessCallback callback) override;

  // AuthenticatorRequestDialogModel::Observer
  void CancelAuthenticatorRequest() override;
  void OnReauthComplete(std::string rapt) override;
  void OnRecoverSecurityDomainClosed() override;
  void OnGPMPinEntered(const std::u16string& pin) override;
  void OnGPMPinOptionChanged(bool is_arbitrary) override;

  static void RecordHistogram(ChangePinEvent event);

 private:
  explicit ChangePinControllerImpl(content::RenderFrameHost* render_frame_host);
  friend class content::DocumentUserData<ChangePinControllerImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  void OnGpmPinChanged(bool success);
  void Reset(bool success);
  void NotifyPinAvailability(PinAvailableCallback callback);

  const bool enclave_enabled_;
  scoped_refptr<AuthenticatorRequestDialogModel> model_;
  SuccessCallback notify_pin_change_callback_;
  // EnclaveManager is a KeyedService.
  raw_ptr<EnclaveManager> enclave_manager_ = nullptr;
  std::optional<std::string> rapt_ = std::nullopt;

  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observation_{this};

  base::WeakPtrFactory<ChangePinControllerImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHANGE_PIN_CONTROLLER_IMPL_H_
