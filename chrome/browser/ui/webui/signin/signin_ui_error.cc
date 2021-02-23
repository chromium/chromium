/// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_ui_error.h"

#include <tuple>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
SigninUIError SigninUIError::Ok() {
  return SigninUIError(Type::kOk, std::string(), base::string16());
}

// static
SigninUIError SigninUIError::Other(const std::string& email) {
  return SigninUIError(Type::kOther, email, base::string16());
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
    const std::string& email) {
  return SigninUIError(
      Type::kAccountAlreadyUsedByAnotherProfile, email,
      l10n_util::GetStringUTF16(IDS_SYNC_USER_NAME_IN_USE_ERROR));
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

bool SigninUIError::IsOk() const {
  return type_ == Type::kOk;
}

SigninUIError::Type SigninUIError::type() const {
  return type_;
}

const base::string16& SigninUIError::email() const {
  return email_;
}

const base::string16& SigninUIError::message() const {
  return message_;
}

bool SigninUIError::operator==(const SigninUIError& other) const {
  return std::tie(type_, email_, message_) ==
         std::tie(other.type_, other.email_, other.message_);
}

bool SigninUIError::operator!=(const SigninUIError& other) const {
  return !(*this == other);
}

SigninUIError::SigninUIError(Type type,
                             const std::string& email,
                             const base::string16& error_message)
    : type_(type), email_(base::UTF8ToUTF16(email)), message_(error_message) {}
