// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTHENTICATOR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTHENTICATOR_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class AccountId;

namespace ash {

class AuthFailure;
class UserContext;

// An interface for objects that will authenticate a Chromium OS user.
// Callbacks will be called on the UI thread:
// 1. On successful authentication, will call consumer_->OnAuthSuccess().
// 2. On failure, will call consumer_->OnAuthFailure().
// 3. On password change, will call consumer_->OnOnlinePasswordUnusable().
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) Authenticator
    : public base::RefCountedThreadSafe<Authenticator> {
 public:
  explicit Authenticator(AuthStatusConsumer* consumer);

  Authenticator(const Authenticator&) = delete;
  Authenticator& operator=(const Authenticator&) = delete;

  // Given externally authenticated username and password (part of
  // |user_context|), this method attempts to complete authentication process.
  virtual void CompleteLogin(bool ephemeral,
                             std::unique_ptr<UserContext> user_context) = 0;

  // Given a user credentials in |user_context|,
  // this method attempts to authenticate to login.
  // Must be called on the UI thread.
  virtual void AuthenticateToLogin(
      bool ephemeral,
      std::unique_ptr<UserContext> user_context) = 0;

  // Given a user credentials in |user_context|,
  // this method attempts to authenticate to unlock.
  // Must be called on the UI thread.
  virtual void AuthenticateToUnlock(
      bool ephemeral,
      std::unique_ptr<UserContext> user_context) = 0;

  // Initiates incognito ("browse without signing in") login.
  virtual void LoginOffTheRecord() = 0;

  // Initiates login into the public account identified by |user_context|.
  virtual void LoginAsPublicSession(const UserContext& user_context) = 0;

  // Initiates login into kiosk mode account identified by |app_account_id|.
  // The |app_account_id| is a generated account id for the account.
  // So called Public mount is used to mount cryptohome.
  // |ephemeral| controls whether cryptohome is ephemeral or persistent.
  virtual void LoginAsKioskAccount(const AccountId& app_account_id,
                                   bool ephemeral) = 0;

  // Initiates login into web kiosk mode account identified by |app_account_id|.
  // The |app_account_id| is a generated account id for the account.
  // Web kiosk mode mounts a public cryptohome.
  // |ephemeral| controls whether cryptohome is ephemeral or persistent.
  virtual void LoginAsWebKioskAccount(const AccountId& app_account_id,
                                      bool ephemeral) = 0;

  // Initiates login into IWA kiosk mode account identified by |app_account_id|.
  // The |app_account_id| is a generated account id for the account.
  // IWA kiosk mode mounts a public cryptohome.
  // |ephemeral| controls whether cryptohome is ephemeral or persistent.
  virtual void LoginAsIwaKioskAccount(const AccountId& app_account_id,
                                      bool ephemeral) = 0;

  // Continues the login of persistent user that is already authenticated via
  // |auth_session|. This method can be used as a part of the recovery flow, or
  // to continue login stopped to perform encryption migration.
  virtual void LoginAuthenticated(
      std::unique_ptr<UserContext> user_context) = 0;

  // Notifies caller that login was successful. Must be called on the UI thread.
  virtual void OnAuthSuccess() = 0;

  // Must be called on the UI thread.
  virtual void OnAuthFailure(const AuthFailure& error) = 0;

  // Sets consumer explicitly.
  void SetConsumer(AuthStatusConsumer* consumer);

 protected:
  virtual ~Authenticator();

  raw_ptr<AuthStatusConsumer, DanglingUntriaged> consumer_;

 private:
  friend class base::RefCountedThreadSafe<Authenticator>;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTHENTICATOR_H_
