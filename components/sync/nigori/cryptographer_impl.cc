// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cryptographer_impl.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/sync/nigori/cross_user_sharing_keys.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

// static
std::unique_ptr<CryptographerImpl> CryptographerImpl::CreateEmpty() {
  return base::WrapUnique(
      new CryptographerImpl(NigoriKeyBag::CreateEmpty(),
                            /*default_encryption_key_name=*/std::string(),
                            CrossUserSharingKeys::CreateEmpty()));
}

// static
std::unique_ptr<CryptographerImpl> CryptographerImpl::FromSingleKeyForTesting(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  std::unique_ptr<CryptographerImpl> cryptographer = CreateEmpty();
  std::string key_name =
      cryptographer->EmplaceKey(passphrase, derivation_params);
  cryptographer->SelectDefaultEncryptionKey(key_name);
  return cryptographer;
}

// static
std::unique_ptr<CryptographerImpl> CryptographerImpl::FromProto(
    const sync_pb::CryptographerData& proto) {
  NigoriKeyBag key_bag = NigoriKeyBag::CreateFromProto(proto.key_bag());
  // TODO(crbug.com/40141634): An invalid local state should be handled in the
  // caller instead of CHECK-ing here, e.g. by resetting the local state.
  CHECK(proto.default_key_name().empty() ||
        key_bag.HasKey(proto.default_key_name()));

  CrossUserSharingKeys cross_user_sharing_keys =
      CrossUserSharingKeys::CreateFromProto(proto.cross_user_sharing_keys());

  return base::WrapUnique(
      new CryptographerImpl(std::move(key_bag), proto.default_key_name(),
                            std::move(cross_user_sharing_keys)));
}

CryptographerImpl::CryptographerImpl(
    NigoriKeyBag key_bag,
    std::string default_encryption_key_name,
    CrossUserSharingKeys cross_user_sharing_keys)
    : key_bag_(std::move(key_bag)),
      default_encryption_key_name_(std::move(default_encryption_key_name)),
      cross_user_sharing_keys_(std::move(cross_user_sharing_keys)) {
  DCHECK(default_encryption_key_name_.empty() ||
         key_bag_.HasKey(default_encryption_key_name_));
}

CryptographerImpl::~CryptographerImpl() = default;

sync_pb::CryptographerData CryptographerImpl::ToProto() const {
  sync_pb::CryptographerData proto;
  *proto.mutable_key_bag() = key_bag_.ToProto();
  proto.set_default_key_name(default_encryption_key_name_);
  *proto.mutable_cross_user_sharing_keys() = cross_user_sharing_keys_.ToProto();
  return proto;
}

std::string CryptographerImpl::EmplaceKey(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  return key_bag_.AddKey(
      Nigori::CreateByDerivation(derivation_params, passphrase));
}

void CryptographerImpl::SetKeyPair(
    CrossUserSharingPublicPrivateKeyPair private_key,
    uint32_t version) {
  cross_user_sharing_keys_.SetKeyPair(std::move(private_key), version);
}

void CryptographerImpl::EmplaceKeysFrom(const NigoriKeyBag& key_bag) {
  key_bag_.AddAllUnknownKeysFrom(key_bag);
}

void CryptographerImpl::ReplaceCrossUserSharingKeys(CrossUserSharingKeys keys) {
  cross_user_sharing_keys_ = std::move(keys);
}

void CryptographerImpl::SelectDefaultEncryptionKey(
    const std::string& key_name) {
  DCHECK(!key_name.empty());
  DCHECK(key_bag_.HasKey(key_name));
  default_encryption_key_name_ = key_name;
}

void CryptographerImpl::EmplaceAllNigoriKeysFrom(
    const CryptographerImpl& other) {
  EmplaceKeysFrom(other.key_bag_);
}

void CryptographerImpl::ClearDefaultEncryptionKey() {
  default_encryption_key_name_.clear();
}

void CryptographerImpl::ClearAllKeys() {
  default_encryption_key_name_.clear();
  key_bag_ = NigoriKeyBag::CreateEmpty();
  default_cross_user_sharing_key_version_ = std::nullopt;
  cross_user_sharing_keys_ = CrossUserSharingKeys::CreateEmpty();
}

bool CryptographerImpl::HasKey(const std::string& key_name) const {
  return key_bag_.HasKey(key_name);
}

bool CryptographerImpl::HasKeyPair(uint32_t key_pair_version) const {
  return cross_user_sharing_keys_.HasKeyPair(key_pair_version);
}

size_t CryptographerImpl::KeyPairSizeForMetrics() const {
  return cross_user_sharing_keys_.size();
}

const CrossUserSharingPublicPrivateKeyPair&
CryptographerImpl::GetCrossUserSharingKeyPair(uint32_t version) const {
  return cross_user_sharing_keys_.GetKeyPair(version);
}

sync_pb::NigoriKey CryptographerImpl::ExportDefaultKey() const {
  DCHECK(CanEncrypt());
  return key_bag_.ExportKey(default_encryption_key_name_);
}

std::unique_ptr<CryptographerImpl> CryptographerImpl::Clone() const {
  return base::WrapUnique(
      new CryptographerImpl(key_bag_.Clone(), default_encryption_key_name_,
                            cross_user_sharing_keys_.Clone()));
}

size_t CryptographerImpl::KeyBagSizeForTesting() const {
  return key_bag_.size();
}

bool CryptographerImpl::CanEncrypt() const {
  return !default_encryption_key_name_.empty();
}

bool CryptographerImpl::CanDecrypt(
    const sync_pb::EncryptedData& encrypted) const {
  return key_bag_.HasKey(encrypted.key_name());
}

std::string CryptographerImpl::GetDefaultEncryptionKeyName() const {
  return default_encryption_key_name_;
}

bool CryptographerImpl::EncryptString(const std::string& decrypted,
                                      sync_pb::EncryptedData* encrypted) const {
  DCHECK(encrypted);
  encrypted->Clear();

  if (!CanEncrypt()) {
    return false;
  }

  *encrypted = key_bag_.EncryptWithKey(default_encryption_key_name_, decrypted);
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
  if (!default_cross_user_sharing_key_version_.has_value()) {
    VLOG(1) << "Default encryption key pair version is not set";
    return std::nullopt;
  }
  if (!cross_user_sharing_keys_.HasKeyPair(
          default_cross_user_sharing_key_version_.value())) {
    VLOG(1) << "Encryption key pair is not available";
    return std::nullopt;
  }

  const CrossUserSharingPublicPrivateKeyPair& encryption_key_pair =
      cross_user_sharing_keys_.GetKeyPair(
          default_cross_user_sharing_key_version_.value());

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

void CryptographerImpl::SelectDefaultCrossUserSharingKey(
    const uint32_t version) {
  default_cross_user_sharing_key_version_ = version;
}

}  // namespace syncer
