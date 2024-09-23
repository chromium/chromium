// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/nearby/src/internal/platform/implementation/crypto.h"

#include "base/containers/span.h"
#include "base/hash/md5.h"
#include "base/memory/ptr_util.h"
#include "crypto/sha2.h"

#include <vector>

namespace nearby {

void Crypto::Init() {}

ByteArray Crypto::Md5(std::string_view input) {
  if (input.empty())
    return ByteArray();

  base::MD5Digest digest;
  base::MD5Sum(base::as_byte_span(input), &digest);
  return ByteArray(std::string(std::begin(digest.a), std::end(digest.a)));
}

ByteArray Crypto::Sha256(std::string_view input) {
  if (input.empty())
    return ByteArray();

  return ByteArray(crypto::SHA256HashString(std::string(input)));
}

}  // namespace nearby
