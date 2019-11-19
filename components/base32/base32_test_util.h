// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BASE32_BASE32_TEST_UTIL_H_
#define COMPONENTS_BASE32_BASE32_TEST_UTIL_H_

#include <string>

#include "base/strings/string_piece.h"

namespace base32 {

// Decodes the |input| string piece from base32.
std::string Base32Decode(base::StringPiece input);

}  // namespace base32

#endif  // COMPONENTS_BASE32_BASE32_TEST_UTIL_H_
