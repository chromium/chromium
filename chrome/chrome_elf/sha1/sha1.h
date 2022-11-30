// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// * This code is taken from base/sha1, with small changes.
//------------------------------------------------------------------------------

#ifndef CHROME_CHROME_ELF_SHA1_SHA1_H_
#define CHROME_CHROME_ELF_SHA1_SHA1_H_

#include <stddef.h>

#include <array>
#include <string>

namespace elf_sha1 {

// Length in bytes of a SHA-1 hash.
constexpr size_t kSHA1Length = 20;

using Digest = std::array<uint8_t, kSHA1Length>;

// Returns the computed SHA1 of the input string |str|.
Digest SHA1HashString(const std::string& str);

}  // namespace elf_sha1

#endif  // CHROME_CHROME_ELF_SHA1_SHA1_H_
