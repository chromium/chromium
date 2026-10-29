// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cryptographer_impl.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/sync/nigori/cross_user_sharing_keys.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

// static
std::unique_ptr<CryptographerImpl> CryptographerImpl::CreateEmpty() {
  return base::WrapUnique(new CryptographerImpl(
      NigoriKeyBag::CreateEmpty(),
      /*primary_encryption_key_name=*/std::string(),
      /*primary_encryption_key_invalidated=*/false,
      CrossUserSharingKeys::CreateEmpty(),
      /*primary_cross_user_sharing_key_version=*/std::nullopt));
}

// static
std::unique_ptr<CryptographerImpl> CryptographerImpl::FromSingleKeyForTesting(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  std::unique_ptr<CryptographerImpl> cryptographer = CreateEmpty();
  std::string key_name = cryptographer->RotatePrimaryKeyByDerivation(
      passphrase, derivation_params);
  CHECK(!key_name.empty());
  return cryptographer;
}

// static
std::unique_ptr<CryptographerImpl> CryptographerImpl::FromLocalProto(
    const sync_pb::CryptographerData& proto,
    bool primary_encryption_key_invalidated) {
  if (!IsLocalProtoValid(proto)) {
    return nullptr;
  }

  NigoriKeyBag key_bag = NigoriKeyBag::CreateFromProto(proto.key_bag());
  CrossUserSharingKeys cross_user_sharing_keys =
      CrossUserSharingKeys::CreateFromProto(proto.cross_user_sharing_keys());

  return base::WrapUnique(new CryptographerImpl(
      std::move(key_bag), proto.primary_key_name(),
      primary_encryption_key_invalidated, std::move(cross_user_sharing_keys),
      /*primary_cross_user_sharing_key_version=*/std::nullopt));
}

// static
bool CryptographerImpl::IsLocalProtoValid(
    const sync_pb::CryptographerData& proto) {
  if (proto.primary_key_name().empty()) {
    return proto.key_bag().key_size() == 0;
  }
  return NigoriKeyBag::CreateFromProto(proto.key_bag())
      .HasKey(proto.primary_key_name());
}

CryptographerImpl::CryptographerImpl(
    NigoriKeyBag key_bag,
    std::string primary_encryption_key_name,
    bool primary_encryption_key_invalidated,
    CrossUserSharingKeys cross_user_sharing_keys,
    std::optional<uint32_t> primary_cross_user_sharing_key_version)
    : key_bag_(std::move(key_bag)),
      primary_encryption_key_name_(std::move(primary_encryption_key_name)),
      primary_encryption_key_invalidated_(primary_encryption_key_invalidated),
      primary_cross_user_sharing_key_version_(
          primary_cross_user_sharing_key_version),
      cross_user_sharing_keys_(std::move(cross_user_sharing_keys)) {
  if (primary_encryption_key_name_.empty()) {
    CHECK_EQ(key_bag_.size(), 0u);
  } else {
    CHECK(key_bag_.HasKey(primary_encryption_key_name_));
  }
}

CryptographerImpl::~CryptographerImpl() = default;

sync_pb::CryptographerData CryptographerImpl::ToLocalProto() const {
  sync_pb::CryptographerData proto;
  *proto.mutable_key_bag() = key_bag_.ToProto();
  proto.set_primary_key_name(primary_encryption_key_name_);
  *proto.mutable_cross_user_sharing_keys() = cross_user_sharing_keys_.ToProto();
  return proto;
}

sync_pb::EncryptedData CryptographerImpl::ExportEncryptedKeyBag() const {
  CHECK(CanEncrypt());

  sync_pb::EncryptionKeys keys_for_encryption;
  *keys_for_encryption.mutable_cross_user_sharing_private_key() =
      cross_user_sharing_keys_.ToProto().private_key();

  *keys_for_encryption.mutable_key() = key_bag_.ToProto().key();

  sync_pb::EncryptedData encrypted;
  const bool success = Encrypt(keys_for_encryption, &encrypted);
  CHECK(success);
  return encrypted;
}

std::string CryptographerImpl::RotatePrimaryKeyByDerivation(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  std::string key_name = key_bag_.AddKey(derivation_params, passphrase);
  CHECK(!key_name.empty());
  SelectPrimaryEncryptionKey(key_name);
  return key_name;
}

void CryptographerImpl::SetCrossUserSharingKeyPair(
    CrossUserSharingPublicPrivateKeyPair private_key,
    uint32_t version) {
  cross_user_sharing_keys_.SetKeyPair(std::move(private_key), version);
}

void CryptographerImpl::EmplaceKeysFrom(const NigoriKeyBag& key_bag) {
  key_bag_.AddAllUnknownKeysFrom(key_bag);
}

bool CryptographerImpl::TryFollowRemoteKeyRotation(
    const NigoriKeyBag& new_key_bag,
    const std::string& new_primary_key_name,
    bool enforce_key_continuity) {
  CHECK(!new_primary_key_name.empty());
  CHECK(new_key_bag.HasKey(new_primary_key_name));
  if (enforce_key_continuity && !primary_encryption_key_name_.empty() &&
      !new_key_bag.HasKey(primary_encryption_key_name_)) {
    return false;
  }
  EmplaceKeysFrom(new_key_bag);
  SelectPrimaryEncryptionKey(new_primary_key_name);
  return true;
}

