// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_UTIL_H_
#define CHROMEOS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/dbus/userdataauth/install_attributes_client.h"

namespace chromeos {

// Wrappers for calls to InstallAttributesClient. Must be called from the UI
// thread. NOTE: Some of these make blocking DBus calls which will spin until
// the DBus call completes. They should be avoided if possible.

namespace install_attributes_util {

// Blocking calls to InstallAttributesClient methods.
COMPONENT_EXPORT(USERDATAAUTH_CLIENT)
bool InstallAttributesGet(const std::string& name, std::string* value);
COMPONENT_EXPORT(USERDATAAUTH_CLIENT)
bool InstallAttributesSet(const std::string& name, const std::string& value);
COMPONENT_EXPORT(USERDATAAUTH_CLIENT) bool InstallAttributesFinalize();
COMPONENT_EXPORT(USERDATAAUTH_CLIENT)
user_data_auth::InstallAttributesState InstallAttributesGetStatus();
COMPONENT_EXPORT(USERDATAAUTH_CLIENT) bool InstallAttributesIsInvalid();
COMPONENT_EXPORT(USERDATAAUTH_CLIENT) bool InstallAttributesIsFirstInstall();

}  // namespace install_attributes_util
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::install_attributes_util {
using ::chromeos::install_attributes_util::InstallAttributesFinalize;
using ::chromeos::install_attributes_util::InstallAttributesGet;
using ::chromeos::install_attributes_util::InstallAttributesIsFirstInstall;
using ::chromeos::install_attributes_util::InstallAttributesIsInvalid;
using ::chromeos::install_attributes_util::InstallAttributesSet;
}  // namespace ash::install_attributes_util

#endif  // CHROMEOS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_UTIL_H_
