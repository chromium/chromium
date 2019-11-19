// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_CONSTANTS_H_
#define CHROME_UPDATER_WIN_CONSTANTS_H_

#include "base/strings/string16.h"
#include "chrome/updater/updater_version.h"

namespace updater {

// The prefix to use for global names in WIN32 API's. The prefix is necessary
// to avoid collision on kernel object names.
extern const base::char16 kGlobalPrefix[];

// Registry keys and value names.
#define COMPANY_KEY "Software\\" COMPANY_SHORTNAME_STRING "\\"
// Use |Update| instead of PRODUCT_FULLNAME_STRING for the registry key name
// to be backward compatible with Google Update / Omaha.
#define UPDATER_KEY COMPANY_KEY "Update\\"
#define CLIENTS_KEY UPDATER_KEY "Clients\\"
#define CLIENT_STATE_KEY UPDATER_KEY "ClientState\\"

extern const base::char16 kRegistryValuePV[];
extern const base::char16 kRegistryValueName[];

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_CONSTANTS_H_
