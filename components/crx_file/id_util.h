// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRX_FILE_ID_UTIL_H_
#define COMPONENTS_CRX_FILE_ID_UTIL_H_

#include "base/strings/string_piece.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace base {
class FilePath;
}

namespace crx_file::id_util {

// The number of bytes in a legal id.
extern const size_t kIdSize;

// Generates an extension ID from arbitrary input. The same input string will
// always generate the same output ID.
std::string GenerateId(base::StringPiece input);

// Generates an ID from a HEX string. The same input string will always generate
// the same output ID.
std::string GenerateIdFromHex(const std::string& input);

// Generates an ID from the first |kIdSize| bytes of a SHA256 hash.
// |hash_size| must be at least |kIdSize|.
std::string GenerateIdFromHash(const uint8_t* hash, size_t hash_size);

// Generates an ID for an extension in the given path.
// Used while developing extensions, before they have a key.
std::string GenerateIdForPath(const base::FilePath& path);

// Returns the hash of an extension ID in hex.
std::string HashedIdInHex(const std::string& id);

// Normalizes the path for use by the extension. On Windows, this will make
// sure the drive letter is uppercase.
base::FilePath MaybeNormalizePath(const base::FilePath& path);

// Checks if |id| is a valid extension-id. Extension-ids are used for anything
// that comes in a CRX file, including apps, extensions, and components.
bool IdIsValid(base::StringPiece id);

}  // namespace crx_file::id_util

#endif  // COMPONENTS_CRX_FILE_ID_UTIL_H_
