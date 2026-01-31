// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_UI_READINESS_BARRIER_H_
#define CHROME_BROWSER_WEBAUTHN_UI_READINESS_BARRIER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/password_credential_fetcher.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {
class FidoRequestHandlerBase;
}  // namespace device

// UiReadinessBarrier coordinates the collection of asynchronous
// inputs (like password credentials and transport availability information)
// required to show the UI. It acts as a barrier that waits for the necessary
// data and readiness signals before triggering the UI flow.
class UiReadinessBarrier : public AuthenticatorRequestDialogModel::Observer {
 public:
  class Delegate {
   public:
    virtual void ShowUI(
        device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
        PasswordCredentialFetcher::PasswordCredentials passwords) = 0;
    virtual bool PasswordsUsable() = 0;
    virtual bool IsEnclaveActive() = 0;
    virtual bool IsEnclaveReady() = 0;
    virtual void GetGpmPasskeys(
        device::FidoRequestHandlerBase::TransportAvailabilityInfo tai,
        base::OnceCallback<
            void(device::FidoRequestHandlerBase::TransportAvailabilityInfo)>
            callback) = 0;
  };

  UiReadinessBarrier(Delegate* delegate,
                     AuthenticatorRequestDialogModel* model);
  ~UiReadinessBarrier() override;

  void SetTransportAvailabilityInfo(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo tai);

  void SetPasswordCredentials(
      PasswordCredentialFetcher::PasswordCredentials passwords);

  // AuthenticatorRequestDialogModel::Observer:
  void OnGPMReadyForUI() override;

  // Can be called to force the UI check, for example after a timeout.
  void TryToShowUI();

 private:
  void ContinueWithGpmPasskeys(
      PasswordCredentialFetcher::PasswordCredentials passwords,
      device::FidoRequestHandlerBase::TransportAvailabilityInfo tai);

  raw_ptr<Delegate> delegate_;

  std::unique_ptr<device::FidoRequestHandlerBase::TransportAvailabilityInfo>
      tai_;
  std::unique_ptr<PasswordCredentialFetcher::PasswordCredentials> passwords_;

  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observation_{this};

  base::WeakPtrFactory<UiReadinessBarrier> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_UI_READINESS_BARRIER_H_
