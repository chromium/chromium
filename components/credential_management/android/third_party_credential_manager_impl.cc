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
    : DocumentUserData(render_frame_host) {}

DOCUMENT_USER_DATA_KEY_IMPL(ThirdPartyCredentialManagerImpl);

ThirdPartyCredentialManagerImpl::~ThirdPartyCredentialManagerImpl() = default;

void ThirdPartyCredentialManagerImpl::Store(
    const password_manager::CredentialInfo& credential,
    StoreCallback callback) {
  // TODO(crbug.com/374710839): Implement.
  NOTIMPLEMENTED();
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
  // TODO(crbug.com/374710839): Implement.
  NOTIMPLEMENTED();
}

void ThirdPartyCredentialManagerImpl::ResetPendingRequest() {
  // TODO(crbug.com/374710839): Implement.
  NOTIMPLEMENTED();
}

}  // namespace credential_management
