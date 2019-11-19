// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_USER_INFO_H_
#define CHROME_UPDATER_WIN_USER_INFO_H_

#include <windows.h>

#include "base/strings/string16.h"
#include "base/win/atl.h"

namespace updater {

// Gets the user name, domain, and the SID associated with the access token
// of the current process.
HRESULT GetProcessUser(base::string16* name,
                       base::string16* domain,
                       base::string16* sid);

// Gets SID associated with the access token of the current process.
HRESULT GetProcessUserSid(CSid* sid);

// Gets the user SID associated with the access token of the current thread if
// the thread is impersonating. If the thread is not impersonating, the API
// fails with ERROR_NO_TOKEN.
HRESULT GetThreadUserSid(base::string16* sid);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_USER_INFO_H_
