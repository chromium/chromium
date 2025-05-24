// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"

#include <memory>
#include <optional>

#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/recoverable_key_store.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptohome {
namespace {

class AuthFactorConversionsTest : public testing::Test {
 protected:
  AuthFactorConversionsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Makes sure that `SafeConvertFactorTypeFromProto` and
// `ConvertFactorTypeFromProto` return corresponding values.
TEST_F(AuthFactorConversionsTest, FactorTypeProtoToChrome) {
  for (user_data_auth::AuthFactorType type = user_data_auth::AuthFactorType_MIN;
       type <= user_data_auth::AuthFactorType_MAX;
       type = static_cast<user_data_auth::AuthFactorType>(type + 1)) {
    std::optional<AuthFactorType> result = SafeConvertFactorTypeFromProto(type);
    SCOPED_TRACE("For user_data_auth::AuthFactorType " +
                 base::NumberToString(type));
    if (result.has_value()) {
      EXPECT_EQ(result.value(), ConvertFactorTypeFromProto(type));
    } else {
      EXPECT_DEATH(ConvertFactorTypeFromProto(type), "FATAL");
    }
  }
}

// Makes sure that `SerializeAuthFactor` and `DeserializeAuthFactor` convert
// factor-specific metadata correctly.
TEST_F(AuthFactorConversionsTest, MetadataConversion) {
  constexpr char kHashInfoSalt[] = "fake_salt";
  {
    // Test password metadata conversion.
    const AuthFactor password(
        AuthFactorRef(AuthFactorType::kPassword, KeyLabel("password")),
        AuthFactorCommonMetadata(),
        PasswordMetadata::CreateForLocalPassword(SystemSalt(kHashInfoSalt)));
    user_data_auth::AuthFactor password_proto;
    SerializeAuthFactor(password, &password_proto);
    ASSERT_TRUE(password_proto.has_password_metadata());
    ASSERT_TRUE(password_proto.password_metadata().has_hash_info());
    user_data_auth::KnowledgeFactorHashInfo password_hash_info_proto =
        password_proto.password_metadata().hash_info();
    EXPECT_EQ(password_hash_info_proto.algorithm(),
              KnowledgeFactorHashAlgorithm::HASH_TYPE_SHA256_TOP_HALF);
    EXPECT_EQ(password_hash_info_proto.salt(), kHashInfoSalt);
    EXPECT_TRUE(password_hash_info_proto.should_generate_key_store());

    user_data_auth::AuthFactorWithStatus factor_with_status;
    *factor_with_status.mutable_auth_factor() = password_proto;

    const AuthFactor reconstructed_password =
        DeserializeAuthFactor(factor_with_status,
                              /*fallback_type=*/AuthFactorType::kPassword);
    const std::optional<KnowledgeFactorHashInfo>& password_hash_info =
        reconstructed_password.GetPasswordMetadata().hash_info();
    ASSERT_TRUE(password_hash_info.has_value());
    EXPECT_EQ(password_hash_info->algorithm,
              KnowledgeFactorHashAlgorithmWrapper::kSha256TopHalf);
    EXPECT_EQ(password_hash_info->salt, kHashInfoSalt);
    EXPECT_TRUE(password_hash_info->should_generate_key_store);
  }

  {
    // Test password metadata conversion without generating key store.
    const AuthFactor password(
        AuthFactorRef(AuthFactorType::kPassword, KeyLabel("password2")),
        AuthFactorCommonMetadata(),
        PasswordMetadata::CreateForOnlinePassword(SystemSalt(kHashInfoSalt)));
    user_data_auth::AuthFactor password_proto;
    SerializeAuthFactor(password, &password_proto);
    ASSERT_TRUE(password_proto.has_password_metadata());
    ASSERT_TRUE(password_proto.password_metadata().has_hash_info());
    user_data_auth::KnowledgeFactorHashInfo password_hash_info_proto =
        password_proto.password_metadata().hash_info();
    EXPECT_EQ(password_hash_info_proto.algorithm(),
              KnowledgeFactorHashAlgorithm::HASH_TYPE_SHA256_TOP_HALF);
    EXPECT_EQ(password_hash_info_proto.salt(), kHashInfoSalt);
    EXPECT_FALSE(password_hash_info_proto.should_generate_key_store());

    user_data_auth::AuthFactorWithStatus factor_with_status;
    *factor_with_status.mutable_auth_factor() = password_proto;

    const AuthFactor reconstructed_password =
        DeserializeAuthFactor(factor_with_status,
                              /*fallback_type=*/AuthFactorType::kPassword);
    const std::optional<KnowledgeFactorHashInfo>& password_hash_info =
        reconstructed_password.GetPasswordMetadata().hash_info();
    ASSERT_TRUE(password_hash_info.has_value());
    EXPECT_EQ(password_hash_info->algorithm,
              KnowledgeFactorHashAlgorithmWrapper::kSha256TopHalf);
    EXPECT_EQ(password_hash_info->salt, kHashInfoSalt);
    EXPECT_FALSE(password_hash_info->should_generate_key_store);
  }

  {
    // Test password metadata conversion without salt.
    const AuthFactor password(
        AuthFactorRef(AuthFactorType::kPassword, KeyLabel("password")),
        AuthFactorCommonMetadata(), PasswordMetadata::CreateWithoutSalt());
    user_data_auth::AuthFactor password_proto;
    SerializeAuthFactor(password, &password_proto);
    ASSERT_TRUE(password_proto.has_password_metadata());
    EXPECT_FALSE(password_proto.password_metadata().has_hash_info());

    user_data_auth::AuthFactorWithStatus factor_with_status;
    *factor_with_status.mutable_auth_factor() = password_proto;

    const AuthFactor reconstructed_password =
        DeserializeAuthFactor(factor_with_status,
                              /*fallback_type=*/AuthFactorType::kPassword);
    EXPECT_EQ(reconstructed_password.GetPasswordMetadata().hash_info(),
              std::nullopt);
  }
  {
    // Test PIN metadata conversion.
    const AuthFactor pin(AuthFactorRef(AuthFactorType::kPin, KeyLabel("pin")),
                         AuthFactorCommonMetadata(),
                         PinMetadata::Create(PinSalt(kHashInfoSalt)));
    user_data_auth::AuthFactor pin_proto;
    SerializeAuthFactor(pin, &pin_proto);
    ASSERT_TRUE(pin_proto.has_pin_metadata());
    ASSERT_TRUE(pin_proto.pin_metadata().has_hash_info());
    user_data_auth::KnowledgeFactorHashInfo pin_hash_info_proto =
        pin_proto.pin_metadata().hash_info();
    EXPECT_EQ(pin_hash_info_proto.algorithm(),
              KnowledgeFactorHashAlgorithm::HASH_TYPE_PBKDF2_AES256_1234);
    EXPECT_EQ(pin_hash_info_proto.salt(), kHashInfoSalt);

    user_data_auth::AuthFactorWithStatus factor_with_status;
    *factor_with_status.mutable_auth_factor() = pin_proto;

    const AuthFactor reconstructed_pin =
        DeserializeAuthFactor(factor_with_status,
                              /*fallback_type=*/AuthFactorType::kPin);
    const std::optional<KnowledgeFactorHashInfo>& pin_hash_info =
        reconstructed_pin.GetPinMetadata().hash_info();
    ASSERT_TRUE(pin_hash_info.has_value());
    EXPECT_EQ(pin_hash_info->algorithm,
              KnowledgeFactorHashAlgorithmWrapper::kPbkdf2Aes2561234);
    EXPECT_EQ(pin_hash_info->salt, kHashInfoSalt);
  }

  {
    // Test recovery metadata conversion.
    cryptohome::AuthFactor recovery(
        AuthFactorRef(cryptohome::AuthFactorType::kRecovery,
                      KeyLabel{"fake-recovery-label"}),
        AuthFactorCommonMetadata(),
        CryptohomeRecoveryMetadata{"hsm-public-key"});
    user_data_auth::AuthFactor recovery_proto;
    cryptohome::SerializeAuthFactor(recovery, &recovery_proto);

    user_data_auth::AuthFactorWithStatus factor_with_status;
    *factor_with_status.mutable_auth_factor() = recovery_proto;

    auto deserialized_recovery = cryptohome::DeserializeAuthFactor(
        factor_with_status,
        /*fallback_type=*/cryptohome::AuthFactorType::kPassword);
    EXPECT_EQ(deserialized_recovery.ref(), recovery.ref());
    EXPECT_EQ(
        deserialized_recovery.GetCryptohomeRecoveryMetadata().mediator_pub_key,
        recovery.GetCryptohomeRecoveryMetadata().mediator_pub_key);
  }
}

// Makes sure that various status bits are handled correctly.
TEST_F(AuthFactorConversionsTest, PinFactorStatusConversion) {
  constexpr char kHashInfoSalt[] = "fake_salt";
  const base::TimeDelta in_a_while = base::Seconds(10);

  // PinFactor with no StatusInfo field.
  user_data_auth::AuthFactorWithStatus factor_with_status;
  auto* factor_proto = factor_with_status.mutable_auth_factor();
  factor_proto->set_type(user_data_auth::AUTH_FACTOR_TYPE_PIN);
  factor_proto->set_label("pin");
  factor_proto->mutable_common_metadata();
  factor_proto->mutable_pin_metadata()->mutable_hash_info()->set_algorithm(
      KnowledgeFactorHashAlgorithm::HASH_TYPE_PBKDF2_AES256_1234);
  factor_proto->mutable_pin_metadata()->mutable_hash_info()->set_salt(
      kHashInfoSalt);
  factor_proto->mutable_pin_metadata()
      ->mutable_hash_info()
      ->set_should_generate_key_store(true);

  // By default, PinFactor is available.
  {
    AuthFactor parsed =
        DeserializeAuthFactor(factor_with_status,
                              /*fallback_type=*/AuthFactorType::kPin);
    EXPECT_FALSE(parsed.GetPinStatus().IsLockedFactor());
    EXPECT_EQ(base::Time::Now(), parsed.GetPinStatus().AvailableAt());
  }

  // PinFactor indefinitely locked.
  {
    factor_with_status.clear_status_info();
    auto* status_info = factor_with_status.mutable_status_info();
    status_info->set_time_available_in(std::numeric_limits<uint64_t>::max());

    AuthFactor parsed =
        DeserializeAuthFactor(factor_with_status,
                              /*fallback_type=*/AuthFactorType::kPin);
    EXPECT_TRUE(parsed.GetPinStatus().IsLockedFactor());
    EXPECT_EQ(base::Time::Max(), parsed.GetPinStatus().AvailableAt());
  }

  // PinFactor temporary locked with a timeout.
  {
    factor_with_status.clear_status_info();
    auto* status_info = factor_with_status.mutable_status_info();
    status_info->set_time_available_in(in_a_while.InMilliseconds());

    AuthFactor parsed =
        DeserializeAuthFactor(factor_with_status,
                              /*fallback_type=*/AuthFactorType::kPin);
    EXPECT_TRUE(parsed.GetPinStatus().IsLockedFactor());
    EXPECT_EQ(base::Time::Now() + in_a_while,
              parsed.GetPinStatus().AvailableAt());
  }
}

}  // namespace
}  // namespace cryptohome
