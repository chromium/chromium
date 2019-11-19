// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reauth_credential.h"

#include "base/command_line.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

CReauthCredential::CReauthCredential() = default;

CReauthCredential::~CReauthCredential() = default;

HRESULT CReauthCredential::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CReauthCredential::FinalRelease() {
  LOGFN(INFO);
}

// CGaiaCredentialBase /////////////////////////////////////////////////////////

HRESULT CReauthCredential::GetUserGlsCommandline(
    base::CommandLine* command_line) {
  DCHECK(command_line);
  DCHECK(os_user_sid_.Length());

  // If this is an existing user with an SID, try to get its gaia id and pass
  // it to the GLS for verification.
  base::string16 gaia_id;
  if (GetIdFromSid(OLE2CW(os_user_sid_), &gaia_id) == S_OK &&
      !gaia_id.empty()) {
    command_line->AppendSwitchNative(kGaiaIdSwitch, gaia_id);
    if (email_for_reauth_.Length()) {
      command_line->AppendSwitchNative(kPrefillEmailSwitch,
                                       OLE2CW(email_for_reauth_));
    }
    return CGaiaCredentialBase::GetUserGlsCommandline(command_line);
  } else if (CGaiaCredentialBase::IsAdToGoogleAssociationEnabled() &&
             OSUserManager::Get()->IsUserDomainJoined(OLE2CW(os_user_sid_))) {
    // Note that if ADAssociationIsEnabled and the reauth credential is an AD
    // user account, then fallback to the GaiaCredentialBase for loading Gls.
    return CGaiaCredentialBase::GetUserGlsCommandline(command_line);
  } else {
    LOGFN(ERROR) << "Reauth credential on user=" << os_username_
                 << " does not have an associated Gaia id";
    return E_UNEXPECTED;
  }
}

HRESULT CReauthCredential::ValidateExistingUser(const base::string16& username,
                                                const base::string16& domain,
                                                const base::string16& sid,
                                                BSTR* error_text) {
  DCHECK(os_username_.Length());
  DCHECK(os_user_sid_.Length());

  // SID, domain and username found must match what is stored in this
  // credential.
  if ((os_username_ != W2COLE(username.c_str())) ||
      (os_user_domain_.Length() && os_user_domain_ != W2COLE(domain.c_str())) ||
      (os_user_sid_.Length() && os_user_sid_ != W2COLE(sid.c_str()))) {
    LOGFN(ERROR) << "Username '" << domain << "\\" << username << "' or SID '"
                 << sid << "' does not match the username '"
                 << OLE2CW(os_user_domain_) << "\\" << OLE2CW(os_username_)
                 << "' or SID '" << OLE2CW(os_user_sid_)
                 << "' for this credential";
    *error_text = AllocErrorString(IDS_ACCOUNT_IN_USE_BASE);
    return E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT CReauthCredential::GetStringValueImpl(DWORD field_id, wchar_t** value) {
  if (field_id == FID_PROVIDER_LABEL) {
    base::string16 label(
        GetStringResource(IDS_EXISTING_AUTH_FID_PROVIDER_LABEL_BASE));
    return ::SHStrDupW(label.c_str(), value);
  } else if (field_id == FID_DESCRIPTION) {
    wchar_t* sid_buffer = nullptr;
    HRESULT hr = GetUserSid(&sid_buffer);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "GetUserSid: Empty sid found";
      return ::SHStrDupW(base::string16().c_str(), value);
    }
    base::string16 sid = sid_buffer;
    ::CoTaskMemFree(sid_buffer);

    int description_label_id;
    // If its an AD user sid without a user_id set in the registry, then
    // we need to show a different description message.
    if (email_for_reauth_.Length() == 0 &&
        CGaiaCredentialBase::IsAdToGoogleAssociationEnabled() &&
        OSUserManager::Get()->IsUserDomainJoined(sid)) {
      description_label_id = IDS_REAUTH_AD_NO_USER_FID_DESCRIPTION_BASE;
    } else {
      auto auth_enforce_reason =
          AssociatedUserValidator::Get()->GetAuthEnforceReason(sid);
      switch (auth_enforce_reason) {
        case AssociatedUserValidator::EnforceAuthReason::NOT_ENROLLED_WITH_MDM:
          description_label_id =
              IDS_REAUTH_NOT_ENROLLED_WITH_MDM_FID_DESCRIPTION_BASE;
          break;
        case AssociatedUserValidator::EnforceAuthReason::
            MISSING_PASSWORD_RECOVERY_INFO:
          description_label_id =
              IDS_REAUTH_MISSING_PASSWORD_RECOVERY_INFO_FID_DESCRIPTION_BASE;
          break;
        default:
          description_label_id = IDS_REAUTH_FID_DESCRIPTION_BASE;
          break;
      }
    }

    base::string16 label(GetStringResource(description_label_id));
    return ::SHStrDupW(label.c_str(), value);
  }

  return CGaiaCredentialBase::GetStringValueImpl(field_id, value);
}

// ICredentialProviderCredential2 //////////////////////////////////////////////

HRESULT CReauthCredential::GetUserSid(wchar_t** sid) {
  USES_CONVERSION;
  DCHECK(sid);
  LOGFN(INFO) << "sid=" << OLE2CW(get_os_user_sid());

  HRESULT hr = ::SHStrDupW(OLE2CW(get_os_user_sid()), sid);
  if (FAILED(hr))
    LOGFN(ERROR) << "SHStrDupW hr=" << putHR(hr);

  return hr;
}

// IReauthCredential //////////////////////////////////////////////

HRESULT CReauthCredential::SetOSUserInfo(BSTR sid, BSTR domain, BSTR username) {
  DCHECK(sid);
  DCHECK(domain);
  DCHECK(username);

  os_user_domain_ = domain;
  os_user_sid_ = sid;
  os_username_ = username;

  // Set the default credential provider for this tile.
  HRESULT hr =
      SetLogonUiUserTileEntry(OLE2W(sid), CLSID_GaiaCredentialProvider);
  if (FAILED(hr))
    LOGFN(ERROR) << "SetLogonUIUserTileEntry hr=" << putHR(hr);

  return hr;
}

HRESULT CReauthCredential::SetEmailForReauth(BSTR email) {
  DCHECK(email);

  email_for_reauth_ = email;
  return S_OK;
}

}  // namespace credential_provider
