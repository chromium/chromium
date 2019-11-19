// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_AUTH_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_AUTH_UTILS_H_

#include "base/strings/string16.h"

#include "chrome/credential_provider/gaiacp/stdafx.h"

namespace credential_provider {

// Gets the auth package id for NEGOSSP_NAME_A.
HRESULT GetAuthenticationPackageId(ULONG* id);

HRESULT DetermineUserSidFromAuthenticationBuffer(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs,
    base::string16* sid);

HRESULT BuildCredPackAuthenticationBuffer(
    BSTR domain,
    BSTR username,
    BSTR password,
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* cpcs);

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_AUTH_UTILS_H_
