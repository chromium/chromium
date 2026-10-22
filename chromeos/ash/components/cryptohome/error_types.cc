// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/error_types.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

namespace cryptohome {

namespace {

std::unique_ptr<::user_data_auth::CryptohomeErrorInfo> CloneInfo(
    const std::unique_ptr<::user_data_auth::CryptohomeErrorInfo>& info) {
  if (!info) {
    return nullptr;
  }
  return std::make_unique<::user_data_auth::CryptohomeErrorInfo>(*info);
}

}  // namespace

ErrorWrapper::ErrorWrapper(
    ::user_data_auth::CryptohomeErrorCode code,
    std::unique_ptr<::user_data_auth::CryptohomeErrorInfo> info)
    : code_(code), info_(std::move(info)) {}

ErrorWrapper::ErrorWrapper(const ErrorWrapper& other)
    : ErrorWrapper(other.code_, CloneInfo(other.info_)) {}

ErrorWrapper::ErrorWrapper(ErrorWrapper&& other) noexcept = default;

ErrorWrapper& ErrorWrapper::operator=(const ErrorWrapper& other) {
  if (this != &other) {
    code_ = other.code_;
    info_ = CloneInfo(other.info_);
  }
  return *this;
}

ErrorWrapper& ErrorWrapper::operator=(ErrorWrapper&& other) noexcept = default;

ErrorWrapper::~ErrorWrapper() = default;

std::optional<::user_data_auth::CryptohomeErrorInfo> ErrorWrapper::info()
    const {
  if (!info_) {
    return std::nullopt;
  }
  return *info_;
}

// static
ErrorWrapper ErrorWrapper::success() {
  auto info = std::make_unique<::user_data_auth::CryptohomeErrorInfo>();
  info->set_primary_action(::user_data_auth::PrimaryAction::PRIMARY_NO_ERROR);
  return ErrorWrapper(
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET,
      std::move(info));
}

ErrorWrapper ErrorWrapper::CreateFrom(
    ::user_data_auth::CryptohomeErrorCode code,
    ::user_data_auth::CryptohomeErrorInfo info) {
  return ErrorWrapper(
      code,
      std::make_unique<::user_data_auth::CryptohomeErrorInfo>(std::move(info)));
}

ErrorWrapper ErrorWrapper::CreateFromErrorCodeOnly(
    ::user_data_auth::CryptohomeErrorCode code) {
  return ErrorWrapper(code, nullptr);
}

std::ostream& operator<<(std::ostream& os, const ErrorWrapper& error) {
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