void CryptographerImpl::ReplaceCrossUserSharingKeys(CrossUserSharingKeys keys) {
  cross_user_sharing_keys_ = std::move(keys);
}

void CryptographerImpl::SelectPrimaryEncryptionKey(
    const std::string& key_name) {
  DCHECK(!key_name.empty());
  DCHECK(key_bag_.HasKey(key_name));
  primary_encryption_key_name_ = key_name;
  primary_encryption_key_invalidated_ = false;
}

void CryptographerImpl::InvalidatePrimaryEncryptionKey() {
  primary_encryption_key_invalidated_ = true;
}

void CryptographerImpl::ClearAllKeys() {
  primary_encryption_key_name_.clear();
  primary_encryption_key_invalidated_ = false;
  key_bag_ = NigoriKeyBag::CreateEmpty();
  primary_cross_user_sharing_key_version_ = std::nullopt;
  cross_user_sharing_keys_ = CrossUserSharingKeys::CreateEmpty();
}

bool CryptographerImpl::HasKey(const std::string& key_name) const {
  return key_bag_.HasKey(key_name);
}

bool CryptographerImpl::HasCrossUserSharingKeyPair(
    uint32_t key_pair_version) const {
  return cross_user_sharing_keys_.HasKeyPair(key_pair_version);
}

size_t CryptographerImpl::CrossUserSharingKeyPairSizeForMetrics() const {
  return cross_user_sharing_keys_.size();
}

const CrossUserSharingPublicPrivateKeyPair&
CryptographerImpl::GetCrossUserSharingKeyPair(uint32_t version) const {
  return cross_user_sharing_keys_.GetKeyPair(version);
}

sync_pb::NigoriKey CryptographerImpl::ExportPrimaryKey() const {
  DCHECK(CanEncrypt());
  return key_bag_.ExportKey(primary_encryption_key_name_);
}

std::unique_ptr<CryptographerImpl> CryptographerImpl::Clone() const {
  return base::WrapUnique(new CryptographerImpl(
      key_bag_.Clone(), primary_encryption_key_name_,
      primary_encryption_key_invalidated_, cross_user_sharing_keys_.Clone(),
      primary_cross_user_sharing_key_version_));
}

size_t CryptographerImpl::KeyBagSizeForTesting() const {
  return key_bag_.size();
}

bool CryptographerImpl::CanEncrypt() const {
  return !primary_encryption_key_invalidated_ &&
         !primary_encryption_key_name_.empty();
}

bool CryptographerImpl::CanDecrypt(
    const sync_pb::EncryptedData& encrypted) const {
  return key_bag_.HasKey(encrypted.key_name());
}

std::string CryptographerImpl::GetPrimaryEncryptionKeyName() const {
  if (primary_encryption_key_invalidated_) {
    return std::string();
  }
  return primary_encryption_key_name_;
}

bool CryptographerImpl::EncryptString(const std::string& decrypted,
                                      sync_pb::EncryptedData* encrypted) const {
  DCHECK(encrypted);
  encrypted->Clear();

  if (!CanEncrypt()) {
    return false;
  }

  *encrypted = key_bag_.EncryptWithKey(primary_encryption_key_name_, decrypted);
  return true;
}

bool CryptographerImpl::DecryptToString(const sync_pb::EncryptedData& encrypted,
                                        std::string* decrypted) const {
  DCHECK(decrypted);
  return key_bag_.Decrypt(encrypted, decrypted);
}

std::optional<std::vector<uint8_t>>
CryptographerImpl::AuthEncryptForCrossUserSharing(
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> recipient_public_key) const {
  if (!primary_cross_user_sharing_key_version_.has_value()) {
    VLOG(1) << "Primary encryption key pair version is not set";
    return std::nullopt;
  }
  if (!cross_user_sharing_keys_.HasKeyPair(
          primary_cross_user_sharing_key_version_.value())) {
    VLOG(1) << "Encryption key pair is not available";
    return std::nullopt;
  }

  const CrossUserSharingPublicPrivateKeyPair& encryption_key_pair =
      cross_user_sharing_keys_.GetKeyPair(
          primary_cross_user_sharing_key_version_.value());

  return encryption_key_pair.HpkeAuthEncrypt(plaintext, recipient_public_key,
                                             {});
}

std::optional<std::vector<uint8_t>>
CryptographerImpl::AuthDecryptForCrossUserSharing(
    base::span<const uint8_t> encrypted_data,
    base::span<const uint8_t> sender_public_key,
    const uint32_t recipient_key_version) const {
  if (!cross_user_sharing_keys_.HasKeyPair(recipient_key_version)) {
    VLOG(1) << "Decryption key pair does not exist: " << recipient_key_version;
    return std::nullopt;
  }

  const CrossUserSharingPublicPrivateKeyPair& decryption_key_pair =
      cross_user_sharing_keys_.GetKeyPair(recipient_key_version);

  return decryption_key_pair.HpkeAuthDecrypt(encrypted_data, sender_public_key,
                                             {});
}

void CryptographerImpl::SelectPrimaryCrossUserSharingKey(
    const uint32_t version) {
  primary_cross_user_sharing_key_version_ = version;
}

}  // namespace syncer
