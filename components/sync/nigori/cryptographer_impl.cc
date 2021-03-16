// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cryptographer_impl.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"

namespace syncer {

// static
std::unique_ptr<CryptographerImpl> CryptographerImpl::CreateEmpty() {
  return base::WrapUnique(
      new CryptographerImpl(NigoriKeyBag::CreateEmpty(),
                            /*default_encryption_key_name=*/std::string()));
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
  std::string default_encryption_key_name = proto.default_key_name();
  if (!default_encryption_key_name.empty() &&
      !key_bag.HasKey(default_encryption_key_name)) {
    // A default key name was specified but not present among keys.
    return nullptr;
  }

  return base::WrapUnique(new CryptographerImpl(
      std::move(key_bag), std::move(default_encryption_key_name)));
}

CryptographerImpl::CryptographerImpl(NigoriKeyBag key_bag,
                                     std::string default_encryption_key_name)
    : key_bag_(std::move(key_bag)),
      default_encryption_key_name_(std::move(default_encryption_key_name)) {
  DCHECK(default_encryption_key_name_.empty() ||
         key_bag_.HasKey(default_encryption_key_name_));
}

CryptographerImpl::~CryptographerImpl() = default;

sync_pb::CryptographerData CryptographerImpl::ToProto() const {
  sync_pb::CryptographerData proto;
  *proto.mutable_key_bag() = key_bag_.ToProto();
  proto.set_default_key_name(default_encryption_key_name_);
  return proto;
}

std::string CryptographerImpl::EmplaceKey(
    const std::string& passphrase,
    const KeyDerivationParams& derivation_params) {
  return key_bag_.AddKey(
      Nigori::CreateByDerivation(derivation_params, passphrase));
}

void CryptographerImpl::EmplaceKeysFrom(const NigoriKeyBag& key_bag) {
  key_bag_.AddAllUnknownKeysFrom(key_bag);
}

void CryptographerImpl::SelectDefaultEncryptionKey(
    const std::string& key_name) {
  DCHECK(!key_name.empty());
  DCHECK(key_bag_.HasKey(key_name));
  default_encryption_key_name_ = key_name;
}

void CryptographerImpl::ClearDefaultEncryptionKey() {
  default_encryption_key_name_.clear();
}

bool CryptographerImpl::HasKey(const std::string& key_name) const {
  return key_bag_.HasKey(key_name);
}

sync_pb::NigoriKey CryptographerImpl::ExportDefaultKey() const {
  DCHECK(CanEncrypt());
  return key_bag_.ExportKey(default_encryption_key_name_);
}

std::unique_ptr<CryptographerImpl> CryptographerImpl::CloneImpl() const {
  return base::WrapUnique(
      new CryptographerImpl(key_bag_.Clone(), default_encryption_key_name_));
}

size_t CryptographerImpl::KeyBagSizeForTesting() const {
  return key_bag_.size();
}

std::unique_ptr<Cryptographer> CryptographerImpl::Clone() const {
  return CloneImpl();
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

  return key_bag_.EncryptWithKey(default_encryption_key_name_, decrypted,
                                 encrypted);
}

bool CryptographerImpl::DecryptToString(const sync_pb::EncryptedData& encrypted,
                                        std::string* decrypted) const {
  DCHECK(decrypted);
  return key_bag_.Decrypt(encrypted, decrypted);
}

}  // namespace syncer
