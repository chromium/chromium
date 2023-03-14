// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/credential_provider/common/gcp_strings.h"

#include <string>

#include "base/json/json_writer.h"

const char
    CredentialProviderSigninDialogTestDataStorage::kInvalidTokenInfoResponse[] =
        "{"
        "  \"error\": \"invalid_token\""
        "}";
const char
    CredentialProviderSigninDialogTestDataStorage::kInvalidUserInfoResponse[] =
        "{"
        "  \"error\": \"invalid_token\""
        "}";
const char CredentialProviderSigninDialogTestDataStorage::
    kInvalidTokenFetchResponse[] =
        "{"
        "  \"error\": \"invalid_token\""
        "}";

const char
    CredentialProviderSigninDialogTestDataStorage::kInvalidEmailForSignin[] =
        "foo_bar@example.com";
CredentialProviderSigninDialogTestDataStorage::
    CredentialProviderSigninDialogTestDataStorage() {
  SetSigninPassword("password");
}

void CredentialProviderSigninDialogTestDataStorage::SetSigninPassword(
    const std::string& password) {
  // Initialize expected successful result from oauth2 fetches.
  expected_success_fetch_result_.Set(credential_provider::kKeyTokenHandle,
                                     base::Value("token_handle"));
  expected_success_fetch_result_.Set(credential_provider::kKeyMdmIdToken,
                                     base::Value("mdm_token"));
  expected_success_fetch_result_.Set(credential_provider::kKeyFullname,
                                     base::Value("Foo Bar"));
  expected_success_fetch_result_.Set(credential_provider::kKeyMdmAccessToken,
                                     base::Value("123456789"));

  // Initialize expected successful full result sent to the credential provider.
  expected_success_signin_result_.Set(credential_provider::kKeyId,
                                      base::Value("gaia_user_id"));
  expected_success_signin_result_.Set(credential_provider::kKeyPassword,
                                      base::Value(password));
  expected_success_signin_result_.Set(credential_provider::kKeyEmail,
                                      base::Value("foo_bar@gmail.com"));
  expected_success_signin_result_.Set(credential_provider::kKeyAccessToken,
                                      base::Value("access_token"));
  expected_success_signin_result_.Set(credential_provider::kKeyRefreshToken,
                                      base::Value("refresh_token"));
  expected_success_signin_result_.Set(
      credential_provider::kKeyExitCode,
      base::Value(credential_provider::kUiecSuccess));

  // Merge with results from chrome://inline-signin to form the full
  // result.
  expected_success_full_result_ = expected_success_signin_result_.Clone();
  expected_success_full_result_.Merge(expected_success_fetch_result_.Clone());
}

// static
base::Value::Dict
CredentialProviderSigninDialogTestDataStorage::MakeSignInResponseValue(
    const std::string& id,
    const std::string& password,
    const std::string& email,
    const std::string& access_token,
    const std::string& refresh_token) {
  base::Value::Dict args;
  if (!email.empty())
    args.Set(credential_provider::kKeyEmail, email);
  if (!password.empty())
    args.Set(credential_provider::kKeyPassword, password);
  if (!id.empty())
    args.Set(credential_provider::kKeyId, id);
  if (!refresh_token.empty())
    args.Set(credential_provider::kKeyRefreshToken, refresh_token);
  if (!access_token.empty())
    args.Set(credential_provider::kKeyAccessToken, access_token);

  args.Set(credential_provider::kKeyExitCode,
           credential_provider::kUiecSuccess);
  return args;
}

base::Value::Dict
CredentialProviderSigninDialogTestDataStorage::MakeValidSignInResponseValue()
    const {
  return MakeSignInResponseValue(GetSuccessId(), GetSuccessPassword(),
                                 GetSuccessEmail(), GetSuccessAccessToken(),
                                 GetSuccessRefreshToken());
}

std::string
CredentialProviderSigninDialogTestDataStorage::GetSuccessfulSigninResult()
    const {
  std::string json;
  base::JSONWriter::Write(expected_success_signin_result_, &json);
  return json;
}

std::string CredentialProviderSigninDialogTestDataStorage::
    GetSuccessfulSigninTokenFetchResult() const {
  return "{"
         "  \"access_token\": \"" +
         GetSuccessAccessToken() +
         "\","
         "  \"refresh_token\": \"" +
         GetSuccessRefreshToken() +
         "\","
         "  \"id_token\": \"signin_id_token\","
         "  \"expires_in\": 1800"
         "}";
}

std::string CredentialProviderSigninDialogTestDataStorage::
    GetSuccessfulMdmTokenFetchResult() const {
  return "{"
         "  \"access_token\": \"" +
         GetSuccessMdmAccessToken() +
         "\","
         "  \"refresh_token\": \"mdm_refresh_token\","
         "  \"id_token\": \"" +
         GetSuccessMdmIdToken() +
         "\","
         "  \"expires_in\": 1800"
         "}";
}

std::string CredentialProviderSigninDialogTestDataStorage::
    GetSuccessfulTokenInfoFetchResult() const {
  return "{"
         "  \"audience\": \"blah.apps.googleusercontent.blah.com\","
         "  \"used_id\": \"1234567890\","
         "  \"scope\": \"all the things\","
         "  \"expires_in\": 1800,"
         "  \"token_type\": \"Bearer\","
         "  \"token_handle\": \"" +
         GetSuccessTokenHandle() +
         "\""
         "}";
}

std::string CredentialProviderSigninDialogTestDataStorage::
    GetSuccessfulUserInfoFetchResult() const {
  return "{"
         "  \"name\": \"" +
         GetSuccessFullName() +
         "\""
         "}";
}
