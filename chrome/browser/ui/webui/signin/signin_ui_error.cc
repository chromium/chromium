// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_ui_error.h"

#include <map>
#include <tuple>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

// ------------------------------ SigninUIError --------------------------------

// static
SigninUIError SigninUIError::Ok() {
  return SigninUIError(Type::kOk, std::string(), std::u16string());
}

// static
SigninUIError SigninUIError::Other(const std::string& email) {
  return SigninUIError(Type::kOther, email, std::u16string());
}

// static
SigninUIError SigninUIError::UsernameNotAllowedByPatternFromPrefs(
    const std::string& email) {
  return SigninUIError(
      Type::kUsernameNotAllowedByPatternFromPrefs, email,
      l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_NAME_PROHIBITED));
}

// static
SigninUIError SigninUIError::WrongReauthAccount(
    const std::string& email,
    const std::string& current_email) {
  return SigninUIError(
      Type::kWrongReauthAccount, email,
      l10n_util::GetStringFUTF16(IDS_SYNC_WRONG_EMAIL,
                                 base::UTF8ToUTF16(current_email)));
}

// static
SigninUIError SigninUIError::AccountAlreadyUsedByAnotherProfile(
    const std::string& email,
    const base::FilePath& another_profile_path) {
  SigninUIError error(
      Type::kAccountAlreadyUsedByAnotherProfile, email,
      l10n_util::GetStringUTF16(IDS_SYNC_USER_NAME_IN_USE_ERROR));
  error.another_profile_path_ = another_profile_path;
  return error;
}

// static
SigninUIError SigninUIError::ProfileWasUsedByAnotherAccount(
    const std::string& email,
    const std::string& last_email) {
  return SigninUIError(
      Type::kProfileWasUsedByAnotherAccount, email,
      l10n_util::GetStringFUTF16(IDS_SYNC_USED_PROFILE_ERROR,
                                 base::UTF8ToUTF16(last_email)));
}

// static
SigninUIError SigninUIError::FromGoogleServiceAuthError(
    const std::string& email,
    const GoogleServiceAuthError& error) {
  return SigninUIError(Type::kFromGoogleServiceAuthError, email,
                       base::UTF8ToUTF16(error.ToString()));
}

#if BUILDFLAG(IS_WIN)
// static
SigninUIError SigninUIError::FromCredentialProviderUiExitCode(
    const std::string& email,
    credential_provider::UiExitCodes exit_code) {
  SigninUIError error(Type::kFromCredentialProviderUiExitCode, email,
                      base::NumberToString16(exit_code));
  error.credential_provider_exit_code_ = exit_code;
  return error;
}
#endif

// static
SigninUIError SigninUIError::ProfileIsBlocked() {
  return SigninUIError(Type::kProfileIsBlocked, /*email=*/std::string(),
                       /*error_message=*/std::u16string());
}

SigninUIError::SigninUIError(const SigninUIError& other) = default;
SigninUIError& SigninUIError::operator=(const SigninUIError& other) = default;

bool SigninUIError::IsOk() const {
  return type_ == Type::kOk;
}

SigninUIError::Type SigninUIError::type() const {
  return type_;
}

const std::u16string& SigninUIError::email() const {
  return email_;
}

const std::u16string& SigninUIError::message() const {
  return message_;
}

const base::FilePath& SigninUIError::another_profile_path() const {
  DCHECK(type() == Type::kAccountAlreadyUsedByAnotherProfile);
  return another_profile_path_;
}

#if BUILDFLAG(IS_WIN)
credential_provider::UiExitCodes SigninUIError::credential_provider_exit_code()
    const {
  DCHECK(type() == Type::kFromCredentialProviderUiExitCode);
  return credential_provider_exit_code_;
}
#endif

bool SigninUIError::operator==(const SigninUIError& other) const {
  bool result = std::tie(type_, email_, message_, another_profile_path_) ==
                std::tie(other.type_, other.email_, other.message_,
                         other.another_profile_path_);
#if BUILDFLAG(IS_WIN)
  result = result && credential_provider_exit_code_ ==
                         other.credential_provider_exit_code_;
#endif
  return result;
}

bool SigninUIError::operator!=(const SigninUIError& other) const {
  return !(*this == other);
}

SigninUIError::SigninUIError(Type type,
                             const std::string& email,
                             const std::u16string& error_message)
    : type_(type), email_(base::UTF8ToUTF16(email)), message_(error_message) {}

// ---------------------------- ForceSigninUIError -----------------------------

ForceSigninUIError::ForceSigninUIError(const ForceSigninUIError& other) =
    default;
ForceSigninUIError& ForceSigninUIError::operator=(
    const ForceSigninUIError& other) = default;

// static
ForceSigninUIError ForceSigninUIError::ErrorNone() {
  return ForceSigninUIError(Type::kNone, std::string());
}

// static
ForceSigninUIError ForceSigninUIError::ReauthNotAllowed() {
  return ForceSigninUIError(Type::kReauthNotAllowed, std::string());
}

// static
ForceSigninUIError ForceSigninUIError::ReauthWrongAccount(
    const std::string& email) {
  CHECK(!email.empty());
  return ForceSigninUIError(Type::kReauthWrongAccount, email);
}

// static
ForceSigninUIError ForceSigninUIError::ReauthTimeout() {
  return ForceSigninUIError(Type::kReauthTimeout, std::string());
}

// static
ForceSigninUIError ForceSigninUIError::SigninPatternNotMatching(
    const std::string& email) {
  CHECK(!email.empty());
  return ForceSigninUIError(Type::kSigninPatternNotMatching, email);
}

ForceSigninUIError::UiTexts ForceSigninUIError::GetErrorTexts() const {
  CHECK_NE(type_, Type::kNone);
  switch (type_) {
    case Type::kReauthNotAllowed:
      return {
          l10n_util::GetStringUTF16(
              IDS_PROFILE_PICKER_FORCE_SIGN_IN_ERROR_DIALOG_NOT_ALLOWED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_PROFILE_PICKER_FORCE_SIGN_IN_ERROR_DIALOG_NOT_ALLOWED_BODY)};
    case Type::kReauthWrongAccount:
      CHECK(!email_.empty());
      return {
          l10n_util::GetStringUTF16(
              IDS_PROFILE_PICKER_FORCE_SIGN_IN_ERROR_DIALOG_WRONG_ACCOUNT_TITLE),
          l10n_util::GetStringFUTF16(
              IDS_PROFILE_PICKER_FORCE_SIGN_IN_ERROR_DIALOG_WRONG_ACCOUNT_BODY,
              base::UTF8ToUTF16(email_))};
    case Type::kReauthTimeout:
      return {l10n_util::GetStringUTF16(
                  IDS_PROFILE_PICKER_FORCE_SIGN_IN_ERROR_TIMEOUT_TITLE),
              l10n_util::GetStringUTF16(
                  IDS_PROFILE_PICKER_FORCE_SIGN_IN_ERROR_TIMEOUT_BODY)};
    case Type::kSigninPatternNotMatching:
      CHECK(!email_.empty());
      return {l10n_util::GetStringFUTF16(IDS_SIGNIN_ERROR_EMAIL_TITLE,
                                         base::UTF8ToUTF16(email_)),
              l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_NAME_PROHIBITED)};
    case Type::kNone:
      NOTREACHED();
  }
}

ForceSigninUIError::Type ForceSigninUIError::type() const {
  return type_;
}

ForceSigninUIError::ForceSigninUIError(Type type, const std::string& email)
    : type_(type), email_(email) {}
