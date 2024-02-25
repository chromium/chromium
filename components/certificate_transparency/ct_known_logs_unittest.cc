// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/ct_known_logs.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/time/time.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {

namespace {
#include "components/certificate_transparency/data/log_list-inc.cc"
}  // namespace

TEST(CTKnownLogsTest, DisallowedLogsAreSortedByLogID) {
  std::vector<std::pair<std::string, base::Time>> disqualified_logs =
      GetDisqualifiedLogs();
  ASSERT_TRUE(std::is_sorted(
      std::begin(disqualified_logs), std::end(disqualified_logs),
      [](const auto& a, const auto& b) { return a.first < b.first; }));
}

}  // namespace certificate_transparency
