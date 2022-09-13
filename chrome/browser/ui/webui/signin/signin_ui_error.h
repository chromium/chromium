// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UI_ERROR_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UI_ERROR_H_

#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/credential_provider/common/gcp_strings.h"
#endif

class GoogleServiceAuthError;

// Holds different sign-in error types along with error messages for displaying
// in the UI.
class SigninUIError {
 public:
  // An error type.
  // Different types of UI might be shown for different error types.
  enum class Type {
    kOk,
    kOther,
    kUsernameNotAllowedByPatternFromPrefs,
    kWrongReauthAccount,
    kAccountAlreadyUsedByAnotherProfile,
    kProfileWasUsedByAnotherAccount,
    kFromGoogleServiceAuthError,
    kFromCredentialProviderUiExitCode,
    kProfileIsBlocked,
  };

  // Following static functions construct a `SigninUIError` with a corresponding
  // type and error message.
  static SigninUIError Ok();
  static SigninUIError Other(const std::string& email);
  static SigninUIError UsernameNotAllowedByPatternFromPrefs(
      const std::string& email);
  static SigninUIError WrongReauthAccount(const std::string& email,
                                          const std::string& current_email);
  static SigninUIError AccountAlreadyUsedByAnotherProfile(
      const std::string& email,
      const base::FilePath& another_profile_path);
  static SigninUIError ProfileWasUsedByAnotherAccount(
      const std::string& email,
      const std::string& last_email);
  static SigninUIError FromGoogleServiceAuthError(
      const std::string& email,
      const GoogleServiceAuthError& error);
#if BUILDFLAG(IS_WIN)
  static SigninUIError FromCredentialProviderUiExitCode(
      const std::string& email,
      credential_provider::UiExitCodes exit_code);
#endif
  static SigninUIError ProfileIsBlocked();

  SigninUIError(const SigninUIError& other);
  SigninUIError& operator=(const SigninUIError& other);

  // Returns true if the instance contains a non-error type.
  bool IsOk() const;

  Type type() const;
  const std::u16string& email() const;
  const std::u16string& message() const;

  // Should be called only if `type()` ==
  // `Type::kAccountAlreadyUsedByAnotherProfile`.
  const base::FilePath& another_profile_path() const;

#if BUILDFLAG(IS_WIN)
  // Should be called only if `type()` ==
  // `Type::kFromCredentialProviderUiExitCode`.
  credential_provider::UiExitCodes credential_provider_exit_code() const;
#endif

  bool operator==(const SigninUIError& other) const;
  bool operator!=(const SigninUIError& other) const;

 private:
  SigninUIError(Type type,
                const std::string& email,
                const std::u16string& error_message);

  // Don't forget to update operator==() when adding new class members.
  Type type_;
  std::u16string email_;
  std::u16string message_;

  // Defined only for Type::kAccountAlreadyUsedByAnotherProfile.
  base::FilePath another_profile_path_;

#if BUILDFLAG(IS_WIN)
  // Defined only for Type::kFromCredentialProviderUiExitCode.
  credential_provider::UiExitCodes credential_provider_exit_code_ =
      credential_provider::UiExitCodes::kUiecSuccess;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UI_ERROR_H_
