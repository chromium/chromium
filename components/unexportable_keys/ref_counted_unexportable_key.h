// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_KEY_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_KEY_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

// Shared base class for lifetime interlocking of unexportable keys.
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) RefCountedUnexportableKey
    : public base::RefCountedThreadSafe<RefCountedUnexportableKey> {
 public:
  virtual crypto::UnexportableKey& key() const = 0;
  virtual const UnexportableKeyId& id() const = 0;

 protected:
  virtual ~RefCountedUnexportableKey() = default;

 private:
  friend class base::RefCountedThreadSafe<RefCountedUnexportableKey>;
};

// RefCounted wrapper around `crypto::UnexportableSigningKey`.
//
// Also contains a unique id token that identifies a class instance. This id can
// be used for a faster key comparison (as opposed to comparing public key
// infos). It doesn't guarantee that two objects with different ids have
// different underlying keys.
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) RefCountedUnexportableSigningKey
    : public RefCountedUnexportableKey {
 public:
  // `key` must be non-null.
  explicit RefCountedUnexportableSigningKey(
      std::unique_ptr<crypto::UnexportableSigningKey> key,
      UnexportableSigningKeyId key_id);

  // Use covariance to return more specific types for `key` and `id`.
  crypto::UnexportableSigningKey& key() const override;
  const UnexportableSigningKeyId& id() const override;

 private:
  ~RefCountedUnexportableSigningKey() override;

  const std::unique_ptr<crypto::UnexportableSigningKey> key_;
  const UnexportableSigningKeyId id_;
};

// RefCounted wrapper around `crypto::UnexportableAttestationKey`.
//
// Also contains a unique id token that identifies a class instance. This id can
// be used for a faster key comparison (as opposed to comparing public key
// infos). It doesn't guarantee that two objects with different ids have
// different underlying keys.
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) RefCountedUnexportableAttestationKey
    : public RefCountedUnexportableKey {
 public:
  // `key` must be non-null.
  explicit RefCountedUnexportableAttestationKey(
      std::unique_ptr<crypto::UnexportableAttestationKey> key,
      UnexportableAttestationKeyId key_id);

  // Use covariance to return more specific types for `key` and `id`.
  crypto::UnexportableAttestationKey& key() const override;
  const UnexportableAttestationKeyId& id() const override;

 private:
  ~RefCountedUnexportableAttestationKey() override;

  const std::unique_ptr<crypto::UnexportableAttestationKey> key_;
  const UnexportableAttestationKeyId id_;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_KEY_H_
