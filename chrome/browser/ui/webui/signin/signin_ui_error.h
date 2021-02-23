/// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UI_ERROR_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UI_ERROR_H_

#include <string>

#include "base/strings/string16.h"

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
      const std::string& email);
  static SigninUIError ProfileWasUsedByAnotherAccount(
      const std::string& email,
      const std::string& last_email);

  // Returns true if the instance contains a non-error type.
  bool IsOk() const;

  Type type() const;
  const base::string16& email() const;
  const base::string16& message() const;

  bool operator==(const SigninUIError& other) const;
  bool operator!=(const SigninUIError& other) const;

 private:
  SigninUIError(Type type,
                const std::string& email,
                const base::string16& error_message);

  // Don't forget to update operator==() when adding new class members.
  Type type_;
  base::string16 email_;
  base::string16 message_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UI_ERROR_H_
