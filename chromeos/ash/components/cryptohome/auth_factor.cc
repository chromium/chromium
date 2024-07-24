// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor.h"

#include <utility>

#include "base/check_op.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "components/version_info/version_info.h"

namespace cryptohome {

const char kFallbackFactorVersion[] = "0.0.0.0";

// =============== `AuthFactorRef` ===============
AuthFactorRef::AuthFactorRef(AuthFactorType type, KeyLabel label)
    : type_(type), label_(std::move(label)) {
  DCHECK(!label_->empty());
}
AuthFactorRef::AuthFactorRef(AuthFactorRef&&) = default;
AuthFactorRef& AuthFactorRef::operator=(AuthFactorRef&&) = default;
AuthFactorRef::AuthFactorRef(const AuthFactorRef&) = default;
AuthFactorRef& AuthFactorRef::operator=(const AuthFactorRef&) = default;
AuthFactorRef::~AuthFactorRef() = default;

bool AuthFactorRef::operator==(const AuthFactorRef& other) const {
  const bool equal = label_.value() == other.label_.value();
  CHECK(!equal || type_ == other.type_);
  return equal;
}

// =============== `AuthFactorCommonMetadata` ===============
AuthFactorCommonMetadata::AuthFactorCommonMetadata()
    : chrome_version_last_updated_(
          ComponentVersion(std::string(version_info::GetVersionNumber()))) {}

AuthFactorCommonMetadata::AuthFactorCommonMetadata(ComponentVersion chrome,
                                                   ComponentVersion chromeos)
    : chrome_version_last_updated_(std::move(chrome)),
      chromeos_version_last_updated_(std::move(chromeos)) {}

AuthFactorCommonMetadata::AuthFactorCommonMetadata(
    AuthFactorCommonMetadata&&) noexcept = default;
AuthFactorCommonMetadata& AuthFactorCommonMetadata::operator=(
    AuthFactorCommonMetadata&&) noexcept = default;
AuthFactorCommonMetadata::AuthFactorCommonMetadata(
    const AuthFactorCommonMetadata&) = default;
AuthFactorCommonMetadata& AuthFactorCommonMetadata::operator=(
    const AuthFactorCommonMetadata&) = default;
AuthFactorCommonMetadata::~AuthFactorCommonMetadata() = default;

bool AuthFactorCommonMetadata::operator==(
    const AuthFactorCommonMetadata& other) const {
  return (this->chrome_version_last_updated_ ==
          other.chrome_version_last_updated_) &&
         (this->chromeos_version_last_updated_ ==
          other.chromeos_version_last_updated_);
}

// =============== `Factor-specific Status` ===============
PinStatus::PinStatus() : available_at_(base::Time::Now()) {}

PinStatus::PinStatus(base::TimeDelta available_in)
    : available_at_(base::Time::Now() + available_in) {}

PinStatus::PinStatus(PinStatus&&) noexcept = default;
PinStatus& PinStatus::operator=(PinStatus&&) noexcept = default;
PinStatus::PinStatus(const PinStatus&) = default;
PinStatus& PinStatus::operator=(const PinStatus&) = default;
PinStatus::~PinStatus() = default;

bool PinStatus::IsLockedFactor() const {
  return base::Time::Now() < available_at_;
}

base::Time PinStatus::AvailableAt() const {
  return available_at_;
}

// =============== `Factor-specific Metadata` ===============

PasswordMetadata PasswordMetadata::CreateWithoutSalt() {
  return PasswordMetadata(std::nullopt);
}

PasswordMetadata PasswordMetadata::CreateForOnlinePassword(SystemSalt salt) {
  return PasswordMetadata(KnowledgeFactorHashInfo{
      .algorithm = KnowledgeFactorHashAlgorithmWrapper::kSha256TopHalf,
      .salt = std::move(*salt),
      .should_generate_key_store = false,
  });
}

PasswordMetadata PasswordMetadata::CreateForLocalPassword(SystemSalt salt) {
  return PasswordMetadata(KnowledgeFactorHashInfo{
      .algorithm = KnowledgeFactorHashAlgorithmWrapper::kSha256TopHalf,
      .salt = std::move(*salt),
      .should_generate_key_store = true,
  });
}

PasswordMetadata::PasswordMetadata(
    std::optional<KnowledgeFactorHashInfo> hash_info)
    : hash_info_(std::move(hash_info)) {}
PasswordMetadata::PasswordMetadata(PasswordMetadata&&) noexcept = default;
PasswordMetadata& PasswordMetadata::operator=(PasswordMetadata&&) noexcept =
    default;
PasswordMetadata::PasswordMetadata(const PasswordMetadata&) = default;
PasswordMetadata& PasswordMetadata::operator=(const PasswordMetadata&) =
    default;
PasswordMetadata::~PasswordMetadata() = default;

PinMetadata PinMetadata::CreateWithoutSalt() {
  return PinMetadata(std::nullopt);
}

PinMetadata PinMetadata::Create(PinSalt salt) {
  return PinMetadata(KnowledgeFactorHashInfo{
      .algorithm = KnowledgeFactorHashAlgorithmWrapper::kPbkdf2Aes2561234,
      .salt = std::move(*salt),
      .should_generate_key_store = true,
  });
}

PinMetadata::PinMetadata(std::optional<KnowledgeFactorHashInfo> hash_info)
    : hash_info_(std::move(hash_info)) {}
PinMetadata::PinMetadata(PinMetadata&&) noexcept = default;
PinMetadata& PinMetadata::operator=(PinMetadata&&) noexcept = default;
PinMetadata::PinMetadata(const PinMetadata&) = default;
PinMetadata& PinMetadata::operator=(const PinMetadata&) = default;
PinMetadata::~PinMetadata() = default;

// =============== `AuthFactor` ===============

AuthFactor::AuthFactor(AuthFactorRef ref, AuthFactorCommonMetadata metadata)
    : ref_(std::move(ref)), common_metadata_(std::move(metadata)) {}

AuthFactor::AuthFactor(AuthFactorRef ref,
                       AuthFactorCommonMetadata metadata,
                       PinMetadata pin_metadata,
                       PinStatus status)
    : ref_(std::move(ref)),
      common_metadata_(std::move(metadata)),
      factor_metadata_(std::move(pin_metadata)),
      factor_status_(std::move(status)) {
  CHECK_EQ(ref_.type(), AuthFactorType::kPin);
}

AuthFactor::AuthFactor(AuthFactorRef ref,
                       AuthFactorCommonMetadata metadata,
                       SmartCardMetadata factor_metadata)
    : ref_(std::move(ref)),
      common_metadata_(std::move(metadata)),
      factor_metadata_(std::move(factor_metadata)) {
  CHECK_EQ(ref_.type(), AuthFactorType::kSmartCard);
}

AuthFactor::AuthFactor(AuthFactorRef ref,
                       AuthFactorCommonMetadata metadata,
                       CryptohomeRecoveryMetadata factor_metadata)
    : ref_(std::move(ref)),
      common_metadata_(std::move(metadata)),
      factor_metadata_(std::move(factor_metadata)) {
  CHECK_EQ(ref_.type(), AuthFactorType::kRecovery);
}

AuthFactor::AuthFactor(AuthFactorRef ref,
                       AuthFactorCommonMetadata metadata,
                       PasswordMetadata factor_metadata)
    : ref_(std::move(ref)),
      common_metadata_(std::move(metadata)),
      factor_metadata_(std::move(factor_metadata)) {
  CHECK_EQ(ref_.type(), AuthFactorType::kPassword);
}

AuthFactor::AuthFactor(AuthFactorRef ref,
                       AuthFactorCommonMetadata metadata,
                       PinMetadata factor_metadata)
    : ref_(std::move(ref)),
      common_metadata_(std::move(metadata)),
      factor_metadata_(std::move(factor_metadata)) {
  CHECK_EQ(ref_.type(), AuthFactorType::kPin);
}

AuthFactor::AuthFactor(AuthFactorRef ref,
                       AuthFactorCommonMetadata metadata,
                       FingerprintMetadata fingerprint_metadata)
    : ref_(std::move(ref)),
      common_metadata_(std::move(metadata)),
      factor_metadata_(std::move(fingerprint_metadata)) {
  CHECK_EQ(ref_.type(), AuthFactorType::kFingerprint);
}

AuthFactor::AuthFactor(AuthFactor&&) noexcept = default;
AuthFactor& AuthFactor::operator=(AuthFactor&&) noexcept = default;
AuthFactor::AuthFactor(const AuthFactor&) = default;
AuthFactor& AuthFactor::operator=(const AuthFactor&) = default;

AuthFactor::~AuthFactor() = default;

bool AuthFactor::operator==(const AuthFactor& other) const {
  return ref_ == other.ref_ && common_metadata_ == other.common_metadata_;
}

const AuthFactorRef& AuthFactor::ref() const {
  return ref_;
}

const AuthFactorCommonMetadata& AuthFactor::GetCommonMetadata() const {
  return common_metadata_;
}

const PinStatus& AuthFactor::GetPinStatus() const {
  return absl::get<PinStatus>(factor_status_);
}

const SmartCardMetadata& AuthFactor::GetSmartCardMetadata() const {
  return absl::get<SmartCardMetadata>(factor_metadata_);
}

const CryptohomeRecoveryMetadata& AuthFactor::GetCryptohomeRecoveryMetadata()
    const {
  return absl::get<CryptohomeRecoveryMetadata>(factor_metadata_);
}

const PasswordMetadata& AuthFactor::GetPasswordMetadata() const {
  return absl::get<PasswordMetadata>(factor_metadata_);
}

const PinMetadata& AuthFactor::GetPinMetadata() const {
  return absl::get<PinMetadata>(factor_metadata_);
}

const FingerprintMetadata& AuthFactor::GetFingerprintMetadata() const {
  return absl::get<FingerprintMetadata>(factor_metadata_);
}

}  // namespace cryptohome
