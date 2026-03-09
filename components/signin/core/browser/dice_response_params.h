// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_DICE_RESPONSE_PARAMS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_DICE_RESPONSE_PARAMS_H_

#include <memory>
#include <string>
#include <vector>

#include "google_apis/gaia/gaia_id.h"

namespace signin {

enum class DiceAction {
  NONE,
  SIGNIN,      // Sign in an account.
  SIGNOUT,     // Sign out of all sessions.
  ENABLE_SYNC  // Enable Sync on a signed-in account.
};

// Struct describing the parameters received in the Dice response header.
struct DiceResponseParams {
  struct AccountInfo {
    static constexpr int kInvalidSessionIndex = -1;

    AccountInfo();
    AccountInfo(const GaiaId& gaia_id,
                const std::string& email,
                int session_index);
    ~AccountInfo();
    AccountInfo(const AccountInfo&);

    bool IsValid() const;

    // Gaia ID of the account.
    GaiaId gaia_id;
    // Email of the account.
    std::string email;
    // Session index for the account.
    int session_index = kInvalidSessionIndex;
  };

  // Parameters for the SIGNIN action.
  struct SigninInfo {
    // Information for a single account being signed in.
    struct SigninAccount {
      SigninAccount();
      SigninAccount(AccountInfo account_info,
                    std::string authorization_code,
                    bool no_authorization_code,
                    std::string supported_algorithms_for_token_binding);
      SigninAccount(const SigninAccount&);
      ~SigninAccount();

      bool IsValid() const;

      // AccountInfo of the account signed in.
      AccountInfo account_info;
      // Authorization code to fetch a refresh token.
      std::string authorization_code;
      // Whether Dice response contains the 'no_authorization_code' header
      // value. If true then LSO was unavailable for provision of auth code.
      bool no_authorization_code = false;
      // If the account is eligible for token binding, this string is non-empty
      // and contains a list of supported binding algorithms separated by space.
      std::string supported_algorithms_for_token_binding;
    };

    SigninInfo();
    SigninInfo(const SigninInfo&) = delete;
    SigninInfo& operator=(const SigninInfo&) = delete;
    SigninInfo(SigninInfo&&);
    SigninInfo& operator=(SigninInfo&&);
    ~SigninInfo();

    bool IsValid() const;

    // Returns the initiator account, or the first account if there is only one
    // and no initiator is specified. Returns nullptr if no match is found.
    const SigninAccount* GetInitiator() const;

    void SetInitiator(const GaiaId& gaia_id);

    void AddAccount(SigninAccount account);

    const std::vector<SigninAccount>& accounts() const { return accounts_; }

   private:
    GaiaId initiator_id;
    std::vector<SigninAccount> accounts_;
  };

  // Parameters for the SIGNOUT action.
  struct SignoutInfo {
    SignoutInfo();
    SignoutInfo(const SignoutInfo&);
    ~SignoutInfo();

    bool IsValid() const;

    // Account infos for the accounts signed out.
    std::vector<AccountInfo> account_infos;
  };

  // Parameters for the ENABLE_SYNC action.
  struct EnableSyncInfo {
    EnableSyncInfo();
    EnableSyncInfo(const EnableSyncInfo&);
    ~EnableSyncInfo();

    bool IsValid() const;

    // AccountInfo of the account enabling Sync.
    AccountInfo account_info;
  };

  DiceResponseParams();

  DiceResponseParams(const DiceResponseParams&) = delete;
  DiceResponseParams& operator=(const DiceResponseParams&) = delete;

  DiceResponseParams(DiceResponseParams&&);
  DiceResponseParams& operator=(DiceResponseParams&&);

  ~DiceResponseParams();

  bool IsValid() const;

  DiceAction user_intention = DiceAction::NONE;

  // Populated when |user_intention| is SIGNIN.
  std::unique_ptr<SigninInfo> signin_info;

  // Populated when |user_intention| is SIGNOUT.
  std::unique_ptr<SignoutInfo> signout_info;

  // Populated when |user_intention| is ENABLE_SYNC.
  std::unique_ptr<EnableSyncInfo> enable_sync_info;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_DICE_RESPONSE_PARAMS_H_
