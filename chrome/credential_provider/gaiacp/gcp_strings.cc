// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcp_strings.h"

namespace credential_provider {

// Names of keys returned on json data from UI process.
const char kKeyEmail[] = "email";
const char kKeyFullname[] = "full_name";
const char kKeyId[] = "id";
const char kKeyMdmUrl[] = "mdm_url";
const char kKeyMdmIdToken[] = "mdm_id_token";
const char kKeyPassword[] = "password";
const char kKeyRefreshToken[] = "refresh_token";
const char kKeySID[] = "sid";
const char kKeyTokenHandle[] = "token_handle";
const char kKeyUsername[] = "user_name";

// Name of registry value that holds user properties.
const wchar_t kUserTokenHandle[] = L"th";
const wchar_t kUserNeedsReauth[] = L"nr";
const wchar_t kUserEmail[] = L"email";
const wchar_t kUserId[] = L"id";

// Username and password key for special GAIA account to run GLS.
const wchar_t kGaiaAccountName[] = L"gaia";
// L$ prefix means this secret can only be accessed locally.
const wchar_t kLsaKeyGaiaPassword[] = L"L$GAIA_PASSWORD";

// These two variables need to remain consistent.
const wchar_t kDesktopName[] = L"Winlogon";
const wchar_t kDesktopFullName[] = L"WinSta0\\Winlogon";

// Google Update related registry paths.
const wchar_t kRegUpdaterClientStateAppPath[] =
    L"SOFTWARE\\Google\\Update\\ClientState\\"
    L"{32987697-A14E-4B89-84D6-630D5431E831}";
const wchar_t kRegUpdaterClientsAppPath[] =
    L"SOFTWARE\\Google\\Update\\Clients\\"
    L"{32987697-A14E-4B89-84D6-630D5431E831}";

}  // namespace credential_provider
