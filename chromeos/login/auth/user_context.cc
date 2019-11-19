// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"

namespace chromeos {

UserContext::UserContext() : account_id_(EmptyAccountId()) {}

UserContext::UserContext(const UserContext& other) = default;

UserContext::UserContext(const user_manager::User& user)
    : account_id_(user.GetAccountId()), user_type_(user.GetType()) {
  if (user_type_ == user_manager::USER_TYPE_REGULAR) {
    account_id_.SetUserEmail(
        user_manager::CanonicalizeUserID(account_id_.GetUserEmail()));
  }
}

UserContext::UserContext(user_manager::UserType user_type,
                         const AccountId& account_id)
    : account_id_(account_id), user_type_(user_type) {
  if (user_type_ == user_manager::USER_TYPE_REGULAR)
    account_id_.SetUserEmail(
        user_manager::CanonicalizeUserID(account_id_.GetUserEmail()));
}

UserContext::~UserContext() = default;

bool UserContext::operator==(const UserContext& context) const {
  return context.account_id_ == account_id_ && context.key_ == key_ &&
         context.auth_code_ == auth_code_ &&
         context.refresh_token_ == refresh_token_ &&
         context.access_token_ == access_token_ &&
         context.user_id_hash_ == user_id_hash_ &&
         context.is_using_oauth_ == is_using_oauth_ &&
         context.auth_flow_ == auth_flow_ && context.user_type_ == user_type_ &&
         context.public_session_locale_ == public_session_locale_ &&
         context.public_session_input_method_ == public_session_input_method_;
}

bool UserContext::operator!=(const UserContext& context) const {
  return !(*this == context);
}

const AccountId& UserContext::GetAccountId() const {
  return account_id_;
}

const std::string& UserContext::GetGaiaID() const {
  return account_id_.GetGaiaId();
}

const Key* UserContext::GetKey() const {
  return &key_;
}

Key* UserContext::GetKey() {
  return &key_;
}

const Key* UserContext::GetPasswordKey() const {
  return &password_key_;
}

Key* UserContext::GetMutablePasswordKey() {
  return &password_key_;
}

const std::vector<ChallengeResponseKey>& UserContext::GetChallengeResponseKeys()
    const {
  return challenge_response_keys_;
}

std::vector<ChallengeResponseKey>*
UserContext::GetMutableChallengeResponseKeys() {
  return &challenge_response_keys_;
}

const std::string& UserContext::GetAuthCode() const {
  return auth_code_;
}

const std::string& UserContext::GetRefreshToken() const {
  return refresh_token_;
}

const std::string& UserContext::GetAccessToken() const {
  return access_token_;
}

const std::string& UserContext::GetUserIDHash() const {
  return user_id_hash_;
}

bool UserContext::IsUsingOAuth() const {
  return is_using_oauth_;
}

bool UserContext::IsUsingPin() const {
  return is_using_pin_;
}

bool UserContext::IsForcingDircrypto() const {
  return is_forcing_dircrypto_;
}

UserContext::AuthFlow UserContext::GetAuthFlow() const {
  return auth_flow_;
}

bool UserContext::IsUsingSamlPrincipalsApi() const {
  return is_using_saml_principals_api_;
}

user_manager::UserType UserContext::GetUserType() const {
  return user_type_;
}

const std::string& UserContext::GetPublicSessionLocale() const {
  return public_session_locale_;
}

const std::string& UserContext::GetPublicSessionInputMethod() const {
  return public_session_input_method_;
}

const std::string& UserContext::GetDeviceId() const {
  return device_id_;
}

const std::string& UserContext::GetGAPSCookie() const {
  return gaps_cookie_;
}

const base::Optional<password_manager::PasswordHashData>&
UserContext::GetSyncPasswordData() const {
  return sync_password_data_;
}

const base::Optional<SamlPasswordAttributes>&
UserContext::GetSamlPasswordAttributes() const {
  return saml_password_attributes_;
}

bool UserContext::HasCredentials() const {
  return (account_id_.is_valid() && !key_.GetSecret().empty()) ||
         !auth_code_.empty();
}

bool UserContext::IsUnderAdvancedProtection() const {
  return is_under_advanced_protection_;
}

void UserContext::SetAccountId(const AccountId& account_id) {
  account_id_ = account_id;
}

void UserContext::SetKey(const Key& key) {
  key_ = key;
}

void UserContext::SetPasswordKey(const Key& key) {
  password_key_ = key;
}

void UserContext::SetAuthCode(const std::string& auth_code) {
  auth_code_ = auth_code;
}

void UserContext::SetRefreshToken(const std::string& refresh_token) {
  refresh_token_ = refresh_token;
}

void UserContext::SetAccessToken(const std::string& access_token) {
  access_token_ = access_token;
}

void UserContext::SetUserIDHash(const std::string& user_id_hash) {
  user_id_hash_ = user_id_hash;
}

void UserContext::SetIsUsingOAuth(bool is_using_oauth) {
  is_using_oauth_ = is_using_oauth;
}

void UserContext::SetIsUsingPin(bool is_using_pin) {
  is_using_pin_ = is_using_pin;
}

void UserContext::SetIsForcingDircrypto(bool is_forcing_dircrypto) {
  is_forcing_dircrypto_ = is_forcing_dircrypto;
}

void UserContext::SetAuthFlow(AuthFlow auth_flow) {
  auth_flow_ = auth_flow;
}

void UserContext::SetIsUsingSamlPrincipalsApi(
    bool is_using_saml_principals_api) {
  is_using_saml_principals_api_ = is_using_saml_principals_api;
}

void UserContext::SetPublicSessionLocale(const std::string& locale) {
  public_session_locale_ = locale;
}

void UserContext::SetPublicSessionInputMethod(const std::string& input_method) {
  public_session_input_method_ = input_method;
}

void UserContext::SetDeviceId(const std::string& device_id) {
  device_id_ = device_id;
}

void UserContext::SetGAPSCookie(const std::string& gaps_cookie) {
  gaps_cookie_ = gaps_cookie;
}

void UserContext::SetSyncPasswordData(
    const password_manager::PasswordHashData& sync_password_data) {
  sync_password_data_ = {sync_password_data};
}

void UserContext::SetSamlPasswordAttributes(
    const SamlPasswordAttributes& saml_password_attributes) {
  saml_password_attributes_ = saml_password_attributes;
}

void UserContext::SetIsUnderAdvancedProtection(
    bool is_under_advanced_protection) {
  is_under_advanced_protection_ = is_under_advanced_protection;
}

void UserContext::ClearSecrets() {
  key_.ClearSecret();
  password_key_.ClearSecret();
  auth_code_.clear();
  refresh_token_.clear();
}

}  // namespace chromeos
