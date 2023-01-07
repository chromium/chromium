// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/murmurhash3_util.h"
#include "third_party/smhasher/src/MurmurHash3.h"

namespace safe_browsing {

uint32_t MurmurHash3String(const std::string& str, uint32_t seed) {
  uint32_t output;
  MurmurHash3_x86_32(str.data(), str.size(), seed, &output);
  return output;
}

}  // namespace safe_browsing
