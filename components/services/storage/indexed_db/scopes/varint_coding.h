// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_VARINT_CODING_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_VARINT_CODING_H_

#include <stdint.h>

#include <string>
#include <string_view>

namespace content::indexed_db {

// Encodes the given number into the given string using VarInt encoding. VarInts
// try to compress the number into the smallest number of bytes possible. It
// basically uses the top bit of the byte to signal if there are more bytes to
// read. As soon as the top bit is 0, then the number has been fully read.
// Note: |from| must be >= 0.
void EncodeVarInt(int64_t from, std::string* into);

// Decodes a varint from the given string piece into the given int64_t. Returns
// if the  string had a valid varint (where a byte was found with it's top bit
// set). This function does NOT check to see if move than 64 bits were read.
[[nodiscard]] bool DecodeVarInt(std::string_view* from, int64_t* into);

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_VARINT_CODING_H_
