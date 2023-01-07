// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

TEST(BackgroundDownloadStatsTest, DownloadComplete) {
  base::HistogramTester histogram_tester;
  stats::LogDownloadCompletion(DownloadClient::BACKGROUND_FETCH,
                               CompletionType::SUCCEED, 1024 * 1024);

  histogram_tester.ExpectUniqueSample("Download.Service.Finish.Type",
                                      static_cast<int>(CompletionType::SUCCEED),
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Download.Service.Complete.FileSize.BackgroundFetch", 1024, 1);
}

}  // namespace
}  // namespace download
