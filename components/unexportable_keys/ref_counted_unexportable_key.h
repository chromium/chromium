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

// RefCounted wrapper around `crypto::UnexportableSigningKey`.
//
// Also contains a unique id token that identifies a class instance. This id can
// be used for a faster key comparison (as opposed to comparing public key
// infos). It doesn't guarantee that two objects with different ids have
// different underlying keys.
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) RefCountedUnexportableSigningKey
    : public base::RefCountedThreadSafe<RefCountedUnexportableSigningKey> {
 public:
  using IdType = UnexportableSigningKeyId;

  virtual crypto::UnexportableSigningKey& key() const = 0;
  virtual const UnexportableSigningKeyId& id() const = 0;

 protected:
  virtual ~RefCountedUnexportableSigningKey() = default;

 private:
  friend class base::RefCountedThreadSafe<RefCountedUnexportableSigningKey>;
};

// Creates a `RefCountedUnexportableSigningKey` wrapping `key`.
// `key` must be non-null.
COMPONENT_EXPORT(UNEXPORTABLE_KEYS)
scoped_refptr<RefCountedUnexportableSigningKey>
MakeRefCountedUnexportableSigningKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key);

// RefCounted wrapper around `crypto::UnexportableAttestationKey`.
//
// Also contains a unique id token that identifies a class instance. This id can
// be used for a faster key comparison (as opposed to comparing public key
// infos). It doesn't guarantee that two objects with different ids have
// different underlying keys.
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) RefCountedUnexportableAttestationKey
    : public RefCountedUnexportableSigningKey {
 public:
  using IdType = UnexportableAttestationKeyId;

  // Use covariance to return more specific types for `key` and `id`.
  crypto::UnexportableAttestationKey& key() const override = 0;
  const UnexportableAttestationKeyId& id() const override = 0;

 protected:
  ~RefCountedUnexportableAttestationKey() override = default;
};

// Creates a `RefCountedUnexportableAttestationKey` wrapping `key`.
// `key` must be non-null.
COMPONENT_EXPORT(UNEXPORTABLE_KEYS)
scoped_refptr<RefCountedUnexportableAttestationKey>
MakeRefCountedUnexportableAttestationKey(
    std::unique_ptr<crypto::UnexportableAttestationKey> key);

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_KEY_H_
