// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_

#include "base/types/token_type.h"

namespace unexportable_keys {

// Strongly typed id for identifying unexportable keys.
// Default constructor creates a new, unique key ID.
using UnexportableKeyId = base::TokenType<class UnexportableKeyIdMarker>;

// A subclass of `UnexportableKeyId` that represents a signing key specifically.
//
// Inheritance is used here instead of a distinct tag to allow implicit
// conversion to the base `UnexportableKeyId` for type-agnostic APIs,
// while preventing accidental interchange with other specific key types.
//
// TODO(b/501307307): Replace existing usages of `UnexportableKeyId` that should
// be `UnexportableSigningKeyId`.
class UnexportableSigningKeyId : public UnexportableKeyId {
 public:
  using UnexportableKeyId::UnexportableKeyId;

  // Allows explicit conversion from the base class.
  explicit UnexportableSigningKeyId(UnexportableKeyId key_id)
      : UnexportableKeyId(key_id) {}
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_
