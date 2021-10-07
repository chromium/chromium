// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_constants.h"

namespace chromeos {

const char kInvalidChromeOSSystemExtensionId[] =
    "Invalid extension id. This extension is not allowed to define "
    "chromeos_system_extension key.";
const char kInvalidChromeOSSystemExtensionDeclaration[] =
    "Invalid value for 'chromeos_system_extension'. Must be a dictionary.";
const char kSerialNumberPermissionMustBeOptional[] =
    "'os.telemetry.serial_number' must be declared in 'optional_permissions'.";
const char kInvalidExternallyConnectableDeclaration[] =
    "chromeos_system_extension's 'externally_connectable' key must be present "
    "and its value must contain exactly one allowlisted origin in 'matches' "
    "array";

}  // namespace chromeos
