// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
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

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) PinStatus {
  bool auth_locked;
};

// Factor-specific metadata:

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) SmartCardMetadata {
  std::string public_key_spki_der;
};

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
    CryptohomeRecoveryMetadata {
  std::string mediator_pub_key;
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
             PinStatus status);

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

 private:
  AuthFactorRef ref_;
  AuthFactorCommonMetadata common_metadata_;
  absl::variant<absl::monostate, PinStatus> factor_status_;
  absl::variant<absl::monostate, SmartCardMetadata, CryptohomeRecoveryMetadata>
      factor_metadata_;
};

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_
