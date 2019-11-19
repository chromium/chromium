// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_ID_ACCOUNT_ID_H_
#define COMPONENTS_ACCOUNT_ID_ACCOUNT_ID_H_

#include <stddef.h>

#include <functional>
#include <ostream>
#include <string>


enum class AccountType { UNKNOWN, GOOGLE, ACTIVE_DIRECTORY };

// Type that contains enough information to identify user.
//
// TODO(alemate): we are in the process of moving away from std::string as a
// type for storing user identifier to AccountId. At this time GaiaId is mostly
// empty, so this type is used as a replacement for e-mail string.
// But in near future AccountId will become full feature user identifier.
// TODO(alemate): Rename functions and fields to reflect different types of
// accounts. (see crbug.com/672253)
class AccountId {
 public:
  // Creates an empty account id.
  //
  // Note: This constructor is public as it is required for mojo serialization
  // To create an AccountId object, prefer using the static FromXXXX methods or
  // the EmptyAccountId method when creating an empty account id.
  AccountId();

  AccountId(const AccountId& other);

  // If any of the comparable AccountIds has AccountType == UNKNOWN then it
  // compares emails.
  // If both are not UNKNOWN and not equal then it returns false.
  // If AccountType == GOOGLE then it checks if either ids or emails are equal.
  // If AccountType == ACTIVE_DIRECTORY then it checks if ids and emails are
  // equal.
  bool operator==(const AccountId& other) const;
  bool operator!=(const AccountId& other) const;
  bool operator<(const AccountId& right) const;

  // empty() is deprecated. Use !is_valid() instead.
  bool empty() const;
  bool is_valid() const;
  void clear();

  AccountType GetAccountType() const;
  const std::string& GetGaiaId() const;
  const std::string& GetObjGuid() const;
  // Users of AccountId should make no assumptions on the format of email.
  // I.e. it cannot be used as account identifier, because it is (in general)
  // non-comparable.
  const std::string& GetUserEmail() const;

  // Returns true if |GetAccountIdKey| would return valid key.
  bool HasAccountIdKey() const;
  // This returns prefixed some string that can be used as a storage key.
  // You should make no assumptions on the format of this string.
  const std::string GetAccountIdKey() const;

  void SetUserEmail(const std::string& email);

  // This method is to be used during transition period only.
  // AccountId with UNKNOWN AccountType;
  static AccountId FromUserEmail(const std::string& user_email);
  // AccountId with GOOGLE AccountType;
  // This method is to be used during transition period only.
  static AccountId FromGaiaId(const std::string& gaia_id);
  // This method is the preferred way to construct AccountId if you have
  // full account information.
  // AccountId with GOOGLE AccountType;
  static AccountId FromUserEmailGaiaId(const std::string& user_email,
                                       const std::string& gaia_id);
  // These methods are used to construct Active Directory AccountIds.
  // AccountId with ACTIVE_DIRECTORY AccountType;
  static AccountId AdFromUserEmailObjGuid(const std::string& email,
                                          const std::string& obj_guid);
  // AccountId with ACTIVE_DIRECTORY AccountType;
  static AccountId AdFromObjGuid(const std::string& obj_guid);

  // Translation functions between AccountType and std::string. Used for
  // serialization.
  static AccountType StringToAccountType(
      const std::string& account_type_string);
  static std::string AccountTypeToString(const AccountType& account_type);

  // These are (for now) unstable and cannot be used to store serialized data to
  // persistent storage. Only in-memory storage is safe.
  // Serialize() returns JSON dictionary,
  // Deserialize() restores AccountId after serialization.
  std::string Serialize() const;
  static bool Deserialize(const std::string& serialized,
                          AccountId* out_account_id);

 private:
  friend std::ostream& operator<<(std::ostream&, const AccountId&);

  AccountId(const std::string& id,
            const std::string& user_email,
            const AccountType& account_type);

  std::string id_;
  std::string user_email_;
  AccountType account_type_ = AccountType::UNKNOWN;
};

// Overload << operator to allow logging of AccountIds.
std::ostream& operator<<(std::ostream& stream, const AccountId& account_id);

// Returns a reference to a singleton.
const AccountId& EmptyAccountId();

namespace std {

// Implement hashing of AccountId, so it can be used as a key in STL containers.
template <>
struct hash<AccountId> {
  std::size_t operator()(const AccountId& user_id) const;
};

}  // namespace std

#endif  // COMPONENTS_ACCOUNT_ID_ACCOUNT_ID_H_
