// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"

#include <string>

#include "components/safe_browsing/core/browser/db/safebrowsing.pb.h"

namespace safe_browsing {

class V4GetHashProtocolManagerFuzzer {
 public:
  static int FuzzMetadata(const uint8_t* data, size_t size) {
    FindFullHashesResponse response;
    std::string input(reinterpret_cast<const char*>(data), size);
    if (!response.ParseFromString(input))
      return 0;
    safe_browsing::ThreatMetadata metadata;
    for (const ThreatMatch& match : response.matches()) {
      V4GetHashProtocolManager::ParseMetadata(match, &metadata);
    }
    return 0;
  }
};

}  // namespace safe_browsing

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return safe_browsing::V4GetHashProtocolManagerFuzzer::FuzzMetadata(data,
                                                                     size);
}
