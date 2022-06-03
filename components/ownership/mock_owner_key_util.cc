// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

MockOwnerKeyUtil::MockOwnerKeyUtil() {
}

MockOwnerKeyUtil::~MockOwnerKeyUtil() {
}

bool MockOwnerKeyUtil::ImportPublicKey(std::vector<uint8_t>* output) {
  *output = public_key_;
  return !public_key_.empty();
}

crypto::ScopedSECKEYPrivateKey MockOwnerKeyUtil::FindPrivateKeyInSlot(
    const std::vector<uint8_t>& key,
    PK11SlotInfo* slot) {
  if (!private_key_)
    return nullptr;
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

void MockOwnerKeyUtil::SetPrivateKey(
    std::unique_ptr<crypto::RSAPrivateKey> key) {
  crypto::EnsureNSSInit();

  CHECK(key->ExportPublicKey(&public_key_));

  std::vector<uint8_t> key_exported;
  CHECK(key->ExportPrivateKey(&key_exported));

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  CHECK(slot);
  private_key_ = crypto::ImportNSSKeyFromPrivateKeyInfo(
      slot.get(), key_exported, false /* not permanent */);
  CHECK(private_key_);
}

}  // namespace ownership
