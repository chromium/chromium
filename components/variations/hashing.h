// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_HASHING_H_
#define COMPONENTS_VARIATIONS_HASHING_H_

#include <stdint.h>

#include "base/strings/string_piece.h"

namespace variations {

// Computes a uint32_t hash of a given string based on its SHA1 hash. Suitable
// for uniquely identifying field trial names and group names.
uint32_t HashName(base::StringPiece name);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_HASHING_H_
