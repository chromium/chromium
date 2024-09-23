// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANIFEST_CONSTANTS_H_
#define CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANIFEST_CONSTANTS_H_

namespace chromeos {

// Error message returned when chromeos_system_extension's value is of incorrect
// type.
extern const char kInvalidChromeOSSystemExtensionDeclaration[];
// Error message returned when a chromeos_system_extension's
// externally_connectable key contains other than allowed origin when IWA is
// supported.
extern const char kInvalidExternallyConnectableDeclaration[];
// Error message returned when extension id is not allowed.
extern const char kInvalidChromeOSSystemExtensionId[];
}  // namespace chromeos

#endif  // CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANIFEST_CONSTANTS_H_
