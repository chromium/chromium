// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_USER_CONTEXT_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_USER_CONTEXT_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/enum_set.h"

#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "chromeos/ash/components/login/auth/public/sync_trusted_vault_keys.h"
#include "components/account_id/account_id.h"
#include "components/password_manager/core/browser/password_hash_data.h"
#include "components/user_manager/user_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {

// Information that is passed around while authentication is in progress. The
// credentials may consist of a |account_id_|, |key_| pair or a GAIA
// |auth_code_|.
// The |user_id_hash_| is used to locate the user's home directory
// mount point for the user. It is set when the mount has been completed.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC) UserContext {
 public:
  // The authentication flow used during sign-in.
  enum AuthFlow {
    // Online authentication against GAIA. GAIA did not redirect to a SAML IdP.
    AUTH_FLOW_GAIA_WITHOUT_SAML,
    // Online authentication against GAIA. GAIA redirected to a SAML IdP.
    AUTH_FLOW_GAIA_WITH_SAML,
    // Offline authentication against a cached key.
    AUTH_FLOW_OFFLINE,
    // Authentication against Active Directory server.
    AUTH_FLOW_ACTIVE_DIRECTORY,
  };

  // Data that is relevant only for interaction with cryptohomed.
  class CryptohomeContext {
   public:
    CryptohomeContext();
    CryptohomeContext(const CryptohomeContext& other);
    ~CryptohomeContext();

    bool operator==(const CryptohomeContext& context) const;
    bool operator!=(const CryptohomeContext& context) const;

    bool IsForcingDircrypto() const;
    void SetIsForcingDircrypto(bool is_forcing_dircrypto);

    // TODO(b/241259026): rename this method.
    const SessionAuthFactors& GetAuthFactorsData() const;
    void SetSessionAuthFactors(SessionAuthFactors keys);

    // May only be called if AuthFactorsConfiguration has been set.
    const AuthFactorsConfiguration& GetAuthFactorsConfiguration();
    bool HasAuthFactorsConfiguration() const;
    void SetAuthFactorsConfiguration(AuthFactorsConfiguration auth_factors);
    void ClearAuthFactorsConfiguration();

    const std::string& GetUserIDHash() const;
    void SetUserIDHash(const std::string& user_id_hash);

    void SetAuthSessionId(const std::string& authsession_id);
    void ResetAuthSessionId();
    const std::string& GetAuthSessionId() const;

    void AddAuthorizedIntent(AuthSessionIntent auth_intent);
    AuthSessionIntents GetAuthorizedIntents() const;

    void ClearSecrets();

   private:
    bool is_forcing_dircrypto_ = false;
    SessionAuthFactors session_auth_factors_;
    absl::optional<AuthFactorsConfiguration> auth_factors_configuration_;
    std::string authsession_id_;
    AuthSessionIntents authorized_for_;
    std::string user_id_hash_;
  };

  UserContext();
  UserContext(const UserContext& other);
  explicit UserContext(const user_manager::User& user);
  UserContext(user_manager::UserType user_type, const AccountId& account_id);
  ~UserContext();

  bool operator==(const UserContext& context) const;
  bool operator!=(const UserContext& context) const;

  const AccountId& GetAccountId() const;
  const std::string& GetGaiaID() const;
  // Information about the user password - either a plain-text password or a
  // its hashed/transformed representation.
  const Key* GetKey() const;
  Key* GetKey();
  // In password change scenario this is the key that would replace old Key
  // used for authentication.
  const Key* GetReplacementKey() const;
  Key* GetReplacementKey();

  // The plain-text user password. See https://crbug.com/386606.
  const Key* GetPasswordKey() const;
  Key* GetMutablePasswordKey();
  // The challenge-response keys for user authentication. Currently, such keys
  // can't be used simultaneously with the plain-text password keys, so when the
  // list stored here is non-empty, both GetKey() and GetPasswordKey() should
  // contain empty keys.
  const std::vector<ChallengeResponseKey>& GetChallengeResponseKeys() const;
  std::vector<ChallengeResponseKey>* GetMutableChallengeResponseKeys();

  // TODO(b/241259026): rename this method.
  const SessionAuthFactors& GetAuthFactorsData() const;
  // May only be called if AuthFactorsConfiguration has been set.
  const AuthFactorsConfiguration& GetAuthFactorsConfiguration();
  bool HasAuthFactorsConfiguration() const;

  const std::string& GetAuthCode() const;
  const std::string& GetRefreshToken() const;
  const std::string& GetAccessToken() const;
  const std::string& GetUserIDHash() const;
  bool IsUsingOAuth() const;
  bool IsUsingPin() const;
  bool IsForcingDircrypto() const;
  AuthFlow GetAuthFlow() const;
  bool IsUsingSamlPrincipalsApi() const;
  user_manager::UserType GetUserType() const;
  const std::string& GetPublicSessionLocale() const;
  const std::string& GetPublicSessionInputMethod() const;
  const std::string& GetDeviceId() const;
  const std::string& GetGAPSCookie() const;
  const std::string& GetReauthProofToken() const;
  const absl::optional<password_manager::PasswordHashData>&
  GetSyncPasswordData() const;
  const absl::optional<SamlPasswordAttributes>& GetSamlPasswordAttributes()
      const;
  const absl::optional<SyncTrustedVaultKeys>& GetSyncTrustedVaultKeys() const;
  bool CanLockManagedGuestSession() const;
  AuthSessionIntents GetAuthorizedIntents() const;

  bool HasCredentials() const;
  bool HasReplacementKey() const;

  // If this user is under advanced protection.
  bool IsUnderAdvancedProtection() const;

  void SetAccountId(const AccountId& account_id);
  void SetKey(const Key& key);
  void SetReplacementKey(const Key& replacement_key);

  // This method is used in key replacement scenario, when user's online
  // password was changed externally. Upon next online sign-in the new verified
  // password is collected as Key/PasswordKey. But as cryptohome still has old
  // key, old password is collected to access it and replace key.
  //
  // This method saves existing Key as ReplacementKey. PasswordKey is not
  // affected, as it contains up-to-date password. As user can attempt to enter
  // old password several times, this method would not overwrite ReplacementKey
  // if it exists after previous attempts.
  void SaveKeyForReplacement();

  // This method is used in password changed scenario when user can not remember
  // their old password and decide to re-create home directory.
  //
  // This method would replace existing Key with saved ReplacementKey.
  void ReuseReplacementKey();

  // Saves the user's plaintext password for possible authentication by system
  // services:
  // - To networks. If the user's OpenNetworkConfiguration policy contains a
  //   ${PASSWORD} variable, then the user's password will be used to
  //   authenticate to the specified network.
  // - To Kerberos. If the user's KerberosAccounts policy contains a ${PASSWORD}
  //   variable, then the user's password will be used to authenticate to the
  //   specified Kerberos account.
  // The user's password needs to be saved in memory until the policies can be
  // examined. When policies come in and none of them contain the ${PASSWORD}
  // variable, the user's password will be discarded. If at least one contains
  // the password, it will be sent to the session manager, which will then save
  // it in a keyring so it can be retrieved by the corresponding services.
  // More details can be found in https://crbug.com/386606.
  void SetPasswordKey(const Key& key);
  void SetAuthCode(const std::string& auth_code);
  void SetRefreshToken(const std::string& refresh_token);
  void SetAccessToken(const std::string& access_token);
  void SetUserIDHash(const std::string& user_id_hash);
  void SetIsUsingOAuth(bool is_using_oauth);
  void SetIsUsingPin(bool is_using_pin);
  void SetIsForcingDircrypto(bool is_forcing_dircrypto);
  void SetAuthFlow(AuthFlow auth_flow);
  void SetIsUsingSamlPrincipalsApi(bool is_using_saml_principals_api);
  void SetPublicSessionLocale(const std::string& locale);
  void SetPublicSessionInputMethod(const std::string& input_method);
  void SetDeviceId(const std::string& device_id);
  void SetGAPSCookie(const std::string& gaps_cookie);
  void SetReauthProofToken(const std::string& reauth_proof_token);
  void SetSyncPasswordData(
      const password_manager::PasswordHashData& sync_password_data);
  void SetSamlPasswordAttributes(
      const SamlPasswordAttributes& saml_password_attributes);
  void SetSyncTrustedVaultKeys(
      const SyncTrustedVaultKeys& sync_trusted_vault_keys);
  void SetIsUnderAdvancedProtection(bool is_under_advanced_protection);
  void SetCanLockManagedGuestSession(bool can_lock_managed_guest_session);
  void SetSessionAuthFactors(SessionAuthFactors keys);
  void SetAuthFactorsConfiguration(AuthFactorsConfiguration auth_factors);
  void ClearAuthFactorsConfiguration();
  // We need to pull input method used to log in into the user session to make
  // it consistent. This method will remember given input method to be used
  // when session starts.
  void SetLoginInputMethodIdUsed(const std::string& input_method_id);
  const std::string& GetLoginInputMethodIdUsed() const;
  void SetAuthSessionId(const std::string& authsession_id);
  void ResetAuthSessionId();
  const std::string& GetAuthSessionId() const;

  void AddAuthorizedIntent(AuthSessionIntent auth_intent);

  void ClearSecrets();

 private:
  AccountId account_id_;
  Key key_;
  Key password_key_;
  absl::optional<Key> replacement_key_ = absl::nullopt;
  std::vector<ChallengeResponseKey> challenge_response_keys_;
  std::string auth_code_;
  std::string refresh_token_;
  std::string access_token_;  // OAuthLogin scoped access token.
  bool is_using_oauth_ = true;
  bool is_using_pin_ = false;
  AuthFlow auth_flow_ = AUTH_FLOW_OFFLINE;
  bool is_using_saml_principals_api_ = false;
  user_manager::UserType user_type_ = user_manager::USER_TYPE_REGULAR;
  std::string public_session_locale_;
  std::string public_session_input_method_;
  std::string device_id_;
  std::string gaps_cookie_;
  std::string reauth_proof_token_;
  bool is_under_advanced_protection_ = false;
  bool can_lock_managed_guest_session_ = false;
  // |login_input_method_id_used_| is non-empty if login password/code was used,
  // i.e. user used some input method to log in.
  std::string login_input_method_id_used_;

  CryptohomeContext cryptohome_;

  // For password reuse detection use.
  absl::optional<password_manager::PasswordHashData> sync_password_data_;

  // Info about the user's SAML password, such as when it will expire.
  absl::optional<SamlPasswordAttributes> saml_password_attributes_;

  // Info about the user's sync encryption keys.
  absl::optional<SyncTrustedVaultKeys> sync_trusted_vault_keys_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_USER_CONTEXT_H_
