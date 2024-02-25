// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_GOOGLE_UPDATE_UTIL_H_
#define CHROME_INSTALLER_GCAPI_GOOGLE_UPDATE_UTIL_H_

#include <string>

namespace gcapi_internals {

extern const wchar_t kChromeRegClientsKey[];
extern const wchar_t kChromeRegClientStateKey[];

// Reads Chrome's brand from HKCU or HKLM. Returns true if |value| is populated
// with the brand.
bool GetBrand(std::wstring* value);

}  // namespace gcapi_internals

#endif  // CHROME_INSTALLER_GCAPI_GOOGLE_UPDATE_UTIL_H_
