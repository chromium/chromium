// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/expired_histograms_checker.h"

#include <algorithm>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_split.h"

namespace metrics {

ExpiredHistogramsChecker::ExpiredHistogramsChecker(
    const uint32_t* expired_histogram_hashes,
    size_t size,
    const std::string& allowlist_str)
    : expired_histogram_hashes_(expired_histogram_hashes), size_(size) {
  InitAllowlist(allowlist_str);
}

ExpiredHistogramsChecker::~ExpiredHistogramsChecker() {}

bool ExpiredHistogramsChecker::ShouldRecord(uint32_t histogram_hash) const {
  // If histogram is explicitly allowed then it should always be recorded.
  if (base::Contains(allowlist_, histogram_hash))
    return true;
  return !std::binary_search(expired_histogram_hashes_,
                             expired_histogram_hashes_ + size_, histogram_hash);
}

void ExpiredHistogramsChecker::InitAllowlist(const std::string& allowlist_str) {
  std::vector<base::StringPiece> allowlist_names = base::SplitStringPiece(
      allowlist_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (base::StringPiece name : allowlist_names)
    allowlist_.insert(base::HashMetricNameAs32Bits(name));
}

}  // namespace metrics
