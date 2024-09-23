// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_ID_MOJOM_ACCOUNT_ID_TRAITS_H_
#define COMPONENTS_ACCOUNT_ID_MOJOM_ACCOUNT_ID_TRAITS_H_

#include <string>

#include "components/account_id/account_id.h"
#include "components/account_id/mojom/account_id.mojom.h"

namespace mojo {

template <>
struct EnumTraits<signin::mojom::AccountType, AccountType> {
  static signin::mojom::AccountType ToMojom(AccountType input) {
    switch (input) {
      case AccountType::UNKNOWN:
        return signin::mojom::AccountType::UNKNOWN;
      case AccountType::GOOGLE:
        return signin::mojom::AccountType::GOOGLE;
      case AccountType::ACTIVE_DIRECTORY:
        return signin::mojom::AccountType::ACTIVE_DIRECTORY;
    }
    NOTREACHED_IN_MIGRATION();
    return signin::mojom::AccountType::UNKNOWN;
  }

  static bool FromMojom(signin::mojom::AccountType input, AccountType* out) {
    switch (input) {
      case signin::mojom::AccountType::UNKNOWN:
        *out = AccountType::UNKNOWN;
        return true;
      case signin::mojom::AccountType::GOOGLE:
        *out = AccountType::GOOGLE;
        return true;
      case signin::mojom::AccountType::ACTIVE_DIRECTORY:
        *out = AccountType::ACTIVE_DIRECTORY;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct StructTraits<signin::mojom::AccountIdDataView, AccountId> {
  static AccountType account_type(const AccountId& r) {
    return r.GetAccountType();
  }
  static std::string id(const AccountId& r) {
    switch (r.GetAccountType()) {
      case AccountType::GOOGLE:
        return r.GetGaiaId();
      case AccountType::ACTIVE_DIRECTORY:
        return r.GetObjGuid();
      case AccountType::UNKNOWN:
        // UNKNOWN type is used for users that have only email (e.g. in tests
        // or legacy users that have not run through migration code).
        // Return an empty string for such accounts.
        return std::string();
    }
    NOTREACHED_IN_MIGRATION();
    return std::string();
  }
  static std::string user_email(const AccountId& r) { return r.GetUserEmail(); }

  static bool Read(signin::mojom::AccountIdDataView data, AccountId* out) {
    AccountType account_type;
    std::string id;
    std::string user_email;
    if (!data.ReadAccountType(&account_type) || !data.ReadId(&id) ||
        !data.ReadUserEmail(&user_email)) {
      return false;
    }

    switch (account_type) {
      case AccountType::GOOGLE:
        *out = AccountId::FromUserEmailGaiaId(user_email, id);
        break;
      case AccountType::ACTIVE_DIRECTORY:
        *out = AccountId::AdFromUserEmailObjGuid(user_email, id);
        break;
      case AccountType::UNKNOWN:
        // UNKNOWN type is used for users that have only email (e.g. in tests
        // or legacy users that have not run through migration code).
        // Bail if there is no user email.
        if (user_email.empty())
          return false;

        *out = AccountId::FromUserEmail(user_email);
        break;
    }

    return out->is_valid();
  }

  static bool IsNull(const AccountId& input) { return !input.is_valid(); }

  static void SetToNull(AccountId* output) { *output = EmptyAccountId(); }
};

}  // namespace mojo

#endif  // COMPONENTS_ACCOUNT_ID_MOJOM_ACCOUNT_ID_TRAITS_H_
