// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/device_management/install_attributes_util.h"

#include <stdint.h>

#include <optional>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/device_management/install_attributes_client.h"

namespace ash::install_attributes_util {

bool InstallAttributesGet(const std::string& name, std::string* value) {
  ::device_management::InstallAttributesGetRequest request;
  request.set_name(name);
  std::optional<::device_management::InstallAttributesGetReply> result =
      InstallAttributesClient::Get()->BlockingInstallAttributesGet(request);
  if (!result.has_value() ||
      result->error() !=
          device_management::DeviceManagementErrorCode::DEVICE_MANAGEMENT_ERROR_NOT_SET) {
    LOG(WARNING) << "Unable to get install attributes, error: "
                 << (result.has_value() ? result->error() : -1);
    return false;
  }

  // result->value() returned by device_management comes with the terminating '\0'
  // character.
  DCHECK(!result->value().empty());
  DCHECK_EQ(result->value().back(), 0);
  value->assign(result->value().data(), result->value().size() - 1);
  return true;
}

bool InstallAttributesSet(const std::string& name, const std::string& value) {
  ::device_management::InstallAttributesSetRequest request;
  request.set_name(name);
  request.set_value(value);
  request.mutable_value()->push_back('\0');
  std::optional<::device_management::InstallAttributesSetReply> result =
      InstallAttributesClient::Get()->BlockingInstallAttributesSet(request);
  if (!result.has_value() ||
      result->error() !=
          device_management::DeviceManagementErrorCode::DEVICE_MANAGEMENT_ERROR_NOT_SET) {
    LOG(WARNING) << "Unable to set install attributes, error: "
                 << (result.has_value() ? result->error() : -1);
    return false;
  }
  return true;
}

bool InstallAttributesFinalize() {
  std::optional<::device_management::InstallAttributesFinalizeReply> result =
      InstallAttributesClient::Get()->BlockingInstallAttributesFinalize(
          ::device_management::InstallAttributesFinalizeRequest());
  if (!result.has_value() ||
      result->error() !=
          device_management::DeviceManagementErrorCode::DEVICE_MANAGEMENT_ERROR_NOT_SET) {
    LOG(WARNING) << "Unable to finalize install attributes, error: "
                 << (result.has_value() ? result->error() : -1);
    return false;
  }
  return true;
}

device_management::InstallAttributesState InstallAttributesGetStatus() {
  std::optional<::device_management::InstallAttributesGetStatusReply> result =
      InstallAttributesClient::Get()->BlockingInstallAttributesGetStatus(
          ::device_management::InstallAttributesGetStatusRequest());
  if (!result.has_value() ||
      result->error() !=
          device_management::DeviceManagementErrorCode::DEVICE_MANAGEMENT_ERROR_NOT_SET) {
    LOG(WARNING) << "Unable to fetch install attributes status, error: "
                 << (result.has_value() ? result->error() : -1);
    return ::device_management::InstallAttributesState::UNKNOWN;
  }
  return result->state();
}

bool InstallAttributesIsInvalid() {
  device_management::InstallAttributesState state = InstallAttributesGetStatus();
  return state == device_management::InstallAttributesState::INVALID;
}

bool InstallAttributesIsFirstInstall() {
  device_management::InstallAttributesState state = InstallAttributesGetStatus();
  return state == device_management::InstallAttributesState::FIRST_INSTALL;
}

}  // namespace ash::install_attributes_util
