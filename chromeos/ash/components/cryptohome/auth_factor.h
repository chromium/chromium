// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace cryptohome {

enum class AuthFactorType {
  // Special edge case - on old ChromeOS versions Kiosk keys and passwords for
  // regular users had no metadata to distinguish them on cryptohome level,
  // only Chrome can do that based on UserType.
  // This type can be returned when retrieving data from cryptohome,
  // but should not be used in any data passed from chrome to cryptohome.
  kUnknownLegacy,
  kPassword,
  kPin,
  kRecovery,
  kSmartCard,
  kKiosk,
  kLegacyFingerprint
};

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
  AuthFactorCommonMetadata();
  ~AuthFactorCommonMetadata();

  AuthFactorCommonMetadata(AuthFactorCommonMetadata&&) noexcept;
  AuthFactorCommonMetadata& operator=(AuthFactorCommonMetadata&&) noexcept;

  AuthFactorCommonMetadata(const AuthFactorCommonMetadata&);
  AuthFactorCommonMetadata& operator=(const AuthFactorCommonMetadata&);
};

// Per-factor statuses (read-only properties set by cryptohomed):

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) PinStatus {
  bool auth_locked;
};

// Factor-specific metadata:

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) SmartCardMetadata {
  std::string public_key_spki_der;
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
             PinStatus status);

  AuthFactor(AuthFactor&&) noexcept;
  AuthFactor& operator=(AuthFactor&&) noexcept;
  AuthFactor(const AuthFactor&);
  AuthFactor& operator=(const AuthFactor&);

  ~AuthFactor();

  const AuthFactorRef& ref() const;
  const AuthFactorCommonMetadata& GetCommonMetadata() const;

  // Fails if type does not match:
  const PinStatus& GetPinStatus() const;
  const SmartCardMetadata& GetSmartCardMetadata() const;

 private:
  AuthFactorRef ref_;
  AuthFactorCommonMetadata common_metadata_;
  absl::variant<absl::monostate, PinStatus> factor_status_;
  absl::variant<absl::monostate, SmartCardMetadata> factor_metadata_;
};

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_H_
