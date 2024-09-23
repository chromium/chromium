// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/account_id_util.h"

#include <optional>
#include <string>

#include "base/check_is_test.h"
#include "base/json/values_util.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/account_id/account_id.h"

namespace user_manager {

const char kCanonicalEmail[] = "email";
const char kGAIAIdKey[] = "gaia_id";
const char kObjGuidKey[] = "obj_guid";
const char kAccountTypeKey[] = "account_type";

std::optional<AccountId> LoadAccountId(const base::Value::Dict& dict) {
  const std::string* email = dict.FindString(kCanonicalEmail);
  const std::string* gaia_id = dict.FindString(kGAIAIdKey);
  const std::string* obj_guid = dict.FindString(kObjGuidKey);
  AccountType account_type = AccountType::GOOGLE;
  if (const std::string* account_type_string =
          dict.FindString(kAccountTypeKey)) {
    account_type = AccountId::StringToAccountType(*account_type_string);
  }
  switch (account_type) {
    case AccountType::GOOGLE:
      if (email || gaia_id) {
        return AccountId::FromUserEmailGaiaId(
            email ? *email : std::string(), gaia_id ? *gaia_id : std::string());
      }
      break;
    case AccountType::ACTIVE_DIRECTORY:
      if (email && obj_guid) {
        return AccountId::AdFromUserEmailObjGuid(*email, *obj_guid);
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown account type";
  }
  return std::nullopt;
}

bool AccountIdMatches(const AccountId& account_id,
                      const base::Value::Dict& dict) {
  const std::string* account_type = dict.FindString(kAccountTypeKey);
  if (account_id.GetAccountType() != AccountType::UNKNOWN && account_type &&
      account_id.GetAccountType() !=
          AccountId::StringToAccountType(*account_type)) {
    return false;
  }

  // TODO(b/268177869): If the gaia id or GUID are present, but doesn't match,
  // this function should likely be returning false even if the e-mail matches.
  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE: {
      const std::string* gaia_id = dict.FindString(kGAIAIdKey);
      if (gaia_id && account_id.GetGaiaId() == *gaia_id) {
        return true;
      }
      break;
    }
    case AccountType::ACTIVE_DIRECTORY: {
      const std::string* obj_guid = dict.FindString(kObjGuidKey);
      if (obj_guid && account_id.GetObjGuid() == *obj_guid) {
        return true;
      }
      break;
    }
    case AccountType::UNKNOWN: {
    }
  }

  const std::string* email = dict.FindString(kCanonicalEmail);
  if (email && account_id.GetUserEmail() == *email) {
    return true;
  }

  return false;
}

void StoreAccountId(const AccountId& account_id, base::Value::Dict& dict) {
  if (!account_id.GetUserEmail().empty()) {
    dict.Set(kCanonicalEmail, account_id.GetUserEmail());
  }

  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE:
      if (!account_id.GetGaiaId().empty()) {
        dict.Set(kGAIAIdKey, account_id.GetGaiaId());
      }
      break;
    case AccountType::ACTIVE_DIRECTORY:
      if (!account_id.GetObjGuid().empty()) {
        dict.Set(kObjGuidKey, account_id.GetObjGuid());
      }
      break;
    case AccountType::UNKNOWN:
      return;
  }
  dict.Set(kAccountTypeKey,
           AccountId::AccountTypeToString(account_id.GetAccountType()));
}

}  // namespace user_manager
