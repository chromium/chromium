// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_

#include "base/types/token_type.h"

namespace unexportable_keys {

// Strongly typed id for identifying unexportable signing keys.
// Default constructor creates a new, unique key ID.
using UnexportableKeyId = base::TokenType<class UnexportableKeyIdMarker>;

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_ID_H_
