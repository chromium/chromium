// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_FILTER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_FILTER_H_

#include <atlbase.h>
#include <atlcom.h>
#include <credentialprovider.h>

namespace credential_provider {

// Implementation of ICredentialProviderFilter. This filter's only purpose is to
// check whether certain users need to have their normal sign in access revoked
// and forced into a Gaia reauth flow. The verification is done in this object
// to ensure that all user privileges are revoked before any credential
// providers get their SetUserArray called. Otherwise there could be
// inconsistency in the UI where some credential providers are not shown if they
// are called before the user's rights have been revoked.

extern const CLSID CLSID_CGaiaCredentialProviderFilter;

class ATL_NO_VTABLE CGaiaCredentialProviderFilter
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<CGaiaCredentialProviderFilter,
                         &CLSID_CGaiaCredentialProviderFilter>,
      public ICredentialProviderFilter {
 public:
  // This COM object is registered with the rgs file.  The rgs file is used by
  // CGaiaCredentialProviderModule class, see latter for details.
  DECLARE_NO_REGISTRY()

  CGaiaCredentialProviderFilter();
  ~CGaiaCredentialProviderFilter();

  BEGIN_COM_MAP(CGaiaCredentialProviderFilter)
  COM_INTERFACE_ENTRY(ICredentialProviderFilter)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  // ICredentialProviderFilter
  IFACEMETHODIMP Filter(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                        DWORD flags,
                        GUID* providers_clsids,
                        BOOL* providers_allow,
                        DWORD providers_count) override;
  IFACEMETHODIMP UpdateRemoteCredential(
      const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
      CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_out) override;
};

// OBJECT_ENTRY_AUTO() contains an extra semicolon.
// TODO(thakis): Make -Wextra-semi not warn on semicolons that are from a
// macro in a system header, then remove the pragma, https://llvm.org/PR40874
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#endif

OBJECT_ENTRY_AUTO(CLSID_CGaiaCredentialProviderFilter,
                  CGaiaCredentialProviderFilter)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_FILTER_H_
