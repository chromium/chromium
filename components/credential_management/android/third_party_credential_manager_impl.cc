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

void ThirdPartyCredentialManagerImpl::Get(
    password_manager::CredentialMediationRequirement mediation,
    bool include_passwords,
    const std::vector<GURL>& federations,
    GetCallback callback) {
  // TODO(crbug.com/404199116): Pass all the parameters to the bridge.
  bridge_->Get(render_frame_host().GetLastCommittedOrigin().Serialize(),
               std::move(callback));
}

void ThirdPartyCredentialManagerImpl::ResetPendingRequest() {
  // TODO(crbug.com/374710839): Implement.
  NOTIMPLEMENTED();
}

}  // namespace credential_management
