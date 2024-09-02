// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace cryptohome {

// All authfactors created/modified after AuthFactor API is launched should
// have OS/Chrome versions filled in CommonMetadata.
// However, any factor created before the launch would miss such information.
// In order to keep rest of the code simple, this "zero" value would be used
// upon receiving AuthFactor from cryptohome.
// Note that this value should not be passed back to cryptohome, this
// is guarded by CHECKs in serialization code.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
extern const char kFallbackFactorVersion[];

enum class AuthFactorType {
  // Special edge case - on old ChromeOS versions Kiosk keys and passwords for
  // regular users had no metadata to distinguish them on cryptohome level,
  // only Chrome can do that based on UserType.
  // This type can be returned when retrieving data from cryptohome,
  // but should not be used in any data passed from chrome to cryptohome.
  // This factor type is not included in `AuthFactorsSet`.
  kUnknownLegacy,

  // This is a synthetic factor, there is no actual key associated with this
  // factor, it is only used as indicator of specific authentication mode.
  // As such it can never be returned by cryptohome, and is not included in
  // `AuthFactorsSet`.
  kLegacyFingerprint,

  kPassword,
  kPin,
  kRecovery,
  kSmartCard,
  kKiosk,
  kFingerprint,
};

using AuthFactorsSet = base::
    EnumSet<AuthFactorType, AuthFactorType::kPassword, AuthFactorType::kKiosk>;

// Reference to a particular AuthFactor.
// While `label` uniquely identifies factor across all factor types,
// it is convenient to pass AuthFactorType along.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) AuthFactorRef {
 public:
  AuthFactorRef(AuthFactorType type, KeyLabel label);

  AuthFactorRef(AuthFactorRef&&);
  AuthFactorRef& operator=(AuthFactorRef&&);

  AuthFactorRef(const AuthFactorRef&);
  AuthFactorRef& operator=(const AuthFactorRef&);

  ~AuthFactorRef();

  bool operator==(const AuthFactorRef& other) const;

  AuthFactorType type() const { return type_; }

  const KeyLabel& label() const { return label_; }

 private:
  AuthFactorType type_;
  KeyLabel label_;
};

// Each auth factor supported by cryptohome has 4 types of data associated with
// it:
//   * factor identifiers: type and label (though label can be changed by
//     cryptohome);
//   * factor input: part of data that is write-only by Chrome, e.g.
//     during setting up a factor, or attempting an authentication;
//   * factor status: data that is set by cryptohome and is read-only on the
//     Chrome side, e.g. PIN lockout status;
//   * factor metadata: non-identifying data associated with factor that can
//     be both read and written by Chrome.

// Common metadata that should be defined for each auth factor.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
    AuthFactorCommonMetadata {
 public:
  // This constructor should be used for AuthFactors created on the Chrome side.
  // It fills in current Chrome version, leaving ChromeOS version empty.
  AuthFactorCommonMetadata();
  AuthFactorCommonMetadata(ComponentVersion chrome, ComponentVersion chromeos);
  ~AuthFactorCommonMetadata();

  AuthFactorCommonMetadata(AuthFactorCommonMetadata&&) noexcept;
  AuthFactorCommonMetadata& operator=(AuthFactorCommonMetadata&&) noexcept;

  AuthFactorCommonMetadata(const AuthFactorCommonMetadata&);
  AuthFactorCommonMetadata& operator=(const AuthFactorCommonMetadata&);

  // Should only be used for testing purposes.
  bool operator==(const AuthFactorCommonMetadata& other) const;

  const ComponentVersion& chrome_version_last_updated() const {
    return chrome_version_last_updated_;
  }

  const ComponentVersion& chromeos_version_last_updated() const {
    return chromeos_version_last_updated_;
  }

 private:
  ComponentVersion chrome_version_last_updated_;
  ComponentVersion chromeos_version_last_updated_;
};

// Per-factor statuses (read-only properties set by cryptohomed):
// TODO(b/341733466): Individual per-factor statuses are discouraged.
// A general auth factor status is already returned by cryptohomed for
// each AuthFactorWithStatus.

// PinStatus provides the pin status info returned from cryptohomed.
// PinStatus only represents a status snapshot at the time of its
// construction. Care must be taken in checking the freshness of
// any PinStatus object.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) PinStatus {
 public:
  // Default constructor: the pin factor is immediately available.
  PinStatus();
  // Constructor takes one TimeDelta value:
  // |available_in| indicates a timeout after which the factor will become
  // available.
  //   0 means the factor is immediately available.
  //   TimeDelta::Max() means the factor is locked out indefinitely.
  PinStatus(base::TimeDelta available_in);

  PinStatus(PinStatus&&) noexcept;
  PinStatus& operator=(PinStatus&&) noexcept;

  PinStatus(const PinStatus&);
  PinStatus& operator=(const PinStatus&);

  ~PinStatus();

  // The time when the pin auth factor will be available.
  // If locked out indefinitely, return Time::Max().
  base::Time AvailableAt() const;

  // Indicates a not-avaiable pin.
  bool IsLockedFactor() const;

 private:
  base::Time available_at_;
};

