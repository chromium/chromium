// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_ISOLATION_SUPPORT_H_
#define CHROME_INSTALLER_UTIL_ISOLATION_SUPPORT_H_

#include <string>

namespace installer {

// Returns the isolation security attribute name. E.g.
// "GOOGLECHROME://ISOLATION".
std::wstring GetIsolationAttributeName();

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_ISOLATION_SUPPORT_H_
