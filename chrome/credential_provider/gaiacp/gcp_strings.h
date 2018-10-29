// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_STRINGS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_STRINGS_H_

namespace credential_provider {

// Names of keys returned on json data from UI process.
extern const char kKeyEmail[];
extern const char kKeyFullname[];
extern const char kKeyId[];
extern const char kKeyMdmUrl[];
extern const char kKeyMdmIdToken[];
extern const char kKeyPassword[];
extern const char kKeyRefreshToken[];
extern const char kKeySID[];
extern const char kKeyTokenHandle[];
extern const char kKeyUsername[];

// Name of registry value that holds user properties.
extern const wchar_t kUserTokenHandle[];
extern const wchar_t kUserNeedsReauth[];
extern const wchar_t kUserEmail[];
extern const wchar_t kUserId[];

// Username and password key for special GAIA account to run GLS.
extern const wchar_t kGaiaAccountName[];
extern const wchar_t kLsaKeyGaiaPassword[];

// Name of the desktop used on the Window welcome screen for interactive
// logon.
extern const wchar_t kDesktopName[];
extern const wchar_t kDesktopFullName[];

// Google Update related registry paths.
extern const wchar_t kRegUpdaterClientStateAppPath[];
extern const wchar_t kRegUpdaterClientsAppPath[];

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_STRINGS_H_
