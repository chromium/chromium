// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CHECK_PASSWORDS_AGAINST_CRYPTOHOME_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CHECK_PASSWORDS_AGAINST_CRYPTOHOME_HELPER_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/login/base_screen_handler_utils.h"

namespace ash {

class CheckPasswordsAgainstCryptohomeHelper : public AuthStatusConsumer {
 public:
  using OnCheckPasswordsAgainstCryptohomeHelperFailureCallback =
      base::OnceCallback<void()>;
  using OnCheckPasswordsAgainstCryptohomeHelperSuccessCallback =
      base::OnceCallback<void(const std::string&)>;

  CheckPasswordsAgainstCryptohomeHelper(
      const UserContext& user_context,
      const ::login::StringList& scraped_passwords,
      OnCheckPasswordsAgainstCryptohomeHelperFailureCallback
          on_check_passwords_against_cryptohome_helper_failure_callback,
      OnCheckPasswordsAgainstCryptohomeHelperSuccessCallback
          on_check_passwords_against_cryptohome_helper_success_callback);

  CheckPasswordsAgainstCryptohomeHelper(const CheckPasswordsAgainstCryptohomeHelper&) =
      delete;
  CheckPasswordsAgainstCryptohomeHelper& operator=(
      const CheckPasswordsAgainstCryptohomeHelper&) = delete;

  ~CheckPasswordsAgainstCryptohomeHelper() override;

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;

 private:
  UserContext user_context_;
  const ::login::StringList scraped_passwords_;
  size_t current_password_index_ = 0u;

  OnCheckPasswordsAgainstCryptohomeHelperFailureCallback
      on_check_passwords_against_cryptohome_helper_failure_callback_;
  OnCheckPasswordsAgainstCryptohomeHelperSuccessCallback
      on_check_passwords_against_cryptohome_helper_success_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CHECK_PASSWORDS_AGAINST_CRYPTOHOME_HELPER_H_
