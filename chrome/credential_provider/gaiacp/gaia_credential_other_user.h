// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_OTHER_USER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_OTHER_USER_H_

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

// This version of the Gaia credential (unlike CGaiaCredential) supports
// ICredentialProviderCredential2 which allows it to be added to the
// "Other User" tile when certain system policies are enabled on the system.
class ATL_NO_VTABLE COtherUserGaiaCredential
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CGaiaCredentialBase {
 public:
  DECLARE_NO_REGISTRY()

  COtherUserGaiaCredential();
  ~COtherUserGaiaCredential();

  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  BEGIN_COM_MAP(COtherUserGaiaCredential)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential2)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // CGaiaCredentialBase

  // Adds additional command line switches like showing ToS.
  HRESULT GetUserGlsCommandline(base::CommandLine* command_line) override;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_OTHER_USER_H_
