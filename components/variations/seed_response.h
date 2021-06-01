// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SEED_RESPONSE_H_
#define COMPONENTS_VARIATIONS_SEED_RESPONSE_H_

#include <string>

namespace variations {

// Represents data received when downloading the seed: "data" is the response
// body while the other fields come from headers.
// This is only used on Android.
struct SeedResponse {
  SeedResponse();
  ~SeedResponse();

  std::string data;  // "data" is binary, for which protobuf uses strings.
  std::string signature;
  std::string country;
  int64_t date;
  bool is_gzip_compressed = false;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SEED_RESPONSE_H_
