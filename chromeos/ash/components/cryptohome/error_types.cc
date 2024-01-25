// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/error_types.h"
#include "base/strings/string_number_conversions.h"

namespace cryptohome {

ErrorWrapper::ErrorWrapper(
    ::user_data_auth::CryptohomeErrorCode code,
    std::optional<::user_data_auth::CryptohomeErrorInfo> info)
    : code_(code), info_(info) {}

ErrorWrapper::ErrorWrapper(const ErrorWrapper& other)
    : ErrorWrapper(other.code(), other.info()) {}

ErrorWrapper::~ErrorWrapper() {}

ErrorWrapper ErrorWrapper::CreateFrom(
    ::user_data_auth::CryptohomeErrorCode code,
    ::user_data_auth::CryptohomeErrorInfo info) {
  return ErrorWrapper(code, info);
}

ErrorWrapper ErrorWrapper::CreateFromErrorCodeOnly(
    ::user_data_auth::CryptohomeErrorCode code) {
  return ErrorWrapper(code, std::nullopt);
}

std::ostream& operator<<(std::ostream& os, ErrorWrapper error) {
  os << "code=" << error.code() << " ";
  auto info = error.info();
  if (info.has_value()) {
    std::string possible_actions;
    for (int i = 0; i < info->possible_actions_size(); i++) {
      if (i != 0) {
        possible_actions += ",";
      }
      possible_actions +=
          base::NumberToString(static_cast<int>(info->possible_actions(i)));
    }

    os << "error_id=" << info->error_id();
    os << " primary_action=" +
              base::NumberToString(static_cast<int>(info->primary_action()));
    os << " possible_actions=" + possible_actions;
  } else {
    os << "info=nullopt";
  }

  return os;
}

}  // namespace cryptohome
