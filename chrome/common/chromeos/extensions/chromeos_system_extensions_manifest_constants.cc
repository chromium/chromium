// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_constants.h"

namespace chromeos {

const char kInvalidChromeOSSystemExtensionDeclaration[] =
    "Invalid value for 'chromeos_system_extension'. Must be a dictionary.";
const char kInvalidExternallyConnectableDeclaration[] =
    "chromeos_system_extension's 'externally_connectable' key must be present "
    "and its value must contain only allowlisted origins in 'matches' array";
const char kInvalidChromeOSSystemExtensionId[] =
    "'chromeos_system_extension' is not allowed for specified extension ID.";

}  // namespace chromeos
