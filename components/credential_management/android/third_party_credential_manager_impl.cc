// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/credential_management/android/third_party_credential_manager_impl.h"

#include "base/android/jni_callback.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"

namespace credential_management {

ThirdPartyCredentialManagerImpl::ThirdPartyCredentialManagerImpl(
    content::RenderFrameHost* render_frame_host)
    : DocumentUserData(render_frame_host),
      bridge_(std::make_unique<ThirdPartyCredentialManagerBridge>()) {}

ThirdPartyCredentialManagerImpl::ThirdPartyCredentialManagerImpl(
    base::PassKey<class ThirdPartyCredentialManagerImplTest>,
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<CredentialManagerBridge> bridge)
    : DocumentUserData(render_frame_host), bridge_(std::move(bridge)) {}

DOCUMENT_USER_DATA_KEY_IMPL(ThirdPartyCredentialManagerImpl);

ThirdPartyCredentialManagerImpl::~ThirdPartyCredentialManagerImpl() = default;

void ThirdPartyCredentialManagerImpl::Store(
    const password_manager::CredentialInfo& credential,
    StoreCallback callback) {
  std::u16string username = credential.id.value_or(u"");
  std::u16string password = credential.password.value_or(u"");
  bridge_->Store(username, password,
                 render_frame_host().GetLastCommittedOrigin().Serialize(),
                 std::move(callback));
}

void ThirdPartyCredentialManagerImpl::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  // TODO(crbug.com/374710839): Implement.
  NOTIMPLEMENTED();
}

// This method decides if credential picker should be shown.

// Credential mediation can be silent, optional, conditional or
// required.

// Silent mediation should not show a credential picker even if there
// are multiple credentials available and return null.
// Silent mediation can't be implemented here, because the Android API
// doesn't support it. We'd have to know the amount of available credentials
// already before calling get.

// By default, the GetCredentialRequest will have optional
// mediation: if there's more than one matching credential, the system
// will show the credential picker UI to the user.

// Required mediation will show the credential picker, no matter the amount
// of choices.

// Conditional mediation allows the user to pick a credential from the
// picker or avoid selecting a credential without any user-visible error
// condition. That type of mediotion is also not supported in the Android
// API.

bool ShouldAllowAutoSelect(
    password_manager::CredentialMediationRequirement mediation) {
  switch (mediation) {
    case password_manager::CredentialMediationRequirement::kOptional:
      return true;
    case password_manager::CredentialMediationRequirement::kRequired:
      return false;
    case password_manager::CredentialMediationRequirement::kSilent:
    case password_manager::CredentialMediationRequirement::kConditional:
      NOTIMPLEMENTED();
  }
  return false;
}

void ThirdPartyCredentialManagerImpl::Get(
    password_manager::CredentialMediationRequirement mediation,
    bool include_passwords,
    const std::vector<GURL>& federations,
    GetCallback callback) {
  if (mediation == password_manager::CredentialMediationRequirement::kSilent ||
      mediation ==
          password_manager::CredentialMediationRequirement::kConditional) {
    std::move(callback).Run(password_manager::CredentialManagerError::UNKNOWN,
                            std::nullopt);
    return;
  }

  // TODO(crbug.com/404199116): Pass all the parameters to the bridge.
  bridge_->Get(ShouldAllowAutoSelect(mediation),
               include_passwords,
               render_frame_host().GetLastCommittedOrigin().Serialize(),
               std::move(callback));
}

void ThirdPartyCredentialManagerImpl::ResetAfterDisconnecting() {
  // There is currently nothing to do upon disconnecting for this implementation
  // of CredentialManagerInterface.
}

}  // namespace credential_management
