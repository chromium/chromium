// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/ntp_visit_scores.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {

namespace {

using NtpVisitScoresTest = testing::Test;

TEST(NtpVisitScoresTest, GetNtpVisitAttributesScore) {
  history::ClusterVisit visit;
  EXPECT_EQ(GetNtpVisitAttributesScore(visit), 0.0);
}

}  // namespace

}  // namespace history_clusters
