// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_SIGNING_KEY_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_SIGNING_KEY_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "components/unexportable_keys/unexportable_key_id.h"

namespace crypto {
class UnexportableSigningKey;
}

namespace unexportable_keys {

// RefCounted wrapper around `crypto::UnexportableSigningKey`.
//
// Also contains a unique id token that identifies a class instance. This id can
// be used for a faster key comparison (as opposed to comparing public key
// infos). It doesn't guarantee that two objects with different ids have
// different underlying keys.
// This id can be written to disk and re-used across browser sessions.
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) RefCountedUnexportableSigningKey
    : public base::RefCountedThreadSafe<RefCountedUnexportableSigningKey> {
 public:
  // `key` must be non-null.
  explicit RefCountedUnexportableSigningKey(
      std::unique_ptr<crypto::UnexportableSigningKey> key,
      const UnexportableKeyId& key_id);

  RefCountedUnexportableSigningKey(const RefCountedUnexportableSigningKey&) =
      delete;
  RefCountedUnexportableSigningKey& operator=(
      const RefCountedUnexportableSigningKey&) = delete;

  crypto::UnexportableSigningKey& key() const { return *key_; }
  const UnexportableKeyId& id() const { return id_; }

 private:
  friend class base::RefCountedThreadSafe<RefCountedUnexportableSigningKey>;
  ~RefCountedUnexportableSigningKey();

  const std::unique_ptr<crypto::UnexportableSigningKey> key_;
  const UnexportableKeyId id_;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_SIGNING_KEY_H_
