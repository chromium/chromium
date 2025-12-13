// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_IMPL_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/credential_management/android/third_party_credential_manager_bridge.h"
#include "components/credential_management/credential_manager_interface.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/cert_status_flags.h"

namespace credential_management {

// Class implementing Credential Manager methods for Clank in 3P mode.
class ThirdPartyCredentialManagerImpl : public CredentialManagerInterface {
 public:
  explicit ThirdPartyCredentialManagerImpl(content::WebContents* web_contents);
  ThirdPartyCredentialManagerImpl(
      base::PassKey<class ThirdPartyCredentialManagerImplTest>,
      content::WebContents* web_contents,
      std::unique_ptr<CredentialManagerBridge> bridge);

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
  const raw_ref<content::WebContents> web_contents_;

  bool IsOffTheRecord() const;
  net::CertStatus GetMainFrameCertStatus() const;
};

}  // namespace credential_management

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_IMPL_H_
