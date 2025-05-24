// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_IMPL_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_IMPL_H_

#include "components/credential_management/android/third_party_credential_manager_bridge.h"
#include "components/credential_management/credential_manager_interface.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"

namespace credential_management {

// Class implementing Credential Manager methods for Clank in 3P mode.
class ThirdPartyCredentialManagerImpl
    : public content::DocumentUserData<ThirdPartyCredentialManagerImpl>,
      public CredentialManagerInterface {
 public:
  explicit ThirdPartyCredentialManagerImpl(
      content::RenderFrameHost* render_frame_host);
  ThirdPartyCredentialManagerImpl(
      base::PassKey<class ThirdPartyCredentialManagerImplTest>,
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<CredentialManagerBridge> bridge);
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  ThirdPartyCredentialManagerImpl(const ThirdPartyCredentialManagerImpl&) =
      delete;
  ThirdPartyCredentialManagerImpl& operator=(
      const ThirdPartyCredentialManagerImpl&) = delete;
  ~ThirdPartyCredentialManagerImpl() override;

  // CredentialManagerInterface:
  void Store(const password_manager::CredentialInfo& credential,
             StoreCallback callback) override;
  void PreventSilentAccess(PreventSilentAccessCallback callback) override;
  void Get(password_manager::CredentialMediationRequirement mediation,
           bool include_passwords,
           const std::vector<GURL>& federations,
           GetCallback callback) override;
  void ResetAfterDisconnecting() override;

 private:
  std::unique_ptr<CredentialManagerBridge> bridge_;
};

}  // namespace credential_management

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_IMPL_H_
