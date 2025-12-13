// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/credential_management/android/third_party_credential_manager_impl.h"

#include "base/android/jni_callback.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"

namespace credential_management {

ThirdPartyCredentialManagerImpl::ThirdPartyCredentialManagerImpl(
    content::WebContents* web_contents)
    : bridge_(std::make_unique<ThirdPartyCredentialManagerBridge>()),
      web_contents_(CHECK_DEREF(web_contents)) {}

ThirdPartyCredentialManagerImpl::ThirdPartyCredentialManagerImpl(
    base::PassKey<class ThirdPartyCredentialManagerImplTest>,
    content::WebContents* web_contents,
    std::unique_ptr<CredentialManagerBridge> bridge)
    : bridge_(std::move(bridge)), web_contents_(CHECK_DEREF(web_contents)) {}

ThirdPartyCredentialManagerImpl::~ThirdPartyCredentialManagerImpl() = default;

void ThirdPartyCredentialManagerImpl::Store(
    const password_manager::CredentialInfo& credential,
    StoreCallback callback) {
  // Don't store empty credentials.
  if (credential.type ==
      password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY) {
    // Send back an acknowledge response.
    std::move(callback).Run();
    return;
  }

  if (!credential.password.has_value() || credential.password.value().empty()) {
    // Send back an acknowledge response.
    std::move(callback).Run();
    return;
  }
  std::u16string username = credential.id.value_or(u"");
  std::u16string password = credential.password.value();
  bridge_->Store(username, password,
                 web_contents_->GetPrimaryMainFrame()
                     ->GetLastCommittedOrigin()
                     .Serialize(),
                 std::move(callback));
}

void ThirdPartyCredentialManagerImpl::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  // Send acknowledge response back.
  // We're currently preventing silent access for every get request by default
  // in 3rd party mode so there is nothing more to do here for now.
  std::move(callback).Run();
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

net::CertStatus ThirdPartyCredentialManagerImpl::GetMainFrameCertStatus()
    const {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetLastCommittedEntry();
  if (!entry) {
    return 0;
  }
  return entry->GetSSL().cert_status;
}

void ThirdPartyCredentialManagerImpl::Get(
    password_manager::CredentialMediationRequirement mediation,
    bool include_passwords,
    const std::vector<GURL>& federations,
    GetCallback callback) {
  // Return an empty credential if the current page has SSL errors.
  if (net::IsCertStatusError(GetMainFrameCertStatus())) {
    std::move(callback).Run(password_manager::CredentialManagerError::SUCCESS,
                            password_manager::CredentialInfo());
    return;
  }

  // Return an empty credential for incognito mode.
  if (IsOffTheRecord()) {
    // Callback with empty credential info.
    std::move(callback).Run(password_manager::CredentialManagerError::SUCCESS,
                            password_manager::CredentialInfo());
    return;
  }

  // Silent mediation is not supported because the list of origins that prevent
  // silent access can't be persisted.
  // Conditional mediation is only for passkeys, not for passwords that are
  // currently the only supported credential type.
  // Return an empty credential in these cases.
  if (mediation == password_manager::CredentialMediationRequirement::kSilent ||
      mediation ==
          password_manager::CredentialMediationRequirement::kConditional) {
    std::move(callback).Run(password_manager::CredentialManagerError::SUCCESS,
                            password_manager::CredentialInfo());
    return;
  }

  bridge_->Get(ShouldAllowAutoSelect(mediation), include_passwords, federations,
               web_contents_->GetPrimaryMainFrame()
                   ->GetLastCommittedOrigin()
                   .Serialize(),
               std::move(callback));
}

void ThirdPartyCredentialManagerImpl::ResetAfterDisconnecting() {
  // There is currently nothing to do upon disconnecting for this implementation
  // of CredentialManagerInterface.
}

bool ThirdPartyCredentialManagerImpl::IsOffTheRecord() const {
  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

}  // namespace credential_management
