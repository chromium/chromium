// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/check_passwords_against_cryptohome_helper.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {
namespace {

// Maximum number of attempts to check scraped password against cryptohome.
const size_t kMaximumNumberOfAttempts = 5;

void SetKeyForUserContext(UserContext& user_context,
                          const std::string& password) {
  Key key(password);
  key.SetLabel(kCryptohomeGaiaKeyLabel);
  user_context.SetKey(key);
  user_context.SetSamlPassword(SamlPassword{password});
  user_context.SetPasswordKey(Key(password));
}

}  // namespace

CheckPasswordsAgainstCryptohomeHelper::CheckPasswordsAgainstCryptohomeHelper(
    const UserContext& user_context,
    const ::login::StringList& scraped_passwords,
    OnCheckPasswordsAgainstCryptohomeHelperFailureCallback
        on_check_passwords_against_cryptohome_helper_failure_callback,
    OnCheckPasswordsAgainstCryptohomeHelperSuccessCallback
        on_check_passwords_against_cryptohome_helper_success_callback)
    : user_context_(user_context),
      scraped_passwords_(scraped_passwords),
      on_check_passwords_against_cryptohome_helper_failure_callback_(std::move(
          on_check_passwords_against_cryptohome_helper_failure_callback)),
      on_check_passwords_against_cryptohome_helper_success_callback_(std::move(
          on_check_passwords_against_cryptohome_helper_success_callback)) {
  current_password_index_ = 0u;
  SetKeyForUserContext(user_context_,
                       scraped_passwords_[current_password_index_]);
  // TODO(crbug.com/40214270): Find some way to check the passwords.
}

CheckPasswordsAgainstCryptohomeHelper::
    ~CheckPasswordsAgainstCryptohomeHelper() = default;

void CheckPasswordsAgainstCryptohomeHelper::OnAuthFailure(
    const AuthFailure& error) {
  current_password_index_++;
  if (current_password_index_ == kMaximumNumberOfAttempts ||
      current_password_index_ == scraped_passwords_.size()) {
    current_password_index_ = 0u;
    std::move(on_check_passwords_against_cryptohome_helper_failure_callback_)
        .Run();
    return;
  }

  SetKeyForUserContext(user_context_,
                       scraped_passwords_[current_password_index_]);
  // TODO(crbug.com/40214270): Find some way to check the passwords.
}

void CheckPasswordsAgainstCryptohomeHelper::OnAuthSuccess(
    const UserContext& user_context) {
  std::move(on_check_passwords_against_cryptohome_helper_success_callback_)
      .Run(user_context.GetPasswordKey()->GetSecret());
}

}  // namespace ash
