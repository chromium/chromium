// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"

#include <optional>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/auth_factor_input.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/recoverable_key_store.pb.h"

namespace cryptohome {

namespace {

using ::ash::ChallengeResponseKey;

KnowledgeFactorHashAlgorithm ConvertHashTypeToProto(
    KnowledgeFactorHashAlgorithmWrapper algorithm) {
  using Algorithm = KnowledgeFactorHashAlgorithmWrapper;
  switch (algorithm) {
    case Algorithm::kSha256TopHalf:
      return KnowledgeFactorHashAlgorithm::HASH_TYPE_SHA256_TOP_HALF;
    case Algorithm::kPbkdf2Aes2561234:
      return KnowledgeFactorHashAlgorithm::HASH_TYPE_PBKDF2_AES256_1234;
  }
}

void ConvertKnowledgeFactorHashInfoToProto(
    const KnowledgeFactorHashInfo& hash_info,
    user_data_auth::KnowledgeFactorHashInfo& hash_info_proto) {
  hash_info_proto.set_algorithm(ConvertHashTypeToProto(hash_info.algorithm));
  hash_info_proto.set_salt(hash_info.salt);
  hash_info_proto.set_should_generate_key_store(
      hash_info.should_generate_key_store);
}

PinStatus PasrePinFactorStatus(const user_data_auth::StatusInfo& proto) {
  base::TimeDelta available_in = base::TimeDelta::Max();
  if (proto.time_available_in() != std::numeric_limits<uint64_t>::max()) {
    available_in = base::Milliseconds(proto.time_available_in());
  }
  CHECK(!available_in.is_negative());
  return PinStatus{available_in};
}

PasswordMetadata ParsePasswordMetadata(
    const user_data_auth::AuthFactor& proto) {
  std::optional<KnowledgeFactorHashInfo> hash_info;
  if (proto.has_password_metadata() &&
      proto.password_metadata().has_hash_info()) {
    const user_data_auth::KnowledgeFactorHashInfo& hash_info_proto =
        proto.password_metadata().hash_info();
    DCHECK_EQ(hash_info_proto.algorithm(),
              KnowledgeFactorHashAlgorithm::HASH_TYPE_SHA256_TOP_HALF);
    return hash_info_proto.should_generate_key_store()
               ? PasswordMetadata::CreateForLocalPassword(
                     SystemSalt(hash_info_proto.salt()))
               : PasswordMetadata::CreateForOnlinePassword(
                     SystemSalt(hash_info_proto.salt()));
  }
  return PasswordMetadata::CreateWithoutSalt();
}

PinMetadata ParsePinMetadata(const user_data_auth::AuthFactor& proto) {
  std::optional<KnowledgeFactorHashInfo> hash_info;
  if (proto.has_pin_metadata() && proto.pin_metadata().has_hash_info()) {
    const user_data_auth::KnowledgeFactorHashInfo& hash_info_proto =
        proto.pin_metadata().hash_info();
    DCHECK_EQ(hash_info_proto.algorithm(),
              KnowledgeFactorHashAlgorithm::HASH_TYPE_PBKDF2_AES256_1234);
    DCHECK(hash_info_proto.should_generate_key_store());
    return PinMetadata::Create(PinSalt(hash_info_proto.salt()));
  }
  return PinMetadata::CreateWithoutSalt();
}

}  // namespace

user_data_auth::AuthFactorType ConvertFactorTypeToProto(AuthFactorType type) {
  switch (type) {
    case AuthFactorType::kUnknownLegacy:
      NOTREACHED_IN_MIGRATION()
          << "Unknown factor type should never be sent to cryptohome";
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
    case AuthFactorType::kFingerprint:
      return user_data_auth::AUTH_FACTOR_TYPE_FINGERPRINT;
  }
}

std::optional<AuthFactorType> SafeConvertFactorTypeFromProto(
    user_data_auth::AuthFactorType type) {
  switch (type) {
    case user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED:
      LOG(WARNING) << "Unknown factor type should be handled separately";
      return std::nullopt;
    case user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT:
      LOG(WARNING) << "Fingerprint factor type should never be returned";
      return std::nullopt;
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
    case user_data_auth::AUTH_FACTOR_TYPE_FINGERPRINT:
      return AuthFactorType::kFingerprint;
    default:
      LOG(WARNING)
          << "Unknown auth factor type " << static_cast<int>(type)
          << " Probably factor was added in cryptohome, but is not supported "
             "in chrome yet.";
      return std::nullopt;
  }
}

AuthFactorType ConvertFactorTypeFromProto(user_data_auth::AuthFactorType type) {
  switch (type) {
    case user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED:
      LOG(FATAL) << "Unknown factor type should be handled separately";
    case user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT:
      LOG(FATAL) << "Fingerprint factor type should never be returned";
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
    case user_data_auth::AUTH_FACTOR_TYPE_FINGERPRINT:
      return AuthFactorType::kFingerprint;
    default:
      // Use `--ignore-unknown-auth-factors` to avoid this.
      LOG(FATAL) << "Unknown auth factor type " << static_cast<int>(type);
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
  NOTREACHED_IN_MIGRATION();
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
    case AuthFactorType::kPassword: {
      user_data_auth::PasswordMetadata& password_metadata_proto =
          *out_proto->mutable_password_metadata();
      if (factor.GetPasswordMetadata().hash_info().has_value()) {
        ConvertKnowledgeFactorHashInfoToProto(
            *factor.GetPasswordMetadata().hash_info(),
            *password_metadata_proto.mutable_hash_info());
      }
      break;
    }
    case AuthFactorType::kPin: {
      if (ash::features::IsAllowPinTimeoutSetupEnabled()) {
        out_proto->mutable_common_metadata()->set_lockout_policy(
            user_data_auth::LOCKOUT_POLICY_TIME_LIMITED);
      } else {
        out_proto->mutable_common_metadata()->set_lockout_policy(
            user_data_auth::LOCKOUT_POLICY_ATTEMPT_LIMITED);
      }
      user_data_auth::PinMetadata& pin_metadata_proto =
          *out_proto->mutable_pin_metadata();
      if (factor.GetPinMetadata().hash_info().has_value()) {
        ConvertKnowledgeFactorHashInfoToProto(
            *factor.GetPinMetadata().hash_info(),
            *pin_metadata_proto.mutable_hash_info());
      }
      break;
    }
    case AuthFactorType::kRecovery:
      out_proto->mutable_cryptohome_recovery_metadata()->set_mediator_pub_key(
          factor.GetCryptohomeRecoveryMetadata().mediator_pub_key);
      break;
    case AuthFactorType::kKiosk:
      out_proto->mutable_kiosk_metadata();
      break;
    case AuthFactorType::kSmartCard:
      out_proto->mutable_smart_card_metadata()->set_public_key_spki_der(
          factor.GetSmartCardMetadata().public_key_spki_der);
      break;
    case AuthFactorType::kFingerprint:
      out_proto->mutable_fingerprint_metadata();
      break;
    case AuthFactorType::kLegacyFingerprint:
      LOG(FATAL) << "Legacy fingerprint factor type should never be serialized";
    case AuthFactorType::kUnknownLegacy:
      LOG(FATAL) << "Unknown factor type should never be serialized";
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
        proto_input->set_mediator_pub_key(recovery_creation.pub_key);
        proto_input->set_user_gaia_id(recovery_creation.user_gaia_id);
        proto_input->set_device_user_id(recovery_creation.device_user_id);
        proto_input->set_ensure_fresh_recovery_id(
            recovery_creation.ensure_fresh_recovery_id);
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
    case AuthFactorType::kFingerprint:
      // Fingerprint does not use any information from the Ash side,
      // only the signal. Creating empty input for `oneof` to work.
      out_proto->mutable_fingerprint_input();
      break;
    case AuthFactorType::kUnknownLegacy:
      LOG(FATAL) << "Unknown factor type should never be serialized";
    default:
      NOTIMPLEMENTED() << "Auth factor "
                       << static_cast<int>(auth_input.GetType())
                       << " is not implemented in cryptohome yet.";
      break;
  }
}

AuthFactor DeserializeAuthFactor(
    const user_data_auth::AuthFactorWithStatus& proto,
    AuthFactorType fallback_type) {
  CHECK(proto.has_auth_factor());
  auto factor_proto = proto.auth_factor();
  AuthFactorType type;
  if (factor_proto.type() == user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED) {
    LOG(WARNING) << "Unspecified auth factor type found, treating it as a "
                 << static_cast<int>(fallback_type);
    type = fallback_type;
  } else {
    type = ConvertFactorTypeFromProto(factor_proto.type());
    // TODO(b/243808147): Remove this hack after fixing cryptohome to return
    // `AUTH_FACTOR_TYPE_UNSPECIFIED` for legacy kiosk keysets.
    if (fallback_type == cryptohome::AuthFactorType::kKiosk &&
        type != cryptohome::AuthFactorType::kKiosk) {
      LOG(WARNING) << "Fixup kiosk key type for " << factor_proto.label() << " "
                   << factor_proto.type();
      type = cryptohome::AuthFactorType::kKiosk;
    }
  }
  AuthFactorRef ref(type, KeyLabel{factor_proto.label()});
  ComponentVersion chrome_ver{kFallbackFactorVersion};
  ComponentVersion chromeos_ver{kFallbackFactorVersion};
  if (factor_proto.has_common_metadata()) {
    auto common_metadata_proto = factor_proto.common_metadata();
    if (!common_metadata_proto.chrome_version_last_updated().empty()) {
      chrome_ver =
          ComponentVersion(common_metadata_proto.chrome_version_last_updated());
    }
    if (!common_metadata_proto.chromeos_version_last_updated().empty()) {
      chromeos_ver = ComponentVersion(
          common_metadata_proto.chromeos_version_last_updated());
    }
  }
  AuthFactorCommonMetadata common_metadata{std::move(chrome_ver),
                                           std::move(chromeos_ver)};

  // Ignore is_active_for_login for now
  switch (type) {
    case AuthFactorType::kPassword: {
      auto password_metadata = ParsePasswordMetadata(factor_proto);
      return AuthFactor(std::move(ref), std::move(common_metadata),
                        std::move(password_metadata));
    }
    case AuthFactorType::kRecovery: {
      if (!factor_proto.has_cryptohome_recovery_metadata()) {
        return AuthFactor(std::move(ref), std::move(common_metadata));
      }
      CryptohomeRecoveryMetadata recovery_metadata;
      recovery_metadata.mediator_pub_key =
          factor_proto.cryptohome_recovery_metadata().mediator_pub_key();
      return AuthFactor(std::move(ref), std::move(common_metadata),
                        std::move(recovery_metadata));
    }
    case AuthFactorType::kKiosk:
      return AuthFactor(std::move(ref), std::move(common_metadata));
    case AuthFactorType::kPin: {
      DCHECK(factor_proto.has_pin_metadata());
      auto pin_metadata = ParsePinMetadata(factor_proto);
      PinStatus pin_status = proto.has_status_info()
                                 ? PasrePinFactorStatus(proto.status_info())
                                 : PinStatus();
      return AuthFactor(std::move(ref), std::move(common_metadata),
                        std::move(pin_metadata), std::move(pin_status));
    }
    case AuthFactorType::kSmartCard: {
      DCHECK(factor_proto.has_smart_card_metadata());
      SmartCardMetadata smart_card_metadata;
      smart_card_metadata.public_key_spki_der =
          factor_proto.smart_card_metadata().public_key_spki_der();
      return AuthFactor(std::move(ref), std::move(common_metadata),
                        std::move(smart_card_metadata));
    }
    case AuthFactorType::kLegacyFingerprint: {
      LOG(FATAL) << "Legacy fingerprint factor should never be returned"
                 << " by cryptohome.";
      __builtin_unreachable();
    }
    case AuthFactorType::kFingerprint: {
      DCHECK(factor_proto.has_fingerprint_metadata());
      FingerprintMetadata fingerprint_metadata;
      return AuthFactor(std::move(ref), std::move(common_metadata),
                        std::move(fingerprint_metadata));
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
