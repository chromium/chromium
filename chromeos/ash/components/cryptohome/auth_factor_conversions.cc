// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"

namespace cryptohome {

namespace {

using ::ash::ChallengeResponseKey;

}  // namespace

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
    case AuthFactorType::kSmartCard:
      return user_data_auth::AUTH_FACTOR_TYPE_SMART_CARD;
    case AuthFactorType::kLegacyFingerprint:
      return user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT;
  }
}

AuthFactorType ConvertFactorTypeFromProto(user_data_auth::AuthFactorType type) {
  switch (type) {
    case user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED:
      NOTREACHED() << "Unknown factor type should be handled separately";
      return AuthFactorType::kUnknownLegacy;
    case user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT:
      NOTREACHED() << "Fingerprint factor type should never be returned";
      return AuthFactorType::kUnknownLegacy;
    case user_data_auth::AUTH_FACTOR_TYPE_PASSWORD:
      return AuthFactorType::kPassword;
    case user_data_auth::AUTH_FACTOR_TYPE_PIN:
      return AuthFactorType::kPin;
    case user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY:
      return AuthFactorType::kRecovery;
    case user_data_auth::AUTH_FACTOR_TYPE_KIOSK:
      return AuthFactorType::kKiosk;
    case user_data_auth::AUTH_FACTOR_TYPE_SMART_CARD:
      return AuthFactorType::kSmartCard;
    default:
      NOTREACHED() << "Unknown auth factor type " << static_cast<int>(type);
      return AuthFactorType::kUnknownLegacy;
  }
}

user_data_auth::SmartCardSignatureAlgorithm
ChallengeSignatureAlgorithmToProtoEnum(
    ChallengeResponseKey::SignatureAlgorithm algorithm) {
  using Algorithm = ChallengeResponseKey::SignatureAlgorithm;
  switch (algorithm) {
    case Algorithm::kRsassaPkcs1V15Sha1:
      return user_data_auth::CHALLENGE_RSASSA_PKCS1_V1_5_SHA1;
    case Algorithm::kRsassaPkcs1V15Sha256:
      return user_data_auth::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256;
    case Algorithm::kRsassaPkcs1V15Sha384:
      return user_data_auth::CHALLENGE_RSASSA_PKCS1_V1_5_SHA384;
    case Algorithm::kRsassaPkcs1V15Sha512:
      return user_data_auth::CHALLENGE_RSASSA_PKCS1_V1_5_SHA512;
  }
  NOTREACHED();
}

void SerializeAuthFactor(const AuthFactor& factor,
                         user_data_auth::AuthFactor* out_proto) {
  out_proto->set_type(ConvertFactorTypeToProto(factor.ref().type()));
  out_proto->set_label(factor.ref().label().value());
  // Do not do anything with is_active_for_login yet.

  CHECK_NE(factor.GetCommonMetadata().chrome_version_last_updated().value(),
           kFallbackFactorVersion);
  CHECK_NE(factor.GetCommonMetadata().chromeos_version_last_updated().value(),
           kFallbackFactorVersion);

  out_proto->mutable_common_metadata()->set_chrome_version_last_updated(
      factor.GetCommonMetadata().chrome_version_last_updated().value());
  const auto& chromeos_version =
      factor.GetCommonMetadata().chromeos_version_last_updated().value();
  if (!chromeos_version.empty()) {
    out_proto->mutable_common_metadata()->set_chromeos_version_last_updated(
        chromeos_version);
  }

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
    case AuthFactorType::kSmartCard:
      out_proto->mutable_smart_card_metadata()->set_public_key_spki_der(
          factor.GetSmartCardMetadata().public_key_spki_der);
      break;
    case AuthFactorType::kLegacyFingerprint:
      LOG(FATAL) << "Legacy fingerprint factor type should never be serialized";
      break;
    case AuthFactorType::kUnknownLegacy:
      LOG(FATAL) << "Unknown factor type should never be serialized";
      break;
    default:
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
        const bool result = base::HexStringToString(
            recovery_creation.pub_key, proto_input->mutable_mediator_pub_key());
        CHECK(result);
        proto_input->set_user_gaia_id(recovery_creation.user_gaia_id);
        proto_input->set_device_user_id(recovery_creation.device_user_id);
      }
    } break;
    case AuthFactorType::kKiosk:
      // Just create an input.
      out_proto->mutable_kiosk_input();
      break;
    case AuthFactorType::kSmartCard: {
      auto* proto_input = out_proto->mutable_smart_card_input();
      proto_input->set_key_delegate_dbus_service_name(
          auth_input.GetSmartCardInput().key_delegate_dbus_service_name);
      for (auto algorithm :
           auth_input.GetSmartCardInput().signature_algorithms) {
        proto_input->add_signature_algorithms(
            ChallengeSignatureAlgorithmToProtoEnum(algorithm));
      }
      break;
    }
    case AuthFactorType::kLegacyFingerprint:
      // Legacy Fingerprint does not use any information from the Ash side,
      // only the signal. Creating empty input for `oneof` to work.
      out_proto->mutable_legacy_fingerprint_input();
      break;
    case AuthFactorType::kUnknownLegacy:
      LOG(FATAL) << "Unknown factor type should never be serialized";
      break;
    default:
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
    // TODO(b/243808147): Remove this hack after fixing cryptohome to return
    // `AUTH_FACTOR_TYPE_UNSPECIFIED` for legacy kiosk keysets.
    if (fallback_type == cryptohome::AuthFactorType::kKiosk &&
        type != cryptohome::AuthFactorType::kKiosk) {
      LOG(WARNING) << "Fixup kiosk key type for " << proto.label() << " "
                   << proto.type();
      type = cryptohome::AuthFactorType::kKiosk;
    }
  }
  AuthFactorRef ref(type, KeyLabel{proto.label()});
  ComponentVersion chrome_ver{kFallbackFactorVersion};
  ComponentVersion chromeos_ver{kFallbackFactorVersion};
  if (proto.has_common_metadata()) {
    if (!proto.common_metadata().chrome_version_last_updated().empty()) {
      chrome_ver = ComponentVersion(
          proto.common_metadata().chrome_version_last_updated());
    }
    if (!proto.common_metadata().chromeos_version_last_updated().empty()) {
      chromeos_ver = ComponentVersion(
          proto.common_metadata().chromeos_version_last_updated());
    }
  }
  AuthFactorCommonMetadata common_metadata{std::move(chrome_ver),
                                           std::move(chromeos_ver)};

  // Ignore is_active_for_login for now
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
    case AuthFactorType::kSmartCard: {
      DCHECK(proto.has_smart_card_metadata());
      SmartCardMetadata smart_card_metadata;
      smart_card_metadata.public_key_spki_der =
          proto.smart_card_metadata().public_key_spki_der();
      return AuthFactor(std::move(ref), std::move(common_metadata),
                        std::move(smart_card_metadata));
    }
    case AuthFactorType::kLegacyFingerprint: {
      LOG(FATAL) << "Legacy fingerprint factor should never be returned"
                 << " by cryptohome.";
      __builtin_unreachable();
    }
    case AuthFactorType::kUnknownLegacy:
      LOG(FATAL) << "Should already be handled above";
      __builtin_unreachable();
    default:
      NOTIMPLEMENTED() << "Auth factor " << static_cast<int>(type)
                       << " is not implemented in cryptohome yet.";
      return AuthFactor(std::move(ref), std::move(common_metadata));
  }
}

}  // namespace cryptohome
