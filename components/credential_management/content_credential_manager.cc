// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/content_credential_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/password_manager/core/browser/credential_manager_impl.h"
#include "mojo/public/cpp/bindings/message.h"

namespace credential_management {

// ContentCredentialManager -------------------------------------------------

ContentCredentialManager::ContentCredentialManager(
    std::unique_ptr<CredentialManagerInterface> credential_manager)
    : credential_manager_(std::move(credential_manager)) {}

ContentCredentialManager::~ContentCredentialManager() = default;

void ContentCredentialManager::BindRequest(
    mojo::PendingReceiver<blink::mojom::CredentialManager> receiver) {
  if (receiver_.is_bound()) {
    mojo::ReportBadMessage("CredentialManager is already bound.");
    return;
  }
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
  credential_manager_->ResetAfterDisconnecting();
}

void ContentCredentialManager::Store(
    const password_manager::CredentialInfo& credential,
    StoreCallback callback) {
  credential_manager_->Store(credential, std::move(callback));
}

void ContentCredentialManager::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  credential_manager_->PreventSilentAccess(std::move(callback));
}

void ContentCredentialManager::Get(
    password_manager::CredentialMediationRequirement mediation,
    bool include_passwords,
    const std::vector<GURL>& federations,
    GetCallback callback) {
  credential_manager_->Get(mediation, include_passwords, federations,
                           std::move(callback));
}

}  // namespace credential_management
