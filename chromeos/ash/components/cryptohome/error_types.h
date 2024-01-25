// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

namespace cryptohome {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) ErrorWrapper {
 public:
  ErrorWrapper(const ErrorWrapper& other);

  ~ErrorWrapper();

  ::user_data_auth::CryptohomeErrorCode code() const { return code_; }

  std::optional<::user_data_auth::CryptohomeErrorInfo> info() const {
    return info_;
  }

  static ErrorWrapper success() {
    ::user_data_auth::CryptohomeErrorInfo info;
    info.set_primary_action(::user_data_auth::PrimaryAction::PRIMARY_NO_ERROR);
    return ErrorWrapper(
        ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET, info);
  }

  // Creates an ErrorWrapper with both legacy CryptohomeErrorCode and the new
  // CryptohomeErrorInfo. This should be used if possible.
  static ErrorWrapper CreateFrom(::user_data_auth::CryptohomeErrorCode code,
                                 ::user_data_auth::CryptohomeErrorInfo info);
  // Creates an ErrorWrapper with only the legacy CryptohomeErrorCode. This is
  // only used during the migration and will be removed in the future.
  static ErrorWrapper CreateFromErrorCodeOnly(
      ::user_data_auth::CryptohomeErrorCode code);

 private:
  ErrorWrapper(::user_data_auth::CryptohomeErrorCode code,
               std::optional<::user_data_auth::CryptohomeErrorInfo> info);

  // code_ is the legacy CryptohomeErrorCode.
  ::user_data_auth::CryptohomeErrorCode code_;

  // info_ is the new way to represent Cryptohome Error.
  std::optional<::user_data_auth::CryptohomeErrorInfo> info_;
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
std::ostream& operator<<(std::ostream& os, ErrorWrapper error);

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_
