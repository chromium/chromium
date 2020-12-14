// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CRYPTOHOME_TPM_UTIL_H_
#define CHROMEOS_DBUS_CRYPTOHOME_TPM_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"

namespace chromeos {

// Wrappers for calls to CryptohomeClient. Must be called from the UI thread.
// NOTE: Some of these make blocking DBus calls which will spin until the
// DBus call completes. They should be avoided if possible.

namespace tpm_util {

// Blocking calls to CryptohomeClient methods.
COMPONENT_EXPORT(CRYPTOHOME_CLIENT)
bool InstallAttributesGet(const std::string& name, std::string* value);
COMPONENT_EXPORT(CRYPTOHOME_CLIENT)
bool InstallAttributesSet(const std::string& name, const std::string& value);
COMPONENT_EXPORT(CRYPTOHOME_CLIENT) bool InstallAttributesFinalize();
COMPONENT_EXPORT(CRYPTOHOME_CLIENT) bool InstallAttributesIsInvalid();
COMPONENT_EXPORT(CRYPTOHOME_CLIENT) bool InstallAttributesIsFirstInstall();

}  // namespace tpm_util
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRYPTOHOME_TPM_UTIL_H_
