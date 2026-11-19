// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_CHANGE_PIN_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_CHANGE_PIN_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/change_pin_controller.h"
#include "content/public/browser/document_user_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace device_reauth {
class DeviceAuthAvailabilityChecker;
class DeviceAuthenticator;
}  // namespace device_reauth

namespace signin {
class IdentityManager;
}  // namespace signin

namespace webauthn {

class GpmPinService;

// LocalChangePinControllerImpl controls the Google Password Manager PIN change
// flow from Settings when the local browser-based GPM passkey authenticator is
// active. Unlike ChangePinControllerImpl (which always uses GAIA ReAuth for the
// cloud enclave), it prefers OS user verification via DeviceAuthenticator and
// falls back to GAIA ReAuth via AuthenticatorRequestDialogModel when OS user
// verification is unavailable. To edit the diagram, paste it in asciiflow.com.
//
//    LocalChangePinControllerImpl               DeviceAuthenticator (OS UV)
// ┌──────────────────────────────────┐           ┌────────────────────────────┐
// │                                  │ OS UV     │                            │
// │ StartChangePin ──────────────────┼─available─┼─► AuthenticateWithMessage  │
// │   │                              │           │              │             │
// │   │                       result │           │              │             │
// │   │    ┌─────────────────────────┼───────────┼──────────────┘             │
// │   │    ▼                         │           └────────────────────────────┘
// │   │  OnOsUvCompleted             │
// │   │  (failure resets)            │         AuthenticatorRequestDialogModel
// │   │    │                 success │           ┌────────────────────────────┐
// │   │    └─────────────────────────┼───────────┼─────────────────────────┐  │
// │   │                              │           │                         │  │
// │   │  OS UV unavailable           │           │                         │  │
// │   └──────────────────────────────┼───────────┼─► kGPMReauthForPinReset │  │
// │                                  │ Cancelled │          │              │  │
// │ OnGPMRecoverSecurityDomainClosed◄┼───────────┼──────────┤              │  │
// │                                  │  Success  │          │              │  │
// │        ┌─────────────────────────┼───────────┼──────────┘              │  │
// │        ▼                         │           │                         ▼  │
// │ OnGPMReauthComplete ─────────────┼───────────┼─► kGPMChangePin* ◄──────┘  │
// │                                  │ Cancelled │          │                 │
// │ CancelAuthenticatorRequest ◄─────┼───────────┼──────────┤                 │
// │                                  │PIN entered│          │                 │
// │        ┌─────────────────────────┼───────────┼──────────┘                 │
// │        ▼                         │           └────────────────────────────┘
// │ OnGPMPinEntered ─────────────────┼────┐             GpmPinService
// │                                  │    │      ┌────────────────────────────┐
// │                                  │    └──────┼─► SetPin                   │
// │                 success/failure  │           │     │                      │
// │        ┌─────────────────────────┼───────────┼─────┘                      │
// │        ▼                         │           │                            │
// │ OnGpmPinChanged                  │           └────────────────────────────┘
// │                                  │
// └──────────────────────────────────┘
//
// *: this can also be kGPMChangeArbitraryPin when the user switches the step in
// the view via OnGPMPinOptionChanged.
class LocalChangePinControllerImpl
    : public ChangePinController,
      public content::DocumentUserData<LocalChangePinControllerImpl>,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  LocalChangePinControllerImpl(const LocalChangePinControllerImpl&) = delete;
  LocalChangePinControllerImpl& operator=(const LocalChangePinControllerImpl&) =
      delete;
  ~LocalChangePinControllerImpl() override;

  // ChangePinController:
  void IsChangePinFlowAvailable(PinAvailableCallback callback) override;
  void StartChangePin(SuccessCallback callback) override;

  // AuthenticatorRequestDialogModel::Observer:
  void CancelAuthenticatorRequest() override;
  void OnGPMReauthComplete(std::string rapt) override;
  void OnGPMRecoverSecurityDomainClosed() override;
  void OnGPMPinEntered(const std::u16string& pin) override;
  void OnGPMPinOptionChanged(bool is_arbitrary) override;

 private:
  explicit LocalChangePinControllerImpl(
      content::RenderFrameHost* render_frame_host);
  friend class content::DocumentUserData<LocalChangePinControllerImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  void OnOsUvCompleted(bool success);
  void OnGpmPinChanged(bool success);

  std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker>
  CreateAvailabilityChecker();
  std::unique_ptr<device_reauth::DeviceAuthenticator>
  CreateDeviceAuthenticator();

  // May be null (e.g. for Guest or AshInternals profiles, or in tests).
  const raw_ptr<GpmPinService> gpm_pin_service_;
  // May be null (e.g. for Guest or AshInternals profiles, or in tests).
  const raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<AuthenticatorRequestDialogModel> model_;
  SuccessCallback notify_pin_change_callback_;

  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;

  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observation_{this};

  base::WeakPtrFactory<LocalChangePinControllerImpl> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_LOCAL_CHANGE_PIN_CONTROLLER_IMPL_H_
