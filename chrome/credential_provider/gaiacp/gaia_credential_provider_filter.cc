// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_filter.h"

#include "base/strings/string16.h"
#include "build/branding_buildflags.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const CLSID CLSID_CGaiaCredentialProviderFilter = {
    0xaec62ffe,
    0x6617,
    0x4685,
    {0xa0, 0x80, 0xb1, 0x1a, 0x84, 0x8a, 0x06, 0x07}};
#else
const CLSID CLSID_CGaiaCredentialProviderFilter = {
    0xfd768777,
    0x340e,
    0x4426,
    {0x9b, 0x07, 0x8f, 0xdf, 0x48, 0x9f, 0x1f, 0xf9}};
#endif

CGaiaCredentialProviderFilter::CGaiaCredentialProviderFilter() = default;

CGaiaCredentialProviderFilter::~CGaiaCredentialProviderFilter() = default;

HRESULT CGaiaCredentialProviderFilter::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CGaiaCredentialProviderFilter::FinalRelease() {
  LOGFN(INFO);
}

HRESULT CGaiaCredentialProviderFilter::Filter(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD flags,
    GUID* providers_clsids,
    BOOL* providers_allow,
    DWORD providers_count) {
  // Re-enable all users in case internet has been lost or the computer
  // crashed while users were locked out.
  AssociatedUserValidator::Get()->AllowSigninForAllAssociatedUsers(cpus);
  // Check to see if any users need to have their access to this system
  // using the normal credential providers revoked.
  AssociatedUserValidator::Get()->DenySigninForUsersWithInvalidTokenHandles(
      cpus);
  return S_OK;
}

HRESULT CGaiaCredentialProviderFilter::UpdateRemoteCredential(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_out) {
  if (!pcpcs_out)
    return E_NOTIMPL;

  ULONG auth_package_id;
  HRESULT hr = GetAuthenticationPackageId(&auth_package_id);
  if (FAILED(hr))
    return E_NOTIMPL;

  if (pcpcs_in->ulAuthenticationPackage != auth_package_id)
    return E_NOTIMPL;

  if (pcpcs_in->cbSerialization == 0)
    return E_NOTIMPL;

  // If serialziation data is set, try to extract the sid for the user
  // referenced in the serialization data.
  base::string16 serialization_sid;
  hr = DetermineUserSidFromAuthenticationBuffer(pcpcs_in, &serialization_sid);
  if (FAILED(hr))
    return E_NOTIMPL;

  // Check if this user needs a reauth, if not, just pass the serialization
  // to the default handler.
  if (AssociatedUserValidator::Get()->IsTokenHandleValidForUser(
          serialization_sid)) {
    return E_NOTIMPL;
  }

  // Copy out the serialization data to pass it into the provider.
  byte* serialization_buffer =
      (BYTE*)::CoTaskMemAlloc(pcpcs_in->cbSerialization);
  if (serialization_buffer == nullptr)
    return E_NOTIMPL;

  pcpcs_out->rgbSerialization = serialization_buffer;
  memcpy(pcpcs_out->rgbSerialization, pcpcs_in->rgbSerialization,
         pcpcs_in->cbSerialization);
  pcpcs_out->cbSerialization = pcpcs_in->cbSerialization;
  pcpcs_out->clsidCredentialProvider = CLSID_GaiaCredentialProvider;
  pcpcs_out->ulAuthenticationPackage = pcpcs_in->ulAuthenticationPackage;

  return S_OK;
}

}  // namespace credential_provider
