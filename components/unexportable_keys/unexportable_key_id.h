// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_

#include "base/types/token_type.h"

namespace unexportable_keys {

// Strongly typed id for identifying unexportable signing keys.
// Default constructor creates a new, unique key ID.
using UnexportableSigningKeyId =
    base::TokenType<class UnexportableSigningKeyIdMarker>;

// A subclass of `UnexportableSigningKeyId` that represents an attestation key
// specifically.
//
// Inheritance is used here instead of a distinct tag to allow implicit
// conversion to the base `UnexportableSigningKeyId` for type-agnostic APIs,
// while preventing accidental interchange with other specific key types.
class UnexportableAttestationKeyId : public UnexportableSigningKeyId {
 public:
  using UnexportableSigningKeyId::UnexportableSigningKeyId;

  // Allows explicit conversion from the base class.
  explicit UnexportableAttestationKeyId(UnexportableSigningKeyId key_id)
      : UnexportableSigningKeyId(key_id) {}
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_
