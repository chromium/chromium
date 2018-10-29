// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

// Implementation of a ICredentialProviderCredential backed by a Gaia account.
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
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential2)
  COM_INTERFACE_ENTRY(IReauthCredential)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT GetEmailForReauth(wchar_t* email, size_t length) override;

  // ICredentialProviderCredential2
  IFACEMETHODIMP GetStringValue(DWORD dwFieldID, wchar_t** ppsz) override;
  IFACEMETHODIMP GetUserSid(wchar_t** sid) override;

  // IGaiaCredential
  IFACEMETHODIMP FinishAuthentication(BSTR username,
                                      BSTR password,
                                      BSTR fullname,
                                      BSTR* sid,
                                      BSTR* error_text) override;
  IFACEMETHODIMP OnUserAuthenticated(BSTR username,
                                     BSTR password,
                                     BSTR sid) override;

  // IReauthCredential
  IFACEMETHODIMP SetUserInfo(BSTR sid, BSTR username) override;

  CComBSTR user_sid_;
  CComBSTR user_email_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_
