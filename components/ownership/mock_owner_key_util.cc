// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/mock_owner_key_util.h"

#include <pk11pub.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"

namespace ownership {

static const uint16_t kKeySizeInBits = 2048;

MockOwnerKeyUtil::MockOwnerKeyUtil() = default;

MockOwnerKeyUtil::~MockOwnerKeyUtil() = default;

scoped_refptr<PublicKey> MockOwnerKeyUtil::ImportPublicKey() {
  return public_key_.empty() ? nullptr
                             : base::MakeRefCounted<ownership::PublicKey>(
                                   /*is_persisted=*/true, /*data=*/public_key_);
}

crypto::ScopedSECKEYPrivateKey MockOwnerKeyUtil::GenerateKeyPair(
    PK11SlotInfo* slot) {
  if (generate_key_fail_times_ > 0) {
    --generate_key_fail_times_;
    return nullptr;
  }

  PK11RSAGenParams param;
  param.keySizeInBits = kKeySizeInBits;
  param.pe = 65537L;
  SECKEYPublicKey* public_key_ptr = nullptr;

  crypto::ScopedSECKEYPrivateKey key(PK11_GenerateKeyPair(
      slot, CKM_RSA_PKCS_KEY_PAIR_GEN, &param, &public_key_ptr,
      PR_TRUE /* permanent */, PR_TRUE /* sensitive */, nullptr));
  crypto::ScopedSECKEYPublicKey public_key(public_key_ptr);
  return key;
}

crypto::ScopedSECKEYPrivateKey MockOwnerKeyUtil::FindPrivateKeyInSlot(
    const std::vector<uint8_t>& key,
    PK11SlotInfo* slot) {
  if (!private_key_ || !slot) {
    return nullptr;
  }

  if (private_key_slot_id_.has_value() &&
      (private_key_slot_id_.value() != PK11_GetSlotID(slot))) {
    return nullptr;
  }

  return crypto::ScopedSECKEYPrivateKey(
      SECKEY_CopyPrivateKey(private_key_.get()));
}

bool MockOwnerKeyUtil::IsPublicKeyPresent() {
  return !public_key_.empty();
}

void MockOwnerKeyUtil::Clear() {
  public_key_.clear();
  private_key_.reset();
}

void MockOwnerKeyUtil::SetPublicKey(const std::vector<uint8_t>& key) {
  public_key_ = key;
}

void MockOwnerKeyUtil::SetPublicKeyFromPrivateKey(
    const crypto::RSAPrivateKey& key) {
  CHECK(key.ExportPublicKey(&public_key_));
}

void MockOwnerKeyUtil::ImportPrivateKeyAndSetPublicKey(
    std::unique_ptr<crypto::RSAPrivateKey> key) {
  crypto::EnsureNSSInit();

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  CHECK(slot);
  ImportPrivateKeyAndSetPublicKeyImpl(std::move(key), slot.get());
}

void MockOwnerKeyUtil::ImportPrivateKeyAndSetPublicKeyImpl(
    std::unique_ptr<crypto::RSAPrivateKey> key,
    PK11SlotInfo* slot) {
  CHECK(slot);
  crypto::EnsureNSSInit();

  CHECK(key->ExportPublicKey(&public_key_));

  std::vector<uint8_t> key_exported;
  CHECK(key->ExportPrivateKey(&key_exported));

  private_key_ = crypto::ImportNSSKeyFromPrivateKeyInfo(
      slot, key_exported, false /* not permanent */);
  CHECK(private_key_);
}

void MockOwnerKeyUtil::ImportPrivateKeyInSlotAndSetPublicKey(
    std::unique_ptr<crypto::RSAPrivateKey> key,
    PK11SlotInfo* slot) {
  private_key_slot_id_ = PK11_GetSlotID(slot);
  ImportPrivateKeyAndSetPublicKeyImpl(std::move(key), slot);
}

void MockOwnerKeyUtil::SimulateGenerateKeyFailure(int fail_times) {
  generate_key_fail_times_ = fail_times;
}

}  // namespace ownership
