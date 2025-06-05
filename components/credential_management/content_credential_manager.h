// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_CONTENT_CREDENTIAL_MANAGER_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_CONTENT_CREDENTIAL_MANAGER_H_

#include <memory>

#include "components/credential_management/credential_manager_interface.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"

class GURL;

namespace password_manager {
struct CredentialInfo;
}

namespace credential_management {
// Implements blink::mojom::CredentialManager using an implementation of
// ChromeCredentialManagerInterface. Methods Store, PreventSilentAccess and Get
// are invoked from the renderer with callbacks as arguments.
// PasswordManagerClient is used to invoke UI.
class ContentCredentialManager : public blink::mojom::CredentialManager {
 public:
  explicit ContentCredentialManager(
      std::unique_ptr<credential_management::CredentialManagerInterface>
          credential_manager);

  ContentCredentialManager(const ContentCredentialManager&) = delete;
  ContentCredentialManager& operator=(const ContentCredentialManager&) = delete;

  ~ContentCredentialManager() override;

  void BindRequest(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::CredentialManager> receiver);
  bool HasBinding() const;
  void DisconnectBinding();

  // blink::mojom::CredentialManager methods:
  void Store(const password_manager::CredentialInfo& credential,
             StoreCallback callback) override;
  void PreventSilentAccess(PreventSilentAccessCallback callback) override;
  void Get(password_manager::CredentialMediationRequirement mediation,
           bool include_passwords,
           const std::vector<GURL>& federations,
           GetCallback callback) override;

 private:
  std::unique_ptr<CredentialManagerInterface> credential_manager_;

  mojo::Receiver<blink::mojom::CredentialManager> receiver_{this};
};

}  // namespace credential_management

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_CONTENT_CREDENTIAL_MANAGER_H_
