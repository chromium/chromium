// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_id/account_id.h"

#include <functional>
#include <memory>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

// Serialization keys
constexpr char kGaiaIdKey[] = "gaia_id";
constexpr char kEmailKey[] = "email";
constexpr char kAccountTypeKey[] = "account_type";

// Serialization values for account type.
constexpr char kGoogle[] = "google";
constexpr char kUnknown[] = "unknown";

// Prefix for GetAccountIdKey().
constexpr char kKeyGaiaIdPrefix[] = "g-";

}  // anonymous namespace

AccountId::AccountId() = default;

AccountId::AccountId(std::string_view user_email,
                     AccountType account_type,
                     const GaiaId& gaia_id)
    : user_email_(user_email), account_type_(account_type), gaia_id_(gaia_id) {
  DCHECK_EQ(user_email, gaia::CanonicalizeEmail(user_email));

  switch (account_type_) {
    case AccountType::UNKNOWN:
      CHECK(gaia_id_.empty());
      break;
    case AccountType::GOOGLE:
      // TODO(alemate): check gaia_id is not empty once it is required.
      break;
  }

  // Fail if e-mail looks similar to GaiaIdKey.
  LOG_ASSERT(!base::StartsWith(user_email, kKeyGaiaIdPrefix,
                               base::CompareCase::SENSITIVE) ||
             user_email.find('@') != std::string::npos)
      << "Bad e-mail: '" << user_email << "' with gaia_id='" << gaia_id_ << "'";
}

AccountId::AccountId(const AccountId& other) = default;

AccountId& AccountId::operator=(const AccountId& other) = default;

bool AccountId::operator==(const AccountId& other) const {
  if (this == &other) {
    return true;
  }
  if (account_type_ == AccountType::UNKNOWN ||
      other.account_type_ == AccountType::UNKNOWN) {
    return user_email_ == other.user_email_;
  }
  if (account_type_ != other.account_type_) {
    return false;
  }
  switch (account_type_) {
    case AccountType::UNKNOWN:
      NOTREACHED() << "Unknown account type";
    case AccountType::GOOGLE:
      return (gaia_id_ == other.gaia_id_ && user_email_ == other.user_email_) ||
             (!gaia_id_.empty() && gaia_id_ == other.gaia_id_) ||
             (!user_email_.empty() && user_email_ == other.user_email_);
  }
}

bool AccountId::operator<(const AccountId& right) const {
  // TODO(alemate): update this once all AccountId members are filled.
  return user_email_ < right.user_email_;
}

bool AccountId::empty() const {
  return gaia_id_.empty() && user_email_.empty() &&
         account_type_ == AccountType::UNKNOWN;
}

bool AccountId::is_valid() const {
  switch (account_type_) {
    case AccountType::GOOGLE:
      // TODO(http://b/279005619): Add an additional check for empty account ids
      // when this bug is fixed.
      return !user_email_.empty();
    case AccountType::UNKNOWN:
      return gaia_id_.empty() && !user_email_.empty();
  }
  NOTREACHED();
}

void AccountId::clear() {
  gaia_id_ = GaiaId();
  user_email_.clear();
  account_type_ = AccountType::UNKNOWN;
}

AccountType AccountId::GetAccountType() const {
  return account_type_;
}

const GaiaId& AccountId::GetGaiaId() const {
  if (account_type_ != AccountType::GOOGLE) {
    NOTIMPLEMENTED() << "Failed to get gaia_id for non-Google account.";
  }
  return gaia_id_;
}

const std::string& AccountId::GetUserEmail() const {
  return user_email_;
}

bool AccountId::HasAccountIdKey() const {
  switch (account_type_) {
    case AccountType::UNKNOWN:
      return false;
    case AccountType::GOOGLE:
      return !gaia_id_.empty();
  }
  NOTREACHED();
}

const std::string AccountId::GetAccountIdKey() const {
  switch (GetAccountType()) {
    case AccountType::UNKNOWN:
      NOTREACHED() << "Unknown account type";
    case AccountType::GOOGLE:
#ifdef NDEBUG
      if (gaia_id_.empty()) {
        LOG(FATAL) << "GetAccountIdKey(): no id for " << Serialize();
      }
#else
      CHECK(!gaia_id_.empty());
#endif
      return std::string(kKeyGaiaIdPrefix) + gaia_id_.ToString();
  }
}

