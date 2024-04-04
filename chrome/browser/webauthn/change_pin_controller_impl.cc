// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/change_pin_controller_impl.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/features.h"

ChangePinControllerImpl::ChangePinControllerImpl(
    content::WebContents* web_contents)
    : enclave_enabled_(
          base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAuthenticator)) {
  if (!enclave_enabled_) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  enclave_manager_ = EnclaveManagerFactory::GetForProfile(profile);
  sync_service_ = SyncServiceFactory::IsSyncAllowed(profile)
                      ? SyncServiceFactory::GetForProfile(profile)
                      : nullptr;
  model_ = std::make_unique<AuthenticatorRequestDialogModel>(
      web_contents->GetPrimaryMainFrame());
}

ChangePinControllerImpl::~ChangePinControllerImpl() = default;

// static
ChangePinControllerImpl* ChangePinControllerImpl::ForWebContents(
    content::WebContents* web_contents) {
  static constexpr char kChangePinControllerImplKey[] =
      "ChangePinControllerImplKey";
  if (!web_contents->GetUserData(kChangePinControllerImplKey)) {
    web_contents->SetUserData(
        kChangePinControllerImplKey,
        std::make_unique<ChangePinControllerImpl>(web_contents));
  }
  return static_cast<ChangePinControllerImpl*>(
      web_contents->GetUserData(kChangePinControllerImplKey));
}

bool ChangePinControllerImpl::IsChangePinFlowAvailable() {
  if (!enclave_enabled_) {
    return false;
  }
  bool sync_enabled = sync_service_ && sync_service_->IsSyncFeatureEnabled() &&
                      sync_service_->GetUserSettings()->GetSelectedTypes().Has(
                          syncer::UserSelectableType::kPasswords);
  bool enclave_valid = enclave_enabled_ && enclave_manager_->is_ready() &&
                       enclave_manager_->has_wrapped_pin();
  return sync_enabled && enclave_valid;
}

bool ChangePinControllerImpl::StartChangePin() {
  if (!IsChangePinFlowAvailable()) {
    return false;
  }
  model_->SetStep(AuthenticatorRequestDialogModel::Step::kGPMReauthAccount);
  return true;
}
