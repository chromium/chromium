// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_ID_ACCOUNT_ID_LITERAL_H_
#define COMPONENTS_ACCOUNT_ID_ACCOUNT_ID_LITERAL_H_

#include <string_view>

#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_id.h"

// Literal for the AccountId. This is testing utility to allow using constexpr
// for AccountId in tests.
class AccountIdLiteral {
 public:
  ~AccountIdLiteral() = default;

  static constexpr AccountIdLiteral FromUserEmail(std::string_view user_email) {
    return AccountIdLiteral(AccountType::UNKNOWN, user_email,
                            GaiaId::Literal(""));
  }
  static constexpr AccountIdLiteral FromUserEmailGaiaId(
      std::string_view user_email,
      const GaiaId::Literal gaia_id) {
    return AccountIdLiteral(AccountType::GOOGLE, user_email, gaia_id);
  }

  const std::string_view GetUserEmail() const { return user_email_; }
  const GaiaId::Literal GetGaiaId() const { return gaia_id_; }

  // Allows implicit conversion so functions taking AccountId can use the
  // constexpr literal.
  inline operator AccountId() const {
    switch (account_type_) {
      case AccountType::UNKNOWN:
        return AccountId::FromUserEmail(user_email_);
      case AccountType::GOOGLE:
        return AccountId::FromUserEmailGaiaId(user_email_, gaia_id_);
    }
  }

 private:
  constexpr AccountIdLiteral(AccountType account_type,
                             std::string_view user_email,
                             const GaiaId::Literal gaia_id)
      : account_type_(account_type),
        user_email_(user_email),
        gaia_id_(gaia_id) {}

  AccountType account_type_;
  std::string_view user_email_;
  GaiaId::Literal gaia_id_;
};

#endif  // COMPONENTS_ACCOUNT_ID_ACCOUNT_ID_LITERAL_H_
