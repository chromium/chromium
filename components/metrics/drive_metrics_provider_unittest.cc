// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(DriveMetricsProviderTest, HasSeekPenalty) {
  base::FilePath tmp_path;
  ASSERT_TRUE(base::GetTempDir(&tmp_path));
  bool unused;
  DriveMetricsProvider::HasSeekPenalty(tmp_path, &unused);
}

}  // namespace metrics
