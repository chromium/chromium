// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_id/account_id.h"

#include <functional>
#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

// Serialization keys
constexpr char kGaiaIdKey[] = "gaia_id";
constexpr char kEmailKey[] = "email";
constexpr char kObjGuid[] = "obj_guid";
constexpr char kAccountTypeKey[] = "account_type";

// Serialization values for account type.
constexpr char kGoogle[] = "google";
constexpr char kAd[] = "ad";
constexpr char kUnknown[] = "unknown";

// Prefix for GetAccountIdKey().
constexpr char kKeyGaiaIdPrefix[] = "g-";
constexpr char kKeyAdIdPrefix[] = "a-";

}  // anonymous namespace

AccountId::AccountId() = default;

AccountId::AccountId(std::string_view id,
                     std::string_view user_email,
                     AccountType account_type)
    : id_(id), user_email_(user_email), account_type_(account_type) {
  DCHECK_EQ(user_email, gaia::CanonicalizeEmail(user_email));
  DCHECK(account_type != AccountType::UNKNOWN || id.empty());
  DCHECK(account_type != AccountType::ACTIVE_DIRECTORY || !id.empty());
  // Fail if e-mail looks similar to GaiaIdKey.
  LOG_ASSERT(!base::StartsWith(user_email, kKeyGaiaIdPrefix,
                               base::CompareCase::SENSITIVE) ||
             user_email.find('@') != std::string::npos)
      << "Bad e-mail: '" << user_email << "' with gaia_id='" << id << "'";

  // TODO(alemate): DCHECK(!email.empty());
  // TODO(alemate): check gaia_id is not empty once it is required.
}

AccountId::AccountId(const AccountId& other) = default;

AccountId& AccountId::operator=(const AccountId& other) = default;

bool AccountId::operator==(const AccountId& other) const {
  if (this == &other)
    return true;
  if (account_type_ == AccountType::UNKNOWN ||
      other.account_type_ == AccountType::UNKNOWN)
    return user_email_ == other.user_email_;
  if (account_type_ != other.account_type_)
    return false;
  switch (account_type_) {
    case AccountType::GOOGLE:
      return (id_ == other.id_ && user_email_ == other.user_email_) ||
             (!id_.empty() && id_ == other.id_) ||
             (!user_email_.empty() && user_email_ == other.user_email_);
    case AccountType::ACTIVE_DIRECTORY:
      return id_ == other.id_ && user_email_ == other.user_email_;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown account type";
  }
  return false;
}

bool AccountId::operator!=(const AccountId& other) const {
  return !operator==(other);
}

bool AccountId::operator<(const AccountId& right) const {
  // TODO(alemate): update this once all AccountId members are filled.
  return user_email_ < right.user_email_;
}

bool AccountId::empty() const {
  return id_.empty() && user_email_.empty() &&
         account_type_ == AccountType::UNKNOWN;
}

