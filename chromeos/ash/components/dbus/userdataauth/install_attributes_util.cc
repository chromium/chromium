// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/install_attributes_util.h"

#include <stdint.h>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/userdataauth/install_attributes_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::install_attributes_util {

bool InstallAttributesGet(const std::string& name, std::string* value) {
  ::user_data_auth::InstallAttributesGetRequest request;
  request.set_name(name);
  absl::optional<::user_data_auth::InstallAttributesGetReply> result =
      InstallAttributesClient::Get()->BlockingInstallAttributesGet(request);
  if (!result.has_value() ||
      result->error() !=
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(WARNING) << "Unable to get install attributes, error: "
                 << (result.has_value() ? result->error() : -1);
    return false;
  }

  // result->value() returned by cryptohome comes with the terminating '\0'
  // character.
  DCHECK(!result->value().empty());
  DCHECK_EQ(result->value().back(), 0);
  value->assign(result->value().data(), result->value().size() - 1);
  return true;
}

bool InstallAttributesSet(const std::string& name, const std::string& value) {
  ::user_data_auth::InstallAttributesSetRequest request;
  request.set_name(name);
  request.set_value(value);
  request.mutable_value()->push_back('\0');
  absl::optional<::user_data_auth::InstallAttributesSetReply> result =
      InstallAttributesClient::Get()->BlockingInstallAttributesSet(request);
  if (!result.has_value() ||
      result->error() !=
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(WARNING) << "Unable to set install attributes, error: "
                 << (result.has_value() ? result->error() : -1);
    return false;
  }
  return true;
}

bool InstallAttributesFinalize() {
  absl::optional<::user_data_auth::InstallAttributesFinalizeReply> result =
      InstallAttributesClient::Get()->BlockingInstallAttributesFinalize(
          ::user_data_auth::InstallAttributesFinalizeRequest());
  if (!result.has_value() ||
      result->error() !=
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(WARNING) << "Unable to finalize install attributes, error: "
                 << (result.has_value() ? result->error() : -1);
    return false;
  }
  return true;
}

user_data_auth::InstallAttributesState InstallAttributesGetStatus() {
  absl::optional<::user_data_auth::InstallAttributesGetStatusReply> result =
      InstallAttributesClient::Get()->BlockingInstallAttributesGetStatus(
          ::user_data_auth::InstallAttributesGetStatusRequest());
  if (!result.has_value() ||
      result->error() !=
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(WARNING) << "Unable to fetch install attributes status, error: "
                 << (result.has_value() ? result->error() : -1);
    return ::user_data_auth::InstallAttributesState::UNKNOWN;
  }
  return result->state();
}

bool InstallAttributesIsInvalid() {
  user_data_auth::InstallAttributesState state = InstallAttributesGetStatus();
  return state == user_data_auth::InstallAttributesState::INVALID;
}

bool InstallAttributesIsFirstInstall() {
  user_data_auth::InstallAttributesState state = InstallAttributesGetStatus();
  return state == user_data_auth::InstallAttributesState::FIRST_INSTALL;
}

}  // namespace ash::install_attributes_util
