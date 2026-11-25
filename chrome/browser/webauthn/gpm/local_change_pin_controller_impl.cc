// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/local_change_pin_controller_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/webauthn/gpm_pin_availability.h"
#include "chrome/browser/webauthn/gpm_pin_service.h"
#include "chrome/browser/webauthn/gpm_pin_service_factory.h"
#include "components/device_event_log/device_event_log.h"
#include "components/device_reauth/device_auth_availability_checker.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "crypto/process_bound_string.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "base/time/time.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_reauth/device_reauth_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace webauthn {

namespace {

using Step = AuthenticatorRequestDialogModel::Step;

// TODO(crbug.com/587265895): Finalize and mark OS authentication prompt strings
// translateable.
std::u16string GetOsReauthMessage() {
#if BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_GPM_CHANGE_PIN_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_GPM_CHANGE_PIN_AUTHENTICATION_PROMPT);
#else
  return std::u16string();
#endif
}

}  // namespace

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
  CancelPendingOperations();
  if (!notify_pin_change_callback_.is_null()) {
    std::move(notify_pin_change_callback_).Run(false);
  }
}

void LocalChangePinControllerImpl::IsChangePinFlowAvailable(
    PinAvailableCallback callback) {
  if (!gpm_pin_service_ || !identity_manager_) {
    FIDO_LOG(EVENT)
        << "LocalChangePinControllerImpl unavailable: missing dependencies";
    std::move(callback).Run(false);
    return;
  }

  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.IsEmpty()) {
    FIDO_LOG(EVENT)
        << "LocalChangePinControllerImpl unavailable: no primary account";
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/587265895): Evaluate reading cached local PIN state on
  // Settings page open and syncing server state when the user clicks Change PIN
  // once GpmPinService exposes cached local state.
  gpm_pin_service_->SyncPinState(
      primary_account,
      base::BindOnce([](std::optional<GpmPinService::PinState> state) {
        return state.has_value() &&
               state->availability != GpmPinAvailability::kGpmPinUnset;
      }).Then(std::move(callback)));
}

