// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_TEST_DATA_H_
#define CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_TEST_DATA_H_

#include <string>

#include "base/values.h"

// Class used to store common test data used to validate the functioning of the
// credential provider sign in dialog. This class stores the expected login
// complete information that the dialog is supposed to received from the gaia
// sign in as well as the expected values for any additional token / info
// fetches needed to complete the sign in using the credential provider.
// On a successful sign in result, we expect the final json result to match
// the json result generated from the internal expected_success_result_
// value.
class CredentialProviderSigninDialogTestDataStorage {
 public:
  CredentialProviderSigninDialogTestDataStorage();

  static base::Value::Dict MakeSignInResponseValue(
      const std::string& id = std::string(),
      const std::string& password = std::string(),
      const std::string& email = std::string(),
      const std::string& access_token = std::string(),
      const std::string& refresh_token = std::string());
  base::Value::Dict MakeValidSignInResponseValue() const;

  void SetSigninPassword(const std::string& password);

  const std::string& GetSuccessId() const {
    return expected_success_signin_result_.Find("id")->GetString();
  }
  const std::string& GetSuccessPassword() const {
    return expected_success_signin_result_.Find("password")->GetString();
  }
  const std::string& GetSuccessEmail() const {
    return expected_success_signin_result_.Find("email")->GetString();
  }
  const std::string& GetSuccessAccessToken() const {
    return expected_success_signin_result_.Find("access_token")->GetString();
  }
  const std::string& GetSuccessRefreshToken() const {
    return expected_success_signin_result_.Find("refresh_token")->GetString();
  }
  const std::string& GetSuccessTokenHandle() const {
    return expected_success_fetch_result_.Find("token_handle")->GetString();
  }
  const std::string& GetSuccessMdmIdToken() const {
    return expected_success_fetch_result_.Find("mdm_id_token")->GetString();
  }
  const std::string& GetSuccessMdmAccessToken() const {
    return expected_success_fetch_result_.Find("mdm_access_token")->GetString();
  }
  const std::string& GetSuccessFullName() const {
    return expected_success_fetch_result_.Find("full_name")->GetString();
  }

  const base::Value::Dict& expected_signin_result() const {
    return expected_success_signin_result_;
  }

  const base::Value::Dict& expected_success_fetch_result() const {
    return expected_success_fetch_result_;
  }

  const base::Value::Dict& expected_full_result() const {
    return expected_success_full_result_;
  }

  bool EqualsSuccessfulSigninResult(
      const base::Value::Dict& result_value) const {
    return EqualsEncodedValue(expected_signin_result(), result_value);
  }
  bool EqualsSccessfulFetchResult(const base::Value::Dict& result_value) const {
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
  bool EqualsEncodedValue(const base::Value::Dict& success_value,
                          const base::Value::Dict& result_value) const {
    return result_value == success_value;
  }

  // An expected successful result from chrome://inline-signin.
  base::Value::Dict expected_success_signin_result_;

  // An expected successful result from oauth2 fetches for user info, token
  // handle, and id token.
  base::Value::Dict expected_success_fetch_result_;

  // An expected successful full result sent to the credential provider from
  // GLS.
  base::Value::Dict expected_success_full_result_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_TEST_DATA_H_
