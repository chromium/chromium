// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_filter.h"

#include <string>

#include "build/branding_buildflags.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
#include "chrome/credential_provider/gaiacp/chrome_availability_checker.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
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
  LOGFN(VERBOSE);
  return S_OK;
}

void CGaiaCredentialProviderFilter::FinalRelease() {
  LOGFN(VERBOSE);
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

  // If a supported version of Chrome is not found, return right away so that
  // we don't revoke any access to normal credential providers.
  if (!ChromeAvailabilityChecker::Get()->HasSupportedChromeVersion()) {
    // Filter out the GaiaCredentialProvider in this case.
    for (DWORD i = 0; i < providers_count; ++i) {
      if (providers_clsids[i] == CLSID_GaiaCredentialProvider)
        providers_allow[i] = FALSE;
    }

    // Delete the startup sentinel file since if Chrome is not installed/usable
    // we will repeatedly fail and will hit the maximum number of crashes
    // after which we stop associating as a credential provider.
    DeleteStartupSentinel();
    LOGFN(ERROR) << "Supported Chrome version not found.";
    return S_OK;
  }

  // Update sid mapping at least once before invoking any other  methods in the
  // later stages.
  AssociatedUserValidator::Get()->UpdateAssociatedSids(nullptr);

  return S_OK;
}

HRESULT CGaiaCredentialProviderFilter::UpdateRemoteCredential(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_in,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs_out) {
  // Don't do anything if Chrome is not found.
  if (!ChromeAvailabilityChecker::Get()->HasSupportedChromeVersion())
    return E_NOTIMPL;

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
  std::wstring serialization_sid;
  hr = DetermineUserSidFromAuthenticationBuffer(pcpcs_in, &serialization_sid);
  if (FAILED(hr))
    return E_NOTIMPL;

  // Check if this user needs a reauth, if not, just pass the serialization
  // to the default handler.
  if (!AssociatedUserValidator::Get()->IsAuthEnforcedForUser(
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
