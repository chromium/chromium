// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reauth_credential.h"

#include "base/stl_util.h"
#include "chrome/credential_provider/gaiacp/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

CReauthCredential::CReauthCredential() {}

CReauthCredential::~CReauthCredential() {}

HRESULT CReauthCredential::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CReauthCredential::FinalRelease() {
  LOGFN(INFO);
}

HRESULT CReauthCredential::GetEmailForReauth(wchar_t* email, size_t length) {
  if (email == nullptr)
    return E_POINTER;

  if (length < 1)
    return E_INVALIDARG;

  errno_t err = wcsncpy_s(email, length, OLE2CW(user_email_), _TRUNCATE);
  return err == 0 ? S_OK : E_FAIL;
}

HRESULT CReauthCredential::GetStringValue(DWORD field_id, wchar_t** value) {
  DCHECK(value);

  HRESULT hr = E_INVALIDARG;
  switch (field_id) {
    case FID_DESCRIPTION: {
      base::string16 description(GetStringResource(IDS_REAUTH_FID_DESCRIPTION));
      hr = ::SHStrDupW(description.c_str(), value);
      break;
    }
    case FID_PROVIDER_LABEL: {
      base::string16 label(GetStringResource(IDS_REAUTH_FID_PROVIDER_LABEL));
      hr = ::SHStrDupW(label.c_str(), value);
      break;
    }
    default:
      hr = GetStringValueImpl(field_id, value);
      break;
  }

  return hr;
}

HRESULT CReauthCredential::GetUserSid(wchar_t** sid) {
  USES_CONVERSION;
  DCHECK(sid);
  LOGFN(INFO) << "sid=" << OLE2CW(user_sid_)
              << " email=" << OLE2CW(user_email_);

  HRESULT hr = ::SHStrDupW(OLE2CW(user_sid_), sid);
  if (FAILED(hr))
    LOGFN(ERROR) << "SHStrDupW hr=" << putHR(hr);

  return hr;
}

HRESULT CReauthCredential::FinishAuthentication(BSTR username,
                                                BSTR password,
                                                BSTR /*fullname*/,
                                                BSTR* sid,
                                                BSTR* error_text) {
  USES_CONVERSION;
  LOGFN(INFO);
  DCHECK(sid);
  DCHECK(error_text);

  // Change the user's password.
  OSUserManager* manager = OSUserManager::Get();
  DWORD error;
  HRESULT hr = manager->SetUserPassword(username, password, &error);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Unable to change password for '" << username
                 << "' hr=" << putHR(hr);
    *error_text = AllocErrorString(IDS_INTERNAL_ERROR);
    return hr;
  }

  hr = SetUserProperty(OLE2CW(user_sid_), kUserNeedsReauth, 0);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty hr=" << putHR(hr);
    return hr;
  }

  // Return the user's SID.
  *sid = ::SysAllocString(user_sid_);
  if (*sid == nullptr) {
    LOGFN(ERROR) << "Out of memory username=" << username;
    *error_text = AllocErrorString(IDS_INTERNAL_ERROR);
    return E_OUTOFMEMORY;
  }

  return S_OK;
}

HRESULT CReauthCredential::OnUserAuthenticated(BSTR username,
                                               BSTR password,
                                               BSTR sid) {
  // Make sure arguments contain expected values.
  if (user_sid_ != sid) {
    LOGFN(ERROR) << "Unexpected SID(" << sid << "," << user_sid_ << ")";
    return E_INVALIDARG;
  }

  return FinishOnUserAuthenticated(username, password, sid);
}

IFACEMETHODIMP CReauthCredential::SetUserInfo(BSTR sid, BSTR email) {
  DCHECK(sid);
  DCHECK(email);

  user_sid_ = sid;
  user_email_ = email;
  return S_OK;
}

}  // namespace credential_provider
