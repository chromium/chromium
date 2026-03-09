// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_response_params.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace signin {

// Trivial constructors and destructors.
DiceResponseParams::DiceResponseParams() = default;
DiceResponseParams::~DiceResponseParams() = default;
DiceResponseParams::DiceResponseParams(DiceResponseParams&&) = default;
DiceResponseParams& DiceResponseParams::operator=(DiceResponseParams&&) =
    default;

bool DiceResponseParams::IsValid() const {
  switch (user_intention) {
    case DiceAction::NONE:
      return false;
    case DiceAction::SIGNIN:
      return signin_info && signin_info->IsValid();
    case DiceAction::SIGNOUT:
      return signout_info && signout_info->IsValid();
    case DiceAction::ENABLE_SYNC:
      return enable_sync_info && enable_sync_info->IsValid();
  }
  NOTREACHED();
}

DiceResponseParams::AccountInfo::AccountInfo() = default;
DiceResponseParams::AccountInfo::AccountInfo(const GaiaId& gaia_id,
                                             const std::string& email,
                                             int session_index)
    : gaia_id(gaia_id), email(email), session_index(session_index) {}
DiceResponseParams::AccountInfo::~AccountInfo() = default;
DiceResponseParams::AccountInfo::AccountInfo(const AccountInfo&) = default;

bool DiceResponseParams::AccountInfo::IsValid() const {
  return !gaia_id.empty() && !email.empty() &&
         session_index != DiceResponseParams::AccountInfo::kInvalidSessionIndex;
}

DiceResponseParams::SigninInfo::SigninAccount::SigninAccount() = default;
DiceResponseParams::SigninInfo::SigninAccount::SigninAccount(
    AccountInfo account_info,
    std::string authorization_code,
    bool no_authorization_code,
    std::string supported_algorithms_for_token_binding)
    : account_info(std::move(account_info)),
      authorization_code(std::move(authorization_code)),
      no_authorization_code(no_authorization_code),
      supported_algorithms_for_token_binding(
          std::move(supported_algorithms_for_token_binding)) {}
DiceResponseParams::SigninInfo::SigninAccount::~SigninAccount() = default;
DiceResponseParams::SigninInfo::SigninAccount::SigninAccount(
    const SigninAccount&) = default;

bool DiceResponseParams::SigninInfo::SigninAccount::IsValid() const {
  return account_info.IsValid() &&
         (!authorization_code.empty() || no_authorization_code);
}

DiceResponseParams::SigninInfo::SigninInfo() = default;
DiceResponseParams::SigninInfo::~SigninInfo() = default;
DiceResponseParams::SigninInfo::SigninInfo(SigninInfo&&) = default;
DiceResponseParams::SigninInfo& DiceResponseParams::SigninInfo::operator=(
    SigninInfo&&) = default;

void DiceResponseParams::SigninInfo::SetInitiator(const GaiaId& gaia_id) {
  initiator_id = gaia_id;
}

const DiceResponseParams::SigninInfo::SigninAccount*
DiceResponseParams::SigninInfo::GetInitiator() const {
  if (initiator_id.empty()) {
    return accounts_.size() == 1u ? &accounts_[0] : nullptr;
  }

  auto it = std::ranges::find_if(accounts_, [&](const SigninAccount& account) {
    return account.account_info.gaia_id == initiator_id;
  });
  return it == accounts_.end() ? nullptr : &*it;
}

void DiceResponseParams::SigninInfo::AddAccount(SigninAccount account) {
  // Uma histogram that records whether the authorization code was present or
  // not.
  if (account.IsValid()) {
    base::UmaHistogramBoolean("Signin.DiceAuthorizationCode",
                              !account.authorization_code.empty());
  }
  accounts_.push_back(std::move(account));
}

bool DiceResponseParams::SigninInfo::IsValid() const {
  if (accounts_.empty()) {
    return false;
  }

  for (const auto& account : accounts_) {
    if (!account.IsValid()) {
      return false;
    }
  }

  return GetInitiator() != nullptr;
}

DiceResponseParams::SignoutInfo::SignoutInfo() = default;
DiceResponseParams::SignoutInfo::~SignoutInfo() = default;
DiceResponseParams::SignoutInfo::SignoutInfo(const SignoutInfo&) = default;

bool DiceResponseParams::SignoutInfo::IsValid() const {
  return !account_infos.empty();
}

DiceResponseParams::EnableSyncInfo::EnableSyncInfo() = default;
DiceResponseParams::EnableSyncInfo::~EnableSyncInfo() = default;
DiceResponseParams::EnableSyncInfo::EnableSyncInfo(const EnableSyncInfo&) =
    default;

bool DiceResponseParams::EnableSyncInfo::IsValid() const {
  return account_info.IsValid();
}

}  // namespace signin
