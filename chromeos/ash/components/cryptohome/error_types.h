// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_

#include <iosfwd>
#include <memory>
#include <optional>

#include "base/component_export.h"

namespace user_data_auth {
class CryptohomeErrorInfo;
enum CryptohomeErrorCode : int;
}  // namespace user_data_auth

namespace cryptohome {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) ErrorWrapper {
 public:
  ErrorWrapper(const ErrorWrapper& other);
  ErrorWrapper(ErrorWrapper&& other) noexcept;
  ErrorWrapper& operator=(const ErrorWrapper& other);
  ErrorWrapper& operator=(ErrorWrapper&& other) noexcept;

  ~ErrorWrapper();

  ::user_data_auth::CryptohomeErrorCode code() const { return code_; }

  std::optional<::user_data_auth::CryptohomeErrorInfo> info() const;

  static ErrorWrapper success();

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
               std::unique_ptr<::user_data_auth::CryptohomeErrorInfo> info);

  // code_ is the legacy CryptohomeErrorCode.
  ::user_data_auth::CryptohomeErrorCode code_;

  // info_ is the new way to represent Cryptohome Error. Null if not set.
  std::unique_ptr<::user_data_auth::CryptohomeErrorInfo> info_;
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
std::ostream& operator<<(std::ostream& os, const ErrorWrapper& error);

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_
