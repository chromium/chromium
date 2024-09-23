// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_H_

#include "components/password_manager/core/browser/credential_manager_impl.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"

class GURL;

namespace password_manager {
class PasswordManagerClient;
struct CredentialInfo;

// Implements blink::mojom::CredentialManager using core class
// CredentialManagerImpl. Methods Store, PreventSilentAccess and Get are invoked
// from the renderer with callbacks as arguments. PasswordManagerClient is used
// to invoke UI.
class ContentCredentialManager : public blink::mojom::CredentialManager {
 public:
  explicit ContentCredentialManager(PasswordManagerClient* client);

  ContentCredentialManager(const ContentCredentialManager&) = delete;
  ContentCredentialManager& operator=(const ContentCredentialManager&) = delete;

  ~ContentCredentialManager() override;

  void BindRequest(
      mojo::PendingReceiver<blink::mojom::CredentialManager> receiver);
  bool HasBinding() const;
  void DisconnectBinding();

  // blink::mojom::CredentialManager methods:
  void Store(const CredentialInfo& credential, StoreCallback callback) override;
  void PreventSilentAccess(PreventSilentAccessCallback callback) override;
  void Get(CredentialMediationRequirement mediation,
           int requested_credential_type_flags,
           const std::vector<GURL>& federations,
           GetCallback callback) override;

 private:
  CredentialManagerImpl impl_;

  mojo::Receiver<blink::mojom::CredentialManager> receiver_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_H_
