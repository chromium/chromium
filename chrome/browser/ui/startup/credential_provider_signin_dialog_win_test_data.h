// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_TEST_DATA_H_
#define CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_TEST_DATA_H_

#include <string>

#include "base/values.h"

// Class used to store common test data used to validate the functioning of the
// credential provider sign in dialog. This class stores the expected login
// complte information that the dialog is supposed to received from the gaia
// sign in as well as the expected values for any additional token / info
// fetches needed to complete the sign in using the credential provider.
// On a successful sign in result, we expect the final json result to match
// the json result generated from the internal expected_success_result_
// value.
class CredentialProviderSigninDialogTestDataStorage {
 public:
  CredentialProviderSigninDialogTestDataStorage();

  static base::Value MakeSignInResponseValue(
      const std::string& id = std::string(),
      const std::string& password = std::string(),
      const std::string& email = std::string(),
      const std::string& access_token = std::string(),
      const std::string& refresh_token = std::string());
  base::Value MakeValidSignInResponseValue() const;

  void SetSigninPassword(const std::string& password);

  std::string GetSuccessId() const {
    return expected_success_signin_result_.FindKey("id")->GetString();
  }
  std::string GetSuccessPassword() const {
    return expected_success_signin_result_.FindKey("password")->GetString();
  }
  std::string GetSuccessEmail() const {
    return expected_success_signin_result_.FindKey("email")->GetString();
  }
  std::string GetSuccessAccessToken() const {
    return expected_success_signin_result_.FindKey("access_token")->GetString();
  }
  std::string GetSuccessRefreshToken() const {
    return expected_success_signin_result_.FindKey("refresh_token")
        ->GetString();
  }
  std::string GetSuccessTokenHandle() const {
    return expected_success_fetch_result_.FindKey("token_handle")->GetString();
  }
  std::string GetSuccessMdmIdToken() const {
    return expected_success_fetch_result_.FindKey("mdm_id_token")->GetString();
  }
  std::string GetSuccessMdmAccessToken() const {
    return expected_success_fetch_result_.FindKey("mdm_access_token")
        ->GetString();
  }
  std::string GetSuccessFullName() const {
    return expected_success_fetch_result_.FindKey("full_name")->GetString();
  }

  const base::Value& expected_signin_result() const {
    return expected_success_signin_result_;
  }

  const base::Value& expected_success_fetch_result() const {
    return expected_success_fetch_result_;
  }

  const base::Value& expected_full_result() const {
    return expected_success_full_result_;
  }

  bool EqualsSuccessfulSigninResult(const base::Value& result_value) const {
    return EqualsEncodedValue(expected_signin_result(), result_value);
  }
  bool EqualsSccessfulFetchResult(const base::Value& result_value) const {
    return EqualsEncodedValue(expected_success_fetch_result(), result_value);
  }

  std::string GetSuccessfulSigninResult() const;
  std::string GetSuccessfulSigninTokenFetchResult() const;
  std::string GetSuccessfulMdmTokenFetchResult() const;
  std::string GetSuccessfulTokenInfoFetchResult() const;
  std::string GetSuccessfulUserInfoFetchResult() const;

  static const char kInvalidTokenInfoResponse[];
  static const char kInvalidUserInfoResponse[];
  static const char kInvalidTokenFetchResponse[];
  static const char kInvalidEmailForSignin[];

 private:
  bool EqualsEncodedValue(const base::Value& success_value,
                          const base::Value& result_value) const {
    return result_value == success_value;
  }

  // An expected successful result from chrome://inline-signin.
  base::Value expected_success_signin_result_;

  // An expected successful result from oauth2 fetches for user info, token
  // handle, and id token.
  base::Value expected_success_fetch_result_;

  // An expected successful full result sent to the credential provider from
  // GLS.
  base::Value expected_success_full_result_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_TEST_DATA_H_
