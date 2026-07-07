// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/ref_counted_unexportable_key.h"

#include <memory>

#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

namespace {

class RefCountedUnexportableSigningKeyImpl
    : public RefCountedUnexportableSigningKey {
 public:
  explicit RefCountedUnexportableSigningKeyImpl(
      std::unique_ptr<crypto::UnexportableSigningKey> key)
      : key_(std::move(key)) {}

  crypto::UnexportableSigningKey& key() const override { return *key_; }
  const UnexportableSigningKeyId& id() const override { return id_; }

 private:
  ~RefCountedUnexportableSigningKeyImpl() override = default;

  const std::unique_ptr<crypto::UnexportableSigningKey> key_;
  const UnexportableSigningKeyId id_;
};

class RefCountedUnexportableAttestationKeyImpl
    : public RefCountedUnexportableAttestationKey {
 public:
  explicit RefCountedUnexportableAttestationKeyImpl(
      std::unique_ptr<crypto::UnexportableAttestationKey> key)
      : key_(std::move(key)) {}

  crypto::UnexportableAttestationKey& key() const override { return *key_; }
  const UnexportableAttestationKeyId& id() const override { return id_; }

 private:
  ~RefCountedUnexportableAttestationKeyImpl() override = default;

  const std::unique_ptr<crypto::UnexportableAttestationKey> key_;
  const UnexportableAttestationKeyId id_;
};

}  // namespace

scoped_refptr<RefCountedUnexportableSigningKey>
MakeRefCountedUnexportableSigningKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key) {
  CHECK(key);
  return base::MakeRefCounted<RefCountedUnexportableSigningKeyImpl>(
      std::move(key));
}

scoped_refptr<RefCountedUnexportableAttestationKey>
MakeRefCountedUnexportableAttestationKey(
    std::unique_ptr<crypto::UnexportableAttestationKey> key) {
  CHECK(key);
  return base::MakeRefCounted<RefCountedUnexportableAttestationKeyImpl>(
      std::move(key));
}

}  // namespace unexportable_keys
