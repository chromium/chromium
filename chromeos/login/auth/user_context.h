// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_USER_CONTEXT_H_
#define CHROMEOS_LOGIN_AUTH_USER_CONTEXT_H_

#include <string>

#include "base/component_export.h"
#include "base/optional.h"
#include "chromeos/login/auth/challenge_response_key.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/account_id/account_id.h"
#include "components/password_manager/core/browser/password_hash_data.h"
#include "components/user_manager/user_type.h"

class AccountId;

namespace user_manager {
class User;
}

namespace chromeos {

// Information that is passed around while authentication is in progress. The
// credentials may consist of a |account_id_|, |key_| pair or a GAIA
// |auth_code_|.
// The |user_id_hash_| is used to locate the user's home directory
// mount point for the user. It is set when the mount has been completed.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) UserContext {
 public:
  // The authentication flow used during sign-in.
  enum AuthFlow {
    // Online authentication against GAIA. GAIA did not redirect to a SAML IdP.
    AUTH_FLOW_GAIA_WITHOUT_SAML,
    // Online authentication against GAIA. GAIA redirected to a SAML IdP.
    AUTH_FLOW_GAIA_WITH_SAML,
    // Offline authentication against a cached key.
    AUTH_FLOW_OFFLINE,
    // Offline authentication using and Easy unlock device (e.g. a phone).
    AUTH_FLOW_EASY_UNLOCK,
    // Authentication against Active Directory server.
    AUTH_FLOW_ACTIVE_DIRECTORY,
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
  // The plain-text user password. See https://crbug.com/386606.
  const Key* GetPasswordKey() const;
  Key* GetMutablePasswordKey();
  // The challenge-response keys for user authentication. Currently, such keys
  // can't be used simultaneously with the plain-text password keys, so when the
  // list stored here is non-empty, both GetKey() and GetPasswordKey() should
  // contain empty keys.
  const std::vector<ChallengeResponseKey>& GetChallengeResponseKeys() const;
  std::vector<ChallengeResponseKey>* GetMutableChallengeResponseKeys();

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
  const base::Optional<password_manager::PasswordHashData>&
  GetSyncPasswordData() const;
  const base::Optional<SamlPasswordAttributes>& GetSamlPasswordAttributes()
      const;

  bool HasCredentials() const;

  // If this user is under advanced protection.
  bool IsUnderAdvancedProtection() const;

  void SetAccountId(const AccountId& account_id);
  void SetKey(const Key& key);
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
  void SetSyncPasswordData(
      const password_manager::PasswordHashData& sync_password_data);
  void SetSamlPasswordAttributes(
      const SamlPasswordAttributes& saml_password_attributes);
  void SetIsUnderAdvancedProtection(bool is_under_advanced_protection);

  void ClearSecrets();

 private:
  AccountId account_id_;
  Key key_;
  Key password_key_;
  std::vector<ChallengeResponseKey> challenge_response_keys_;
  std::string auth_code_;
  std::string refresh_token_;
  std::string access_token_;  // OAuthLogin scoped access token.
  std::string user_id_hash_;
  bool is_using_oauth_ = true;
  bool is_using_pin_ = false;
  bool is_forcing_dircrypto_ = false;
  AuthFlow auth_flow_ = AUTH_FLOW_OFFLINE;
  bool is_using_saml_principals_api_ = false;
  user_manager::UserType user_type_ = user_manager::USER_TYPE_REGULAR;
  std::string public_session_locale_;
  std::string public_session_input_method_;
  std::string device_id_;
  std::string gaps_cookie_;
  bool is_under_advanced_protection_ = false;

  // For password reuse detection use.
  base::Optional<password_manager::PasswordHashData> sync_password_data_;

  // Info about the user's SAML password, such as when it will expire.
  base::Optional<SamlPasswordAttributes> saml_password_attributes_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_USER_CONTEXT_H_