bool AccountId::is_valid() const {
  switch (account_type_) {
    case AccountType::GOOGLE:
      // TODO(http://b/279005619): Add an additional check for empty account ids
      // when this bug is fixed.
      return !user_email_.empty();
    case AccountType::ACTIVE_DIRECTORY:
      return !id_.empty() && !user_email_.empty();
    case AccountType::UNKNOWN:
      return id_.empty() && !user_email_.empty();
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

void AccountId::clear() {
  id_.clear();
  user_email_.clear();
  account_type_ = AccountType::UNKNOWN;
}

AccountType AccountId::GetAccountType() const {
  return account_type_;
}

const std::string& AccountId::GetGaiaId() const {
  if (account_type_ != AccountType::GOOGLE)
    NOTIMPLEMENTED() << "Failed to get gaia_id for non-Google account.";
  return id_;
}

const std::string& AccountId::GetObjGuid() const {
  if (account_type_ != AccountType::ACTIVE_DIRECTORY)
    NOTIMPLEMENTED()
        << "Failed to get obj_guid for non-Active Directory account.";
  return id_;
}

const std::string& AccountId::GetUserEmail() const {
  return user_email_;
}

bool AccountId::HasAccountIdKey() const {
  return account_type_ != AccountType::UNKNOWN && !id_.empty();
}

const std::string AccountId::GetAccountIdKey() const {
#ifdef NDEBUG
  if (id_.empty())
    LOG(FATAL) << "GetAccountIdKey(): no id for " << Serialize();
#else
  CHECK(!id_.empty());
#endif
  switch (GetAccountType()) {
    case AccountType::GOOGLE:
      return std::string(kKeyGaiaIdPrefix) + id_;
    case AccountType::ACTIVE_DIRECTORY:
      return std::string(kKeyAdIdPrefix) + id_;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown account type";
  }
  return std::string();
}

void AccountId::SetUserEmail(std::string_view email) {
  DCHECK(email == gaia::CanonicalizeEmail(email));
  DCHECK(!email.empty());
  user_email_ = email;
}

// static
AccountId AccountId::FromNonCanonicalEmail(std::string_view email,
                                           std::string_view gaia_id,
                                           AccountType account_type) {
  DCHECK(!email.empty());
  return AccountId(gaia_id, gaia::CanonicalizeEmail(gaia::SanitizeEmail(email)),
                   account_type);
}

// static
AccountId AccountId::FromUserEmail(std::string_view email) {
  // TODO(alemate): DCHECK(!email.empty());
  return AccountId(/*id=*/"", email, AccountType::UNKNOWN);
}

// static
AccountId AccountId::FromUserEmailGaiaId(std::string_view email,
                                         std::string_view gaia_id) {
  DCHECK(!(email.empty() && gaia_id.empty()));
  return AccountId(gaia_id, email, AccountType::GOOGLE);
}

// static
AccountId AccountId::AdFromUserEmailObjGuid(std::string_view email,
                                            std::string_view obj_guid) {
  DCHECK(!email.empty() && !obj_guid.empty());
  return AccountId(obj_guid, email, AccountType::ACTIVE_DIRECTORY);
}

// static
AccountType AccountId::StringToAccountType(
    std::string_view account_type_string) {
  if (account_type_string == kGoogle)
    return AccountType::GOOGLE;
  if (account_type_string == kAd)
    return AccountType::ACTIVE_DIRECTORY;
  if (account_type_string == kUnknown)
    return AccountType::UNKNOWN;
  NOTREACHED_IN_MIGRATION() << "Unknown account type " << account_type_string;
  return AccountType::UNKNOWN;
}

// static
const char* AccountId::AccountTypeToString(AccountType account_type) {
  switch (account_type) {
    case AccountType::GOOGLE:
      return kGoogle;
    case AccountType::ACTIVE_DIRECTORY:
      return kAd;
    case AccountType::UNKNOWN:
      return kUnknown;
  }
  return "";
}

std::string AccountId::Serialize() const {
  base::Value::Dict value;
  switch (GetAccountType()) {
    case AccountType::GOOGLE:
      value.Set(kGaiaIdKey, id_);
      break;
    case AccountType::ACTIVE_DIRECTORY:
      value.Set(kObjGuid, id_);
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
  std::optional<base::Value> value(base::JSONReader::Read(serialized));
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }

  AccountType account_type = AccountType::GOOGLE;
  base::Value::Dict& dict = value->GetDict();
  const std::string* gaia_id = dict.FindString(kGaiaIdKey);
  const std::string* user_email = dict.FindString(kEmailKey);
  const std::string* obj_guid = dict.FindString(kObjGuid);
  const std::string* account_type_string = dict.FindString(kAccountTypeKey);
  if (account_type_string) {
    account_type = StringToAccountType(*account_type_string);
  }

  switch (account_type) {
    case AccountType::GOOGLE:
      if (obj_guid) {
        DLOG(ERROR) << "AccountType is 'google' but obj_guid is found in '"
                    << serialized << "'";
      }

      if (!gaia_id)
        DLOG(ERROR) << "gaia_id is not found in '" << serialized << "'";

      if (!user_email)
        DLOG(ERROR) << "user_email is not found in '" << serialized << "'";

      if (!gaia_id && !user_email) {
        return std::nullopt;
      }

      return FromUserEmailGaiaId(user_email ? *user_email : "",
                                 gaia_id ? *gaia_id : "");

    case AccountType::ACTIVE_DIRECTORY:
      if (gaia_id) {
        DLOG(ERROR)
            << "AccountType is 'active directory' but gaia_id is found in '"
            << serialized << "'";
      }

      if (!obj_guid) {
        DLOG(ERROR) << "obj_guid is not found in '" << serialized << "'";
        return std::nullopt;
      }

      if (!user_email) {
        DLOG(ERROR) << "user_email is not found in '" << serialized << "'";
      }

      if (!obj_guid || !user_email) {
        return std::nullopt;
      }

      return AdFromUserEmailObjGuid(*user_email, *obj_guid);

    case AccountType::UNKNOWN:
      if (!user_email) {
        return std::nullopt;
      }
      return FromUserEmail(*user_email);
  }
  return std::nullopt;
}

std::ostream& operator<<(std::ostream& stream, const AccountId& account_id) {
  stream << "{id: " << account_id.id_ << ", email: " << account_id.user_email_
         << ", type: "
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
