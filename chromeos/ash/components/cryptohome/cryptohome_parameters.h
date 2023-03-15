// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CRYPTOHOME_PARAMETERS_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CRYPTOHOME_PARAMETERS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"

class AccountId;

namespace cryptohome {

enum AuthKeyPrivileges {
  PRIV_ADD = 1 << 1,      // Can add new keys.
  PRIV_REMOVE = 1 << 2,   // Can remove other keys.
  PRIV_MIGRATE = 1 << 3,  // Destroy all keys and replace with new.
  PRIV_DEFAULT = PRIV_ADD | PRIV_REMOVE | PRIV_MIGRATE
};

// Identification of the user calling cryptohome method.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) Identification {
 public:
  Identification();

  explicit Identification(const AccountId& account_id);

  bool operator==(const Identification& other) const;

  // This method should be used for migration purpose only.
  static Identification FromString(const std::string& id);

  const std::string& id() const { return id_; }

  bool operator<(const Identification& right) const;

 private:
  explicit Identification(const std::string&);

  std::string id_;
};

// Creates AccountIdentifier from AccountId.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
AccountIdentifier CreateAccountIdentifierFromAccountId(const AccountId& id);

// Creates AccountIdentifier from Identification.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
AccountIdentifier CreateAccountIdentifierFromIdentification(
    const Identification& id);

// Definition of the key (e.g. password) for the cryptohome.
// It contains authorization data along with extra parameters like permissions
// associated with this key.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) KeyDefinition {
  enum Type {
    // Password-based key. The password's text or its hashed/transformed
    // representation is stored in |secret|. The |challenge_response_keys| field
    // should be empty.
    TYPE_PASSWORD = 0,
    // The challenge-response type of key. Information about the keys to be
    // challenged is stored in |challenge_response_keys|, while |secret| should
    // be empty.
    TYPE_CHALLENGE_RESPONSE = 1,
    // DEPRECATED:
    // Fingerprint-based key. It doesn't carry secrets but indicates that
    // cryptohome needs to query fingerprint scan results from biod and
    // compare with the identity passed along with the key.
    // TYPE_FINGERPRINT = 2,
    // Public mount is used by Kiosk sessions. This type of key does not have
    // any secret, instead crypotohomed would generate a key based on user
    // identity.
    TYPE_PUBLIC_MOUNT = 3,
  };

  // This struct holds metadata that will be stored alongside the key. Each
  // |ProviderData| entry must have a |name| and may hold either a |number| or a
  // sequence of |bytes|. The metadata is entirely opaque to cryptohome. It is
  // stored with the key and returned when requested but is never interpreted by
  // cryptohome in any way. The metadata can be used to store information such
  // as the hashing algorithm and the salt used to create the key.
  struct ProviderData {
    ProviderData();
    explicit ProviderData(const std::string& name);
    explicit ProviderData(const ProviderData& other);
    ProviderData(const std::string& name, int64_t number);
    ProviderData(const std::string& name, const std::string& bytes);
    void operator=(const ProviderData& other);
    ~ProviderData();

    bool operator==(const ProviderData& other) const;

    std::string name;
    std::unique_ptr<int64_t> number;
    std::unique_ptr<std::string> bytes;
  };

  struct Policy {
    bool operator==(const Policy& other) const;
    bool operator!=(const Policy& other) const;

    bool low_entropy_credential = false;
    bool auth_locked = false;
  };

  // Creates an instance with the TYPE_PASSWORD type.
  static KeyDefinition CreateForPassword(const std::string& secret,
                                         const KeyLabel& label,
                                         int privileges);
  // Creates an instance with the TYPE_CHALLENGE_RESPONSE type.
  static KeyDefinition CreateForChallengeResponse(
      const std::vector<ash::ChallengeResponseKey>& challenge_response_keys,
      const KeyLabel& label,
      int privileges);

  KeyDefinition();
  KeyDefinition(const KeyDefinition& other);
  ~KeyDefinition();

  bool operator==(const KeyDefinition& other) const;

  Type type = TYPE_PASSWORD;
  KeyLabel label;
  // Privileges associated with key. Combination of |AuthKeyPrivileges| values.
  int privileges = 0;
  Policy policy;
  int revision = 0;
  std::string secret;
  std::vector<ash::ChallengeResponseKey> challenge_response_keys;

  std::vector<ProviderData> provider_data;
};

// Authorization attempt data for user.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) Authorization {
  Authorization(const std::string& key, const KeyLabel& label);
  explicit Authorization(const KeyDefinition& key);
  ~Authorization();

  bool operator==(const Authorization& other) const;

  std::string key;
  KeyLabel label;
};

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CRYPTOHOME_PARAMETERS_H_