// Represents the time when the pin auth factor will be available. If the field
// is not present, it means the PIN is enabled or disabled permanently.
using PinLockAvailability = std::optional<base::Time>;

// Common types used in factor-specific metadata:

// The hash information of an auth factor that is used to generate the actual
// secret. This can be used to generate the recoverable key store for the auth
// factor.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
    KnowledgeFactorHashInfo {
  KnowledgeFactorHashAlgorithmWrapper algorithm;
  std::string salt;
  bool should_generate_key_store;
};

// Factor-specific metadata:

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) SmartCardMetadata {
  std::string public_key_spki_der;
};

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
    CryptohomeRecoveryMetadata {
  std::string mediator_pub_key;
};

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) PasswordMetadata {
 public:
  // Constructors that do/don't include the hash info field. Difference with
  // online and local password metadata is that only local password metadata
  // needs to generate recoverable key stores.
  // Generating recoverable key stores will make the password a recovery
  // factor for some Google sync services, and we only want to do it for local
  // passwords but not GAIA passwords. This is because the user already needs
  // their GAIA password to access the Google sync services, so making the GAIA
  // password a sync recovery factor doesn't provide any extra layer of
  // security.
  static PasswordMetadata CreateWithoutSalt();
  static PasswordMetadata CreateForOnlinePassword(SystemSalt salt);
  static PasswordMetadata CreateForLocalPassword(SystemSalt salt);

  PasswordMetadata(PasswordMetadata&&) noexcept;
  PasswordMetadata& operator=(PasswordMetadata&&) noexcept;

  PasswordMetadata(const PasswordMetadata&);
  PasswordMetadata& operator=(const PasswordMetadata&);

  ~PasswordMetadata();

  const std::optional<KnowledgeFactorHashInfo>& hash_info() const {
    return hash_info_;
  }

 private:
  PasswordMetadata(std::optional<KnowledgeFactorHashInfo> hash_info);

  std::optional<KnowledgeFactorHashInfo> hash_info_;
};

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) PinMetadata {
 public:
  static PinMetadata CreateWithoutSalt();
  static PinMetadata Create(PinSalt salt);

  PinMetadata(PinMetadata&&) noexcept;
  PinMetadata& operator=(PinMetadata&&) noexcept;

  PinMetadata(const PinMetadata&);
  PinMetadata& operator=(const PinMetadata&);

  ~PinMetadata();

  const std::optional<KnowledgeFactorHashInfo>& hash_info() const {
    return hash_info_;
  }

 private:
  PinMetadata(std::optional<KnowledgeFactorHashInfo> hash_info);

  std::optional<KnowledgeFactorHashInfo> hash_info_;
};

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) FingerprintMetadata {
};

// AuthFactor definition.
// If it is obtainted from `cryptohome` it will contain factor-specific status,
// otherwise it would only contain identity and metadata.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) AuthFactor {
 public:
  AuthFactor(AuthFactorRef ref, AuthFactorCommonMetadata metadata);
  AuthFactor(AuthFactorRef ref,
             AuthFactorCommonMetadata metadata,
             SmartCardMetadata smartcard_metadata);
  AuthFactor(AuthFactorRef ref,
             AuthFactorCommonMetadata metadata,
             CryptohomeRecoveryMetadata recovery_metadata);
  AuthFactor(AuthFactorRef ref,
             AuthFactorCommonMetadata metadata,
             PasswordMetadata password_metadata);
  AuthFactor(AuthFactorRef ref,
             AuthFactorCommonMetadata metadata,
             PinMetadata pin_metadata);
  AuthFactor(AuthFactorRef ref,
             AuthFactorCommonMetadata metadata,
             PinMetadata pin_metadata,
             PinStatus status);
  AuthFactor(AuthFactorRef ref,
             AuthFactorCommonMetadata metadata,
             FingerprintMetadata fingerprint_metadata);

  AuthFactor(AuthFactor&&) noexcept;
  AuthFactor& operator=(AuthFactor&&) noexcept;
  AuthFactor(const AuthFactor&);
  AuthFactor& operator=(const AuthFactor&);

  ~AuthFactor();

  // Should only be used for testing purposes.
  bool operator==(const AuthFactor& other) const;

  const AuthFactorRef& ref() const;
  const AuthFactorCommonMetadata& GetCommonMetadata() const;

  // Fails if type does not match:
  const PinStatus& GetPinStatus() const;
  const SmartCardMetadata& GetSmartCardMetadata() const;
  const CryptohomeRecoveryMetadata& GetCryptohomeRecoveryMetadata() const;
  const PasswordMetadata& GetPasswordMetadata() const;
  const PinMetadata& GetPinMetadata() const;
  const FingerprintMetadata& GetFingerprintMetadata() const;

 private:
  AuthFactorRef ref_;
  AuthFactorCommonMetadata common_metadata_;
  absl::variant<absl::monostate,
                SmartCardMetadata,
                CryptohomeRecoveryMetadata,
                PasswordMetadata,
                PinMetadata,
                FingerprintMetadata>
      factor_metadata_;
  absl::variant<absl::monostate, PinStatus> factor_status_;
};

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_
