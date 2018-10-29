// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_

#include <map>

#include "base/strings/string16.h"
#include "base/win/windows_types.h"

namespace credential_provider {

// Gets global DWORD flag.
HRESULT GetGlobalFlag(const base::string16& name, DWORD* value);

// Gets global string flag.  Upon calling |length| contains maximum size of
// the |value| buffer in characters.  Upon return |length| contains the length
// of string.  This function guarantees that the string is null terminated, so
// the maximum number of non-null characters returned is |length-1|.
HRESULT GetGlobalFlag(const base::string16& name,
                      wchar_t* value,
                      ULONG* length);

// Gets DWORD property set for the given user.
HRESULT GetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        DWORD* value);

// Gets a string user property.  Upon calling |length| contains maximum size of
// the |value| buffer in characters.  Upon return |length| contains the length
// of string.  This function guarantees that the string is null terminated, so
// the maximum number of non-null characters returned is |length-1|.
HRESULT GetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        wchar_t* value,
                        ULONG* length);

// Sets a DWORD user property.
HRESULT SetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        DWORD value);

// Sets a string user property.
HRESULT SetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        const base::string16& value);

// Removes all properties for the user.
HRESULT RemoveAllUserProperties(const base::string16& sid);

// Gets token handles for all users created by this credential provider.
HRESULT GetUserTokenHandles(std::map<base::string16, base::string16>* handles);

// Gets the SID associated with the given gaia id.  If none exists, returns
// HRESULT_FROM_WIN32(ERROR_NONE_MAPPED).
HRESULT GetSidFromId(const base::string16& id, wchar_t* sid, ULONG length);

// Returns the root registry key that needs to be verified in unit tests.
const wchar_t* GetUsersRootKeyForTesting();

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REG_UTILS_H_
