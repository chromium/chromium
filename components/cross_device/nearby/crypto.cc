// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/nearby/src/internal/platform/implementation/crypto.h"

#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_view_util.h"
#include "crypto/hash.h"
#include "crypto/obsolete/md5.h"

namespace nearby {

std::array<uint8_t, crypto::obsolete::Md5::kSize> Md5ForNearby(
    std::string_view input) {
  return crypto::obsolete::Md5::Hash(input);
}

void Crypto::Init() {}

ByteArray Crypto::Md5(std::string_view input) {
  if (input.empty())
    return ByteArray();

  auto digest = Md5ForNearby(input);
  auto digest_span = base::as_chars(base::span(digest));
  return ByteArray(digest_span.data(), digest_span.size());
}

ByteArray Crypto::Sha256(std::string_view input) {
  if (input.empty())
    return ByteArray();

  return ByteArray(
      std::string(base::as_string_view(crypto::hash::Sha256(input))));
}

}  // namespace nearby