void AccountId::SetUserEmail(std::string_view email) {
  DCHECK(email == gaia::CanonicalizeEmail(email));
  DCHECK(!email.empty());
  user_email_ = email;
}

// static
AccountId AccountId::FromNonCanonicalEmail(std::string_view email,
                                           const GaiaId& gaia_id,
                                           AccountType account_type) {
  DCHECK(!email.empty());
  return AccountId(gaia::CanonicalizeEmail(gaia::SanitizeEmail(email)),
                   account_type, gaia_id);
}

// static
AccountId AccountId::FromUserEmail(std::string_view email) {
  // TODO(alemate): DCHECK(!email.empty());
  return AccountId(email, AccountType::UNKNOWN, GaiaId());
}

// static
AccountId AccountId::FromUserEmailGaiaId(std::string_view email,
                                         const GaiaId& gaia_id) {
  DCHECK(!(email.empty() && gaia_id.empty()));
  return AccountId(email, AccountType::GOOGLE, gaia_id);
}

// static
AccountType AccountId::StringToAccountType(
    std::string_view account_type_string) {
  if (account_type_string == kGoogle) {
    return AccountType::GOOGLE;
  }
  if (account_type_string == kUnknown) {
    return AccountType::UNKNOWN;
  }
  NOTREACHED() << "Unknown account type " << account_type_string;
}

// static
const char* AccountId::AccountTypeToString(AccountType account_type) {
  switch (account_type) {
    case AccountType::GOOGLE:
      return kGoogle;
    case AccountType::UNKNOWN:
      return kUnknown;
  }
  return "";
}

std::string AccountId::Serialize() const {
  base::Value::Dict value;
  switch (GetAccountType()) {
    case AccountType::GOOGLE:
      value.Set(kGaiaIdKey, gaia_id_.ToString());
      break;
    case AccountType::UNKNOWN:
      break;
  }
  value.Set(kAccountTypeKey, AccountTypeToString(GetAccountType()));
  value.Set(kEmailKey, user_email_);

  std::string serialized;
  base::JSONWriter::Write(value, &serialized);
  return serialized;
}

// static
std::optional<AccountId> AccountId::Deserialize(std::string_view serialized) {
  std::optional<base::Value::Dict> value =
      base::JSONReader::ReadDict(serialized);
  if (!value) {
    return std::nullopt;
  }

  AccountType account_type = AccountType::GOOGLE;
  const std::string* gaia_id = value->FindString(kGaiaIdKey);
  const std::string* user_email = value->FindString(kEmailKey);
  const std::string* account_type_string = value->FindString(kAccountTypeKey);
  if (account_type_string) {
    account_type = StringToAccountType(*account_type_string);
  }

  switch (account_type) {
    case AccountType::GOOGLE:
      if (!gaia_id) {
        DLOG(ERROR) << "gaia_id is not found in '" << serialized << "'";
      }

      if (!user_email) {
        DLOG(ERROR) << "user_email is not found in '" << serialized << "'";
      }

      if (!gaia_id && !user_email) {
        return std::nullopt;
      }

      return FromUserEmailGaiaId(user_email ? *user_email : "",
                                 gaia_id ? GaiaId(*gaia_id) : GaiaId());

    case AccountType::UNKNOWN:
      if (!user_email) {
        return std::nullopt;
      }
      return FromUserEmail(*user_email);
  }
  return std::nullopt;
}

std::ostream& operator<<(std::ostream& stream, const AccountId& account_id) {
  stream << "{gaia_id: " << account_id.gaia_id_
         << ", email: " << account_id.user_email_ << ", type: "
         << static_cast<
                std::underlying_type<decltype(account_id.account_type_)>::type>(
                account_id.account_type_)
         << "}";
  return stream;
}

const AccountId& EmptyAccountId() {
  static const base::NoDestructor<AccountId> empty_account_id;
  return *empty_account_id;
}

namespace std {

std::size_t hash<AccountId>::operator()(const AccountId& user_id) const {
  return hash<std::string>()(user_id.GetUserEmail());
}

}  // namespace std
