// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/local_change_pin_controller_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/webauthn/gpm_pin_service_factory.h"
#include "components/device_reauth/device_auth_availability_checker.h"
#include "components/device_reauth/device_authenticator.h"
#include "content/public/browser/render_frame_host.h"

namespace webauthn {

LocalChangePinControllerImpl::LocalChangePinControllerImpl(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<LocalChangePinControllerImpl>(
          render_frame_host),
      gpm_pin_service_(GpmPinServiceFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))),
      identity_manager_(IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))),
      model_(base::MakeRefCounted<AuthenticatorRequestDialogModel>(
          render_frame_host)) {
  model_observation_.Observe(model_.get());
}

LocalChangePinControllerImpl::~LocalChangePinControllerImpl() {
  if (device_authenticator_) {
    std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
        std::move(device_authenticator_);
    authenticator->Cancel();
  }
  if (!notify_pin_change_callback_.is_null()) {
    std::move(notify_pin_change_callback_).Run(false);
  }
}

void LocalChangePinControllerImpl::IsChangePinFlowAvailable(
    PinAvailableCallback callback) {
  // TODO(crbug.com/587265895): Query GpmPinService::SyncPinState for the
  // primary account.
  std::move(callback).Run(false);
}

void LocalChangePinControllerImpl::StartChangePin(SuccessCallback callback) {
  // TODO(crbug.com/587265895): Authenticate the user via OS UV or GAIA ReAuth
  // and prompt for the new PIN.
  std::move(callback).Run(false);
}

void LocalChangePinControllerImpl::CancelAuthenticatorRequest() {
  // TODO(crbug.com/587265895): Cancel any in-progress PIN change flow.
}

void LocalChangePinControllerImpl::OnGPMReauthComplete(std::string rapt) {
  // TODO(crbug.com/587265895): Transition from GAIA ReAuth to PIN entry.
}

void LocalChangePinControllerImpl::OnGPMRecoverSecurityDomainClosed() {
  // TODO(crbug.com/587265895): Cancel the flow when the GAIA ReAuth window
  // closes.
}

void LocalChangePinControllerImpl::OnGPMPinEntered(const std::u16string& pin) {
  // TODO(crbug.com/587265895): Update the GPM PIN via GpmPinService::SetPin.
}

void LocalChangePinControllerImpl::OnGPMPinOptionChanged(bool is_arbitrary) {
  // TODO(crbug.com/587265895): Switch between 6-digit and arbitrary PIN steps.
}

void LocalChangePinControllerImpl::OnOsUvCompleted(bool success) {
  // TODO(crbug.com/587265895): Transition to PIN entry on OS UV success or
  // reset on failure.
}

void LocalChangePinControllerImpl::OnGpmPinChanged(bool success) {
  // TODO(crbug.com/587265895): Complete the PIN change flow or show an error
  // step on failure.
}

std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker>
LocalChangePinControllerImpl::CreateAvailabilityChecker() {
  // TODO(crbug.com/587265895): Create platform availability checker via
  // ChromeDeviceAuthenticatorFactory.
  return nullptr;
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
LocalChangePinControllerImpl::CreateDeviceAuthenticator() {
  // TODO(crbug.com/587265895): Create platform DeviceAuthenticator via
  // ChromeDeviceAuthenticatorFactory.
  return nullptr;
}

DOCUMENT_USER_DATA_KEY_IMPL(LocalChangePinControllerImpl);

}  // namespace webauthn