void LocalChangePinControllerImpl::StartChangePin(SuccessCallback callback) {
  // TODO(crbug.com/587265895): Record local GPM change-PIN funnel metrics (and
  // DeviceAuthParams result histogram) in a follow-up.
  if (notify_pin_change_callback_ || !gpm_pin_service_ || !identity_manager_) {
    FIDO_LOG(ERROR) << "LocalChangePinControllerImpl::StartChangePin rejected: "
                    << (notify_pin_change_callback_ ? "flow already active"
                                                    : "missing dependencies");
    std::move(callback).Run(false);
    return;
  }

  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account.IsEmpty()) {
    FIDO_LOG(ERROR)
        << "LocalChangePinControllerImpl::StartChangePin: no primary account";
    std::move(callback).Run(false);
    return;
  }

  notify_pin_change_callback_ = std::move(callback);
  account_ = std::move(primary_account);

  if (device_authenticator_factory_for_testing_) {
    device_authenticator_ = device_authenticator_factory_for_testing_.Run();
  } else {
    // TODO(crbug.com/587265895): Consider also supporting the ChromeOS login
    // password; on ChromeOS, CanAuthenticateWithBiometricOrScreenLock() only
    // checks biometrics and QuickUnlock PIN.
    std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker>
        availability_checker = CreateAvailabilityChecker();
    if (availability_checker &&
        availability_checker->CanAuthenticateWithBiometricOrScreenLock()) {
      device_authenticator_ = CreateDeviceAuthenticator();
      CHECK(device_authenticator_);
    }
  }
  if (device_authenticator_) {
    device_authenticator_->AuthenticateWithMessage(
        GetOsReauthMessage(),
        base::BindOnce(&LocalChangePinControllerImpl::OnOsUvCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // TODO(crbug.com/587265895): Fall back to Step::kGPMReauthForPinReset when OS
  // user verification is unavailable.
  FIDO_LOG(EVENT)
      << "LocalChangePinControllerImpl: OS UV unavailable, failing closed";
  Reset(/*success=*/false);
}

void LocalChangePinControllerImpl::CancelAuthenticatorRequest() {
  Reset(/*success=*/false);
}

void LocalChangePinControllerImpl::OnGPMReauthComplete(std::string rapt) {
  // TODO(crbug.com/587265895): Transition from GAIA ReAuth to PIN entry.
}

void LocalChangePinControllerImpl::OnGPMRecoverSecurityDomainClosed() {
  // TODO(crbug.com/587265895): Cancel the flow when the GAIA ReAuth window
  // closes.
}

void LocalChangePinControllerImpl::OnGPMPinEntered(const std::u16string& pin) {
  CHECK(model_->step() == Step::kGPMChangePin ||
        model_->step() == Step::kGPMChangeArbitraryPin);
  if (setting_pin_) {
    FIDO_LOG(ERROR) << "LocalChangePinControllerImpl: ignoring duplicate PIN "
                       "submission while SetPin is in flight";
    return;
  }
  if (!HasValidPrimaryAccount()) {
    FIDO_LOG(ERROR)
        << "LocalChangePinControllerImpl: primary account changed before "
           "PIN entry";
    model_->SetStep(Step::kGPMError);
    return;
  }

  setting_pin_ = true;
  model_->DisableUiOrShowLoadingDialog();
  gpm_pin_service_->SetPin(
      *account_, GpmPin(crypto::ProcessBoundString(base::UTF16ToUTF8(pin))),
      base::BindOnce(&LocalChangePinControllerImpl::OnGpmPinChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalChangePinControllerImpl::OnGPMPinOptionChanged(bool is_arbitrary) {
  CHECK(model_->step() == Step::kGPMChangePin ||
        model_->step() == Step::kGPMChangeArbitraryPin);
  if (setting_pin_) {
    FIDO_LOG(ERROR) << "LocalChangePinControllerImpl: ignoring PIN option "
                       "change while SetPin is in flight";
    return;
  }
  if (!HasValidPrimaryAccount()) {
    FIDO_LOG(ERROR)
        << "LocalChangePinControllerImpl: primary account changed before "
           "PIN option change";
    model_->SetStep(Step::kGPMError);
    return;
  }

  model_->SetStep(is_arbitrary ? Step::kGPMChangeArbitraryPin
                               : Step::kGPMChangePin);
}

void LocalChangePinControllerImpl::SetDeviceAuthenticatorFactoryForTesting(
    DeviceAuthenticatorFactory factory) {
  device_authenticator_factory_for_testing_ = std::move(factory);
}

void LocalChangePinControllerImpl::OnOsUvCompleted(bool success) {
  device_authenticator_.reset();
  if (!success) {
    FIDO_LOG(EVENT) << "LocalChangePinControllerImpl: OS UV failed or canceled";
    Reset(/*success=*/false);
    return;
  }
  if (!HasValidPrimaryAccount()) {
    FIDO_LOG(ERROR)
        << "LocalChangePinControllerImpl: primary account changed during OS UV";
    model_->SetStep(Step::kGPMError);
    return;
  }
  model_->SetStep(Step::kGPMChangePin);
}

void LocalChangePinControllerImpl::OnGpmPinChanged(bool success) {
  setting_pin_ = false;
  if (!success) {
    FIDO_LOG(ERROR) << "LocalChangePinControllerImpl: GpmPinService::SetPin "
                       "failed";
    model_->SetStep(Step::kGPMError);
    return;
  }
  Reset(/*success=*/true);
}

void LocalChangePinControllerImpl::CancelPendingOperations() {
  // Always invalidate weak pointers first: this drops any late
  // GpmPinService::SetPin() callback and prevents platform authenticators
  // (macOS, Windows, ChromeOS) that invoke AuthenticateCallback(false)
  // synchronously inside Cancel() from re-entering OnOsUvCompleted() and
  // Reset() during cancellation or destruction.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (device_authenticator_) {
    // Platform DeviceAuthenticator destructors do not call Cancel(), so cancel
    // explicitly after moving ownership out of `device_authenticator_`.
    std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
        std::move(device_authenticator_);
    authenticator->Cancel();
  }
}

void LocalChangePinControllerImpl::Reset(bool success) {
  CancelPendingOperations();
  setting_pin_ = false;
  account_.reset();
  // Take ownership of the callback before notifying model observers so a
  // re-entrant observer callback (or synchronous destruction of `this`) cannot
  // clobber or double-invoke `notify_pin_change_callback_`.
  SuccessCallback callback = std::move(notify_pin_change_callback_);
  if (model_->step() != Step::kNotStarted) {
    model_->SetStep(Step::kNotStarted);
  }
  if (!callback.is_null()) {
    std::move(callback).Run(success);
  }
}

bool LocalChangePinControllerImpl::HasValidPrimaryAccount() const {
  if (!account_.has_value() || !identity_manager_) {
    return false;
  }
  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return primary_account.account_id == account_->account_id &&
         primary_account.gaia == account_->gaia;
}

std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker>
LocalChangePinControllerImpl::CreateAvailabilityChecker() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  return ChromeDeviceAuthenticatorFactory::CreateAvailabilityChecker();
#else
  return nullptr;
#endif
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
LocalChangePinControllerImpl::CreateDeviceAuthenticator() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  device_reauth::DeviceAuthParams params(
      /*auth_validity_period=*/base::Seconds(0),
      device_reauth::DeviceAuthSource::kPasswordManager);
  return ChromeDeviceAuthenticatorFactory::GetForProfile(profile, params);
#else
  return nullptr;
#endif
}

DOCUMENT_USER_DATA_KEY_IMPL(LocalChangePinControllerImpl);

}  // namespace webauthn
