// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include <string>
#include <unordered_map>

#include "base/optional.h"
#include "components/link_header_util/link_header_util.h"

namespace link_header_util {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  const auto result = SplitLinkHeader(input);

  for (const auto& pair : result) {
    assert(pair.first < pair.second);
    std::string url;
    std::unordered_map<std::string, base::Optional<std::string>> params;
    (void)ParseLinkHeaderValue(pair.first, pair.second, &url, &params);
  }

  return 0;
}

}  // namespace link_header_util
