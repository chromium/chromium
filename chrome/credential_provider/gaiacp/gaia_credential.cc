// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential.h"

#include <algorithm>
#include <memory>

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"

namespace credential_provider {

CGaiaCredential::CGaiaCredential() {}

CGaiaCredential::~CGaiaCredential() {}

HRESULT CGaiaCredential::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CGaiaCredential::FinalRelease() {
  LOGFN(INFO);
}

HRESULT CGaiaCredential::FinishAuthentication(BSTR username,
                                              BSTR password,
                                              BSTR fullname,
                                              BSTR* sid,
                                              BSTR* error_text) {
  LOGFN(INFO);
  DCHECK(error_text);

  *error_text = nullptr;

  base::string16 comment(GetStringResource(IDS_USER_ACCOUNT_COMMENT));
  HRESULT hr = CreateNewUser(OSUserManager::Get(), OLE2CW(username),
                             OLE2CW(password), OLE2CW(fullname),
                             comment.c_str(), /*add_to_users_group=*/true, sid);
  if (hr == HRESULT_FROM_WIN32(NERR_UserExists)) {
    // The user signed in with a Google account that was already used to create
    // a local account.  This is considered a success as long as the password
    // matches the existing one.
    base::win::ScopedHandle handle;
    HRESULT hrLogon = OSUserManager::Get()->CreateLogonToken(
        username, password, /*interactive=*/true, &handle);
    if (SUCCEEDED(hrLogon)) {
      hr = S_OK;
    } else {
      LOGFN(INFO) << "CreateLogonToken hr=" << putHR(hrLogon)
                  << " account=" << OLE2CW(username) << " sid=" << sid;
    }
  } else if (FAILED(hr)) {
    LOGFN(ERROR) << "CreateNewUser hr=" << putHR(hr)
                 << " account=" << OLE2CW(username);
    *error_text = AllocErrorString(IDS_CANT_CREATE_USER);
  }

  return hr;
}

}  // namespace credential_provider
