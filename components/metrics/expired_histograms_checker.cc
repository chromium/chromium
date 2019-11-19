// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/expired_histograms_checker.h"

#include <algorithm>
#include <vector>

#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"

namespace metrics {

ExpiredHistogramsChecker::ExpiredHistogramsChecker(
    const uint64_t* array,
    size_t size,
    const std::string& whitelist_str)
    : array_(array), size_(size) {
  InitWhitelist(whitelist_str);
}

ExpiredHistogramsChecker::~ExpiredHistogramsChecker() {}

bool ExpiredHistogramsChecker::ShouldRecord(uint64_t histogram_hash) const {
  // If histogram is whitelisted then it should always be recorded.
  if (base::Contains(whitelist_, histogram_hash))
    return true;
  return !std::binary_search(array_, array_ + size_, histogram_hash);
}

void ExpiredHistogramsChecker::InitWhitelist(const std::string& whitelist_str) {
  std::vector<base::StringPiece> whitelist_names = base::SplitStringPiece(
      whitelist_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (base::StringPiece name : whitelist_names)
    whitelist_.insert(base::HashMetricName(name));
}

}  // namespace metrics
