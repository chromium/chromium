// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_credential_manager.h"

#include <utility>

#include "base/bind.h"

namespace password_manager {

// ContentCredentialManager -------------------------------------------------

ContentCredentialManager::ContentCredentialManager(
    PasswordManagerClient* client)
    : impl_(client) {}

ContentCredentialManager::~ContentCredentialManager() {}

void ContentCredentialManager::BindRequest(
    mojo::PendingReceiver<blink::mojom::CredentialManager> receiver) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));

  // The browser side will close the message pipe on DidFinishNavigation before
  // the renderer side would be destroyed, and the renderer never explicitly
  // closes the pipe. So a connection error really means an error here, in which
  // case the renderer will try to reconnect when the next call to the API is
  // made. Make sure this implementation will no longer be bound to a broken
  // pipe once that happens, so the DCHECK above will succeed.
  receiver_.set_disconnect_handler(base::BindOnce(
      &ContentCredentialManager::DisconnectBinding, base::Unretained(this)));
}

bool ContentCredentialManager::HasBinding() const {
  return receiver_.is_bound();
}

void ContentCredentialManager::DisconnectBinding() {
  receiver_.reset();
}

void ContentCredentialManager::Store(const CredentialInfo& credential,
                                     StoreCallback callback) {
  impl_.Store(credential, std::move(callback));
}

void ContentCredentialManager::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  impl_.PreventSilentAccess(std::move(callback));
}

void ContentCredentialManager::Get(CredentialMediationRequirement mediation,
                                   bool include_passwords,
                                   const std::vector<GURL>& federations,
                                   GetCallback callback) {
  impl_.Get(mediation, include_passwords, federations, std::move(callback));
}

}  // namespace password_manager
