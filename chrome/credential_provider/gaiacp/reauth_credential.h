// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"

namespace credential_provider {

// A credential for a user that exists on the system and is associated with a
// Gaia account.
class ATL_NO_VTABLE CReauthCredential
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CGaiaCredentialBase,
      public IReauthCredential {
 public:
  DECLARE_NO_REGISTRY()

  CReauthCredential();
  ~CReauthCredential();

  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  // This class does not say it implements ICredentialProviderCredential2.
  // It only implements ICredentialProviderCredential.  Otherwise the
  // credential will show up on the welcome screen only for domain joined
  // machines.
  BEGIN_COM_MAP(CReauthCredential)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(IReauthCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential2)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // ICredentialProviderCredential2
  IFACEMETHODIMP GetUserSid(wchar_t** sid) override;

  // IReauthCredential
  IFACEMETHODIMP SetOSUserInfo(BSTR sid, BSTR domain, BSTR username) override;
  IFACEMETHODIMP SetEmailForReauth(BSTR email) override;

  const CComBSTR& get_os_user_sid() const { return os_user_sid_; }
  const CComBSTR& get_os_user_domain() const { return os_user_domain_; }
  const CComBSTR& get_os_username() const { return os_username_; }

  // CGaiaCredentialBase

  // Adds additional command line switches to specify which gaia id to sign in
  // and which email is used to prefill the Gaia page.
  HRESULT GetUserGlsCommandline(base::CommandLine* command_line) override;

  // Checks if the information for the given |domain|\|username|, |sid| is
  // valid.
  // Returns S_OK if the user information stored in this credential matches
  // the user information that is being validated. Otherwise fills |error_text|
  // with an appropriate error message and returns an error.
  HRESULT ValidateExistingUser(const base::string16& username,
                               const base::string16& domain,
                               const base::string16& sid,
                               BSTR* error_text) override;
  HRESULT GetStringValueImpl(DWORD field_id, wchar_t** value) override;

  // Information about the OS user.
  CComBSTR os_user_domain_;
  CComBSTR os_username_;
  CComBSTR os_user_sid_;

  CComBSTR email_for_reauth_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_
