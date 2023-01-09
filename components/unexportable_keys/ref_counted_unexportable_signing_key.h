// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_SIGNING_KEY_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_SIGNING_KEY_H_

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/types/strong_alias.h"

namespace crypto {
class UnexportableSigningKey;
}

namespace unexportable_keys {

// RefCounted wrapper around `crypto::UnexportableSigningKey`.
//
// Also contains a unique id that identifies a class instance. It doesn't
// guarantee that two objects with different ids have different underlying keys.
class RefCountedUnexportableSigningKey
    : public base::RefCountedThreadSafe<RefCountedUnexportableSigningKey> {
 public:
  // A unique id that identifies a class instance. Can be used for a faster key
  // comparison (as opposed to comparing public key infos).
  using KeyId = base::StrongAlias<class KeyIdTag, uint32_t>;

  // `key` must be non-null.
  explicit RefCountedUnexportableSigningKey(
      std::unique_ptr<crypto::UnexportableSigningKey> key);

  RefCountedUnexportableSigningKey(const RefCountedUnexportableSigningKey&) =
      delete;
  RefCountedUnexportableSigningKey& operator=(
      const RefCountedUnexportableSigningKey&) = delete;

  crypto::UnexportableSigningKey& key() const { return *key_; }
  KeyId id() const { return key_id_; }

 private:
  friend class base::RefCountedThreadSafe<RefCountedUnexportableSigningKey>;
  ~RefCountedUnexportableSigningKey();

  const std::unique_ptr<crypto::UnexportableSigningKey> key_;
  const KeyId key_id_;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_REF_COUNTED_UNEXPORTABLE_SIGNING_KEY_H_
