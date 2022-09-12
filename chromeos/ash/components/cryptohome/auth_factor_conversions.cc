// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"

namespace cryptohome {

namespace {

user_data_auth::AuthFactorType ConvertFactorTypeToProto(AuthFactorType type) {
  switch (type) {
    case AuthFactorType::kUnknownLegacy:
      NOTREACHED() << "Unknown factor type should never be sent to cryptohome";
      return user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED;
    case AuthFactorType::kPassword:
      return user_data_auth::AUTH_FACTOR_TYPE_PASSWORD;
    case AuthFactorType::kPin:
      return user_data_auth::AUTH_FACTOR_TYPE_PIN;
    case AuthFactorType::kRecovery:
      return user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY;
    case AuthFactorType::kKiosk:
      return user_data_auth::AUTH_FACTOR_TYPE_KIOSK;
    case AuthFactorType::kLegacyFingerprint:
    case AuthFactorType::kSmartCard:
      NOTIMPLEMENTED() << "Auth factor " << static_cast<int>(type)
                       << " is not implemented in cryptohome yet.";
      return user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED;
  }
}

}  // namespace

AuthFactorType ConvertFactorTypeFromProto(user_data_auth::AuthFactorType type) {
  switch (type) {
    case user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED:
      NOTREACHED() << "Unknown factor type should be handled separately";
      return AuthFactorType::kUnknownLegacy;
    case user_data_auth::AUTH_FACTOR_TYPE_PASSWORD:
      return AuthFactorType::kPassword;
    case user_data_auth::AUTH_FACTOR_TYPE_PIN:
      return AuthFactorType::kPin;
    case user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY:
      return AuthFactorType::kRecovery;
    case user_data_auth::AUTH_FACTOR_TYPE_KIOSK:
      return AuthFactorType::kKiosk;
    default:
      NOTREACHED() << "Unknown auth factor type " << static_cast<int>(type);
      return AuthFactorType::kUnknownLegacy;
  }
}

void SerializeAuthFactor(const AuthFactor& factor,
                         user_data_auth::AuthFactor* out_proto) {
  out_proto->set_type(ConvertFactorTypeToProto(factor.ref().type()));
  out_proto->set_label(factor.ref().label().value());
  // Do not do anything with is_active_for_login yet.

  // TODO(b/241259026): fill in common metadata.

  switch (factor.ref().type()) {
    case AuthFactorType::kPassword:
      out_proto->mutable_password_metadata();
      break;
    case AuthFactorType::kPin:
      out_proto->mutable_pin_metadata();
      break;
    case AuthFactorType::kRecovery:
      out_proto->mutable_cryptohome_recovery_metadata();
      break;
    case AuthFactorType::kKiosk:
      out_proto->mutable_kiosk_metadata();
      break;
    case AuthFactorType::kUnknownLegacy:
      LOG(FATAL) << "Unknown factor type should never be serialized";
      break;
    case AuthFactorType::kLegacyFingerprint:
    case AuthFactorType::kSmartCard:
      NOTIMPLEMENTED() << "Auth factor "
                       << static_cast<int>(factor.ref().type())
                       << " is not implemented in cryptohome yet.";
  }
}

void SerializeAuthInput(const AuthFactorRef& ref,
                        const AuthFactorInput& auth_input,
                        user_data_auth::AuthInput* out_proto) {
  DCHECK_EQ(ref.type(), auth_input.GetType());
  switch (auth_input.GetType()) {
    case AuthFactorType::kPassword:
      out_proto->mutable_password_input()->set_secret(
          auth_input.GetPasswordInput().hashed_password);
      break;
    case AuthFactorType::kPin:
      out_proto->mutable_pin_input()->set_secret(
          auth_input.GetPinInput().hashed_pin);
      break;
    case AuthFactorType::kRecovery: {
      auto* proto_input = out_proto->mutable_cryptohome_recovery_input();
      if (auth_input.UsableForAuthentication()) {
        const auto& recovery_auth = auth_input.GetRecoveryAuthenticationInput();
        proto_input->set_epoch_response(recovery_auth.epoch_data);
        proto_input->set_recovery_response(recovery_auth.recovery_data);
      } else {
        const auto& recovery_creation = auth_input.GetRecoveryCreationInput();
        proto_input->set_mediator_pub_key(recovery_creation.pub_key);
      }
    } break;
    case AuthFactorType::kKiosk:
      // Just create an input.
      out_proto->mutable_kiosk_input();
      break;
    case AuthFactorType::kUnknownLegacy:
      LOG(FATAL) << "Unknown factor type should never be serialized";
      break;
    case AuthFactorType::kLegacyFingerprint:
    case AuthFactorType::kSmartCard:
      NOTIMPLEMENTED() << "Auth factor "
                       << static_cast<int>(auth_input.GetType())
                       << " is not implemented in cryptohome yet.";
      break;
  }
}

AuthFactor DeserializeAuthFactor(const user_data_auth::AuthFactor& proto,
                                 AuthFactorType fallback_type) {
  AuthFactorType type;
  if (proto.type() == user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED) {
    LOG(WARNING) << "Unspecified auth factor type found, treating it as a "
                 << static_cast<int>(fallback_type);
    type = fallback_type;
  } else {
    type = ConvertFactorTypeFromProto(proto.type());
  }
  AuthFactorRef ref(type, KeyLabel{proto.label()});
  AuthFactorCommonMetadata common_metadata;
  // Ignore is_active_for_login for now
  // TODO(b/241259026) : fill in common metadata
  switch (type) {
    case AuthFactorType::kPassword:
      return AuthFactor(std::move(ref), std::move(common_metadata));
    case AuthFactorType::kRecovery:
      return AuthFactor(std::move(ref), std::move(common_metadata));
    case AuthFactorType::kKiosk:
      return AuthFactor(std::move(ref), std::move(common_metadata));
    case AuthFactorType::kPin: {
      DCHECK(proto.has_pin_metadata());
      PinStatus pin_status{proto.pin_metadata().auth_locked()};
      return AuthFactor(std::move(ref), std::move(common_metadata),
                        std::move(pin_status));
    }
    case AuthFactorType::kUnknownLegacy:
      LOG(FATAL) << "Should already be handled above";
      __builtin_unreachable();
    case AuthFactorType::kLegacyFingerprint:
    case AuthFactorType::kSmartCard:
      NOTIMPLEMENTED() << "Auth factor " << static_cast<int>(type)
                       << " is not implemented in cryptohome yet.";
      return AuthFactor(std::move(ref), std::move(common_metadata));
  }
}

}  // namespace cryptohome
