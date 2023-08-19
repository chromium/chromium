// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ConnectionType = net::NetworkChangeNotifier::ConnectionType;

namespace download {

TEST(DownloadStatsTest, RecordNewDownloadStarted) {
  base::HistogramTester histogram_tester;
  RecordNewDownloadStarted(ConnectionType::CONNECTION_WIFI,
                           DownloadSource::NAVIGATION);
  histogram_tester.ExpectBucketCount("Download.Counts",
                                     DownloadCountTypes::NEW_DOWNLOAD_COUNT, 1);
  histogram_tester.ExpectBucketCount("Download.Counts.Navigation",
                                     DownloadCountTypes::NEW_DOWNLOAD_COUNT, 1);
  histogram_tester.ExpectBucketCount("Download.NetworkConnectionType.StartNew",
                                     ConnectionType::CONNECTION_WIFI, 1);
  histogram_tester.ExpectBucketCount(
      "Download.NetworkConnectionType.StartNew.Navigation",
      ConnectionType::CONNECTION_WIFI, 1);
}

TEST(DownloadStatsTest, RecordDownloadCompleted) {
  base::HistogramTester histogram_tester;
  RecordDownloadCompleted(1, true, ConnectionType::CONNECTION_WIFI,
                          DownloadSource::NAVIGATION);
  histogram_tester.ExpectBucketCount("Download.Counts",
                                     DownloadCountTypes::COMPLETED_COUNT, 1);
  histogram_tester.ExpectBucketCount("Download.Counts.Navigation",
                                     DownloadCountTypes::COMPLETED_COUNT, 1);
  histogram_tester.ExpectBucketCount("Download.DownloadSize", 0, 1);
  histogram_tester.ExpectBucketCount("Download.DownloadSize.Parallelizable", 0,
                                     1);
}

}  // namespace download
