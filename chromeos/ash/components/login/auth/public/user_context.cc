// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/user_context.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"

namespace ash {

UserContext::UserContext() : account_id_(EmptyAccountId()) {}

UserContext::UserContext(const UserContext& other) = default;

UserContext::UserContext(const user_manager::User& user)
    : account_id_(user.GetAccountId()), user_type_(user.GetType()) {
  if (user_type_ == user_manager::UserType::kRegular) {
    account_id_.SetUserEmail(
        user_manager::CanonicalizeUserID(account_id_.GetUserEmail()));
  }
}

UserContext::UserContext(user_manager::UserType user_type,
                         const AccountId& account_id)
    : account_id_(account_id), user_type_(user_type) {
  if (user_type_ == user_manager::UserType::kRegular) {
    account_id_.SetUserEmail(
        user_manager::CanonicalizeUserID(account_id_.GetUserEmail()));
  }
}

UserContext::~UserContext() = default;

bool UserContext::operator==(const UserContext& context) const {
  return context.account_id_ == account_id_ && context.key_ == key_ &&
         context.auth_code_ == auth_code_ &&
         context.refresh_token_ == refresh_token_ &&
         context.access_token_ == access_token_ &&
         context.is_using_oauth_ == is_using_oauth_ &&
         context.auth_flow_ == auth_flow_ && context.user_type_ == user_type_ &&
         context.public_session_locale_ == public_session_locale_ &&
         context.public_session_input_method_ == public_session_input_method_ &&
         context.login_input_method_id_used_ == login_input_method_id_used_ &&
         context.cryptohome_ == cryptohome_;
}

bool UserContext::operator!=(const UserContext& context) const {
  return !(*this == context);
}

UserContext::CryptohomeContext::CryptohomeContext() = default;
UserContext::CryptohomeContext::CryptohomeContext(
    const UserContext::CryptohomeContext& other) = default;
UserContext::CryptohomeContext::~CryptohomeContext() = default;

bool UserContext::CryptohomeContext::operator==(
    const CryptohomeContext& context) const {
  return context.user_id_hash_ == user_id_hash_ &&
         context.authsession_id_ == authsession_id_;
}
bool UserContext::CryptohomeContext::operator!=(
    const CryptohomeContext& context) const {
  return !(*this == context);
}

const std::string& UserContext::CryptohomeContext::GetUserIDHash() const {
  return user_id_hash_;
}

void UserContext::CryptohomeContext::SetUserIDHash(
    const std::string& user_id_hash) {
  user_id_hash_ = user_id_hash;
}

bool UserContext::CryptohomeContext::IsForcingDircrypto() const {
  return is_forcing_dircrypto_;
}

void UserContext::CryptohomeContext::SetIsForcingDircrypto(
    bool is_forcing_dircrypto) {
  is_forcing_dircrypto_ = is_forcing_dircrypto;
}

void UserContext::CryptohomeContext::SetAuthSessionIds(
    const std::string& authsession_id,
    const std::string& broadcast_id) {
  LOG_IF(WARNING, !authsession_id_.empty())
      << "Overwriting existing auth session ID";
  DCHECK(authsession_id_.empty());
  authsession_id_ = authsession_id;
  broadcast_id_ = broadcast_id;
}

void UserContext::CryptohomeContext::ResetAuthSessionIds() {
  authsession_id_.clear();
  broadcast_id_.clear();
  authorized_for_.Clear();
  valid_until_ = base::Time();
}

base::Time UserContext::CryptohomeContext::GetSessionLifetime() const {
  return valid_until_;
}

void UserContext::CryptohomeContext::SetSessionLifetime(
    const base::Time& valid_until) {
  valid_until_ = valid_until;
}

void UserContext::CryptohomeContext::SetSessionAuthFactors(
    SessionAuthFactors data) {
  session_auth_factors_ = std::move(data);
}

const SessionAuthFactors& UserContext::CryptohomeContext::GetAuthFactorsData()
    const {
  return session_auth_factors_;
}

void UserContext::CryptohomeContext::SetAuthFactorsConfiguration(
    AuthFactorsConfiguration auth_factors) {
  auth_factors_configuration_ = std::move(auth_factors);
}

void UserContext::CryptohomeContext::ClearAuthFactorsConfiguration() {
  auth_factors_configuration_ = std::nullopt;
}

const AuthFactorsConfiguration&
UserContext::CryptohomeContext::GetAuthFactorsConfiguration() const {
  if (!auth_factors_configuration_.has_value()) {
    // Crash with debug assertions, try to stay alive otherwise. This method
    // could be const if we didn't set auth_factors_configuration_ if
    // necessary.
    DCHECK(false) << "AuthFactorsConfiguration has not been set";
    auth_factors_configuration_ = AuthFactorsConfiguration();
  }

  return *auth_factors_configuration_;
}

bool UserContext::CryptohomeContext::HasAuthFactorsConfiguration() const {
  return auth_factors_configuration_.has_value();
}

const std::string& UserContext::CryptohomeContext::GetAuthSessionId() const {
  return authsession_id_;
}

const std::string& UserContext::CryptohomeContext::GetBroadcastId() const {
  return broadcast_id_;
}

AuthSessionIntents UserContext::CryptohomeContext::GetAuthorizedIntents()
    const {
  return authorized_for_;
}

void UserContext::CryptohomeContext::ClearAuthorizedIntents() {
  authorized_for_.Clear();
}

void UserContext::CryptohomeContext::AddAuthorizedIntent(
    const AuthSessionIntent auth_intent) {
  authorized_for_.Put(auth_intent);
}

std::optional<UserContext::MountState>
UserContext::CryptohomeContext::GetMountState() const {
  return mount_state_;
}

void UserContext::CryptohomeContext::SetMountState(
    UserContext::MountState mount_state) {
  mount_state_ = mount_state;
}

void UserContext::CryptohomeContext::ClearSecrets() {
  authsession_id_.clear();
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

const Key* UserContext::GetReplacementKey() const {
  return &replacement_key_.value();
}

Key* UserContext::GetReplacementKey() {
  return &replacement_key_.value();
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
  return cryptohome_.GetUserIDHash();
}

bool UserContext::IsUsingOAuth() const {
  return is_using_oauth_;
}

bool UserContext::IsUsingPin() const {
  return is_using_pin_;
}

bool UserContext::IsForcingDircrypto() const {
  return cryptohome_.IsForcingDircrypto();
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

const std::string& UserContext::GetReauthProofToken() const {
  return reauth_proof_token_;
}

const std::optional<password_manager::PasswordHashData>&
UserContext::GetSyncPasswordData() const {
  return sync_password_data_;
}

const std::optional<SamlPasswordAttributes>&
UserContext::GetSamlPasswordAttributes() const {
  return saml_password_attributes_;
}

const std::optional<SyncTrustedVaultKeys>&
UserContext::GetSyncTrustedVaultKeys() const {
  return sync_trusted_vault_keys_;
}

bool UserContext::CanLockManagedGuestSession() const {
  return can_lock_managed_guest_session_;
}

bool UserContext::HasCredentials() const {
  return (account_id_.is_valid() && !key_.GetSecret().empty()) ||
         !auth_code_.empty();
}

bool UserContext::HasReplacementKey() const {
  return replacement_key_.has_value();
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

void UserContext::SetReplacementKey(const Key& replacement_key) {
  replacement_key_ = replacement_key;
}

void UserContext::SaveKeyForReplacement() {
  if (replacement_key_.has_value())
    return;
  replacement_key_ = key_;
}

void UserContext::ReuseReplacementKey() {
  DCHECK(replacement_key_.has_value());
  key_ = *replacement_key_;
  replacement_key_ = std::nullopt;
}

void UserContext::SetPasswordKey(const Key& key) {
  password_key_ = key;
}

void UserContext::SetGaiaPassword(const GaiaPassword& password) {
  gaia_password_.emplace(password);
}

void UserContext::SetSamlPassword(const SamlPassword& password) {
  saml_password_.emplace(password);
}

void UserContext::SetLocalPasswordInput(const LocalPasswordInput& password) {
  local_input_.emplace(password);
}

std::optional<OnlinePassword> UserContext::GetOnlinePassword() const {
  if (gaia_password_.has_value()) {
    return OnlinePassword{gaia_password_->value()};
  } else if (saml_password_.has_value()) {
    return OnlinePassword{saml_password_->value()};
  } else {
    return std::nullopt;
  }
}

std::optional<PasswordInput> UserContext::GetPassword() const {
  if (local_input_.has_value()) {
    return PasswordInput{local_input_->value()};
  } else if (gaia_password_.has_value()) {
    return PasswordInput{gaia_password_->value()};
  } else if (saml_password_.has_value()) {
    return PasswordInput{saml_password_->value()};
  } else {
    return std::nullopt;
  }
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
  cryptohome_.SetUserIDHash(user_id_hash);
}

void UserContext::SetIsUsingOAuth(bool is_using_oauth) {
  is_using_oauth_ = is_using_oauth;
}

void UserContext::SetIsUsingPin(bool is_using_pin) {
  is_using_pin_ = is_using_pin;
}

void UserContext::SetIsForcingDircrypto(bool is_forcing_dircrypto) {
  cryptohome_.SetIsForcingDircrypto(is_forcing_dircrypto);
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

void UserContext::SetReauthProofToken(const std::string& reauth_proof_token) {
  reauth_proof_token_ = reauth_proof_token;
}

void UserContext::SetSyncPasswordData(
    const password_manager::PasswordHashData& sync_password_data) {
  sync_password_data_ = {sync_password_data};
}

void UserContext::SetSamlPasswordAttributes(
    const SamlPasswordAttributes& saml_password_attributes) {
  saml_password_attributes_ = saml_password_attributes;
}

void UserContext::SetSyncTrustedVaultKeys(
    const SyncTrustedVaultKeys& sync_trusted_vault_keys) {
  sync_trusted_vault_keys_ = sync_trusted_vault_keys;
}

void UserContext::SetIsUnderAdvancedProtection(
    bool is_under_advanced_protection) {
  is_under_advanced_protection_ = is_under_advanced_protection;
}

void UserContext::SetCanLockManagedGuestSession(
    bool can_lock_managed_guest_session) {
  can_lock_managed_guest_session_ = can_lock_managed_guest_session;
}

void UserContext::SetLoginInputMethodIdUsed(
    const std::string& input_method_id) {
  DCHECK(login_input_method_id_used_.empty());
  login_input_method_id_used_ = input_method_id;
}

const std::string& UserContext::GetLoginInputMethodIdUsed() const {
  return login_input_method_id_used_;
}

void UserContext::SetAuthSessionIds(const std::string& authsession_id,
                                    const std::string& broadcast_id) {
  cryptohome_.SetAuthSessionIds(authsession_id, broadcast_id);
}

void UserContext::ResetAuthSessionIds() {
  cryptohome_.ResetAuthSessionIds();
}

base::Time UserContext::GetSessionLifetime() const {
  return cryptohome_.GetSessionLifetime();
}

void UserContext::SetSessionLifetime(const base::Time& valid_until) {
  cryptohome_.SetSessionLifetime(valid_until);
}

void UserContext::SetSessionAuthFactors(SessionAuthFactors data) {
  cryptohome_.SetSessionAuthFactors(std::move(data));
}

const SessionAuthFactors& UserContext::GetAuthFactorsData() const {
  return cryptohome_.GetAuthFactorsData();
}

void UserContext::SetAuthFactorsConfiguration(
    AuthFactorsConfiguration auth_factors) {
  cryptohome_.SetAuthFactorsConfiguration(std::move(auth_factors));
}

void UserContext::ClearAuthFactorsConfiguration() {
  cryptohome_.ClearAuthFactorsConfiguration();
}

const AuthFactorsConfiguration& UserContext::GetAuthFactorsConfiguration()
    const {
  return cryptohome_.GetAuthFactorsConfiguration();
}

bool UserContext::HasAuthFactorsConfiguration() const {
  return cryptohome_.HasAuthFactorsConfiguration();
}

const std::string& UserContext::GetAuthSessionId() const {
  return cryptohome_.GetAuthSessionId();
}

const std::string& UserContext::GetBroadcastId() const {
  return cryptohome_.GetBroadcastId();
}

AuthSessionIntents UserContext::GetAuthorizedIntents() const {
  return cryptohome_.GetAuthorizedIntents();
}

void UserContext::ClearAuthorizedIntents() {
  cryptohome_.ClearAuthorizedIntents();
}

void UserContext::AddAuthorizedIntent(const AuthSessionIntent auth_intent) {
  cryptohome_.AddAuthorizedIntent(auth_intent);
}

std::optional<UserContext::MountState> UserContext::GetMountState() const {
  return cryptohome_.GetMountState();
}

void UserContext::SetMountState(UserContext::MountState mount_state) {
  cryptohome_.SetMountState(mount_state);
}

void UserContext::ClearSecrets() {
  key_.ClearSecret();
  password_key_.ClearSecret();
  replacement_key_ = std::nullopt;
  auth_code_.clear();
  refresh_token_.clear();
  sync_trusted_vault_keys_.reset();
  cryptohome_.ClearSecrets();
  gaia_password_.reset();
  saml_password_.reset();
  local_input_.reset();
}

}  // namespace ash
