// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_IMPL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/core/browser/credential_manager_pending_prevent_silent_access_task.h"
#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection_delegate.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/prefs/pref_member.h"

namespace password_manager {

using StoreCallback = base::OnceCallback<void()>;
using PreventSilentAccessCallback = base::OnceCallback<void()>;
using GetCallback =
    base::OnceCallback<void(CredentialManagerError,
                            const std::optional<CredentialInfo>&)>;

// Class implementing Credential Manager methods Store, PreventSilentAccess
// and Get in a platform independent way. Each method takes a callback as an
// argument and runs the callback with the result. Platform specific code
// and UI invocations are performed by PasswordManagerClient taken in
// constructor.
class CredentialManagerImpl
    : public CredentialManagerPendingPreventSilentAccessTaskDelegate,
      public CredentialManagerPendingRequestTaskDelegate,
      public CredentialManagerPasswordFormManagerDelegate {
 public:
  explicit CredentialManagerImpl(PasswordManagerClient* client);
  CredentialManagerImpl(const CredentialManagerImpl&) = delete;
  CredentialManagerImpl& operator=(const CredentialManagerImpl&) = delete;
  ~CredentialManagerImpl() override;

  void Store(const CredentialInfo& credential, StoreCallback callback);
  void PreventSilentAccess(PreventSilentAccessCallback callback);
  void Get(CredentialMediationRequirement mediation,
           int requested_credential_type_flags,
           const std::vector<GURL>& federations,
           GetCallback callback);

  void ResetPendingRequest();

  // CredentialManagerPendingRequestTaskDelegate:
  // Exposed publicly for testing.
  bool IsZeroClickAllowed() const override;

  // Returns PasswordFormDigest for the current URL.
  // Exposed publicly for testing.
  PasswordFormDigest GetSynthesizedFormForOrigin() const;

#if defined(UNIT_TEST)
  void set_leak_factory(std::unique_ptr<LeakDetectionCheckFactory> factory) {
    leak_delegate_.set_leak_factory(std::move(factory));
  }
#endif  // defined(UNIT_TEST)

 private:
  // CredentialManagerPendingRequestTaskDelegate:
  url::Origin GetOrigin() const override;
  void SendCredential(SendCredentialCallback send_callback,
                      const CredentialInfo& info) override;
  void SendPasswordForm(SendCredentialCallback send_callback,
                        CredentialMediationRequirement mediation,
                        const PasswordForm* form) override;
  PasswordManagerClient* client() const override;

  // CredentialManagerPendingPreventSilentAccessTaskDelegate:
  PasswordStoreInterface* GetProfilePasswordStore() override;
  PasswordStoreInterface* GetAccountPasswordStore() override;
  void DoneRequiringUserMediation() override;

  // CredentialManagerPasswordFormManagerDelegate:
  void OnProvisionalSaveComplete() override;

  raw_ptr<PasswordManagerClient> client_;

  // Used to store or update a credential. Calls OnProvisionalSaveComplete
  // on this delegate.
  std::unique_ptr<CredentialManagerPasswordFormManager> form_manager_;
  // Retrieves credentials from the PasswordStoreInterface and calls
  // SendCredential on this delegate. SendCredential then runs a callback
  // which was passed as an argument to Get().
  std::unique_ptr<CredentialManagerPendingRequestTask> pending_request_;
  // Notifies the PasswordStoreInterface that the origin requires user
  // mediation. Calls DoneRequiringUserMediation on this delegate.
  std::unique_ptr<CredentialManagerPendingPreventSilentAccessTask>
      pending_require_user_mediation_;

  // Helper for making the requests on leak detection.
  LeakDetectionDelegate leak_delegate_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_IMPL_H_
