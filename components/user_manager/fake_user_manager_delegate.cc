// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/fake_user_manager_delegate.h"

#include <string>

#include "base/no_destructor.h"
#include "base/notimplemented.h"

namespace user_manager {

FakeUserManagerDelegate::FakeUserManagerDelegate() = default;
FakeUserManagerDelegate::~FakeUserManagerDelegate() = default;

const std::string& FakeUserManagerDelegate::GetApplicationLocale() {
  static const base::NoDestructor<std::string> default_locale("en-US");
  return *default_locale;
}

void FakeUserManagerDelegate::OverrideDirHome(const User& primary_user) {
  NOTIMPLEMENTED();
}

bool FakeUserManagerDelegate::IsUserSessionRestoreInProgress() {
  return false;
}

std::optional<UserType> FakeUserManagerDelegate::GetDeviceLocalAccountUserType(
    std::string_view email) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void FakeUserManagerDelegate::CheckProfileOnLogin(const User& user) {
  // Do nothing.
}

void FakeUserManagerDelegate::RemoveProfileByAccountId(
    const AccountId& account_id) {
  // Do nothing.
}

void FakeUserManagerDelegate::RemoveCryptohomeAsync(
    const AccountId& account_id) {
  // Do nothing.
}

}  // namespace user_manager
