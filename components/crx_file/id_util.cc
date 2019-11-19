// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/id_util.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "crypto/sha2.h"

namespace {

// Converts a normal hexadecimal string into the alphabet used by extensions.
// We use the characters 'a'-'p' instead of '0'-'f' to avoid ever having a
// completely numeric host, since some software interprets that as an IP
// address.
static void ConvertHexadecimalToIDAlphabet(std::string* id) {
  for (auto& ch : *id) {
    int val;
    if (base::HexStringToInt(base::StringPiece(&ch, 1), &val)) {
      ch = 'a' + val;
    } else {
      ch = 'a';
    }
  }
}

}  // namespace

namespace crx_file {
namespace id_util {

// First 16 bytes of SHA256 hashed public key.
const size_t kIdSize = 16;

std::string GenerateId(base::StringPiece input) {
  uint8_t hash[kIdSize];
  crypto::SHA256HashString(input, hash, sizeof(hash));
  return GenerateIdFromHash(hash, sizeof(hash));
}

std::string GenerateIdFromHash(const uint8_t* hash, size_t hash_size) {
  CHECK_GE(hash_size, kIdSize);
  std::string result = base::HexEncode(hash, kIdSize);
  ConvertHexadecimalToIDAlphabet(&result);
  return result;
}

std::string GenerateIdFromHex(const std::string& input) {
  std::string output = input;
  ConvertHexadecimalToIDAlphabet(&output);
  return output;
}

std::string GenerateIdForPath(const base::FilePath& path) {
  base::FilePath new_path = MaybeNormalizePath(path);
  const base::StringPiece path_bytes(
      reinterpret_cast<const char*>(new_path.value().data()),
      new_path.value().size() * sizeof(base::FilePath::CharType));
  return GenerateId(path_bytes);
}

std::string HashedIdInHex(const std::string& id) {
  const std::string id_hash = base::SHA1HashString(id);
  DCHECK_EQ(base::kSHA1Length, id_hash.length());
  return base::HexEncode(id_hash.c_str(), id_hash.length());
}

base::FilePath MaybeNormalizePath(const base::FilePath& path) {
#if defined(OS_WIN)
  // Normalize any drive letter to upper-case. We do this for consistency with
  // net_utils::FilePathToFileURL(), which does the same thing, to make string
  // comparisons simpler.
  base::FilePath::StringType path_str = path.value();
  if (path_str.size() >= 2 && path_str[0] >= L'a' && path_str[0] <= L'z' &&
      path_str[1] == L':')
    path_str[0] = towupper(path_str[0]);

  return base::FilePath(path_str);
#else
  return path;
#endif
}

bool IdIsValid(const std::string& id) {
  // Verify that the id is legal.
  if (id.size() != (crx_file::id_util::kIdSize * 2))
    return false;

  for (size_t i = 0; i < id.size(); i++) {
    const char ch = base::ToLowerASCII(id[i]);
    if (ch < 'a' || ch > 'p')
      return false;
  }

  return true;
}

}  // namespace id_util
}  // namespace crx_file
