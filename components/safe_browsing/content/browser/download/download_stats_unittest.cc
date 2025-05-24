// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/download/download_stats.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "components/download/public/common/download_stats.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The below constants are based on download_file_types.asciipb.
const int kExeFileTypeUmaValue = 0;
const int kApkFileTypeUmaValue = 20;

}  // namespace

namespace safe_browsing {

TEST(SafeBrowsingDownloadStatsTest, RecordDangerousDownloadWarningShown) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordDangerousDownloadWarningShown(
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
      base::FilePath(FILE_PATH_LITERAL("file.apk")),
      /*is_https=*/true, /*has_user_gesture=*/true);
  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.Warning.FileType.Malicious.Shown",
      /*sample=*/kApkFileTypeUmaValue, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.Warning.DownloadIsHttps.Malicious.Shown",
      /*sample=*/1, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.Warning.DownloadHasUserGesture.Malicious.Shown",
      /*sample=*/1, /*expected_bucket_count=*/1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SafeBrowsing.Download.WarningShown"));

  RecordDangerousDownloadWarningShown(
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
      base::FilePath(FILE_PATH_LITERAL("file.exe")),
      /*is_https=*/false, /*has_user_gesture=*/false);
  histogram_tester.ExpectBucketCount(
      "SBClientDownload.Warning.FileType.Uncommon.Shown",
      /*sample=*/kExeFileTypeUmaValue,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "SBClientDownload.Warning.DownloadIsHttps.Uncommon.Shown",
      /*sample=*/0,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "SBClientDownload.Warning.DownloadHasUserGesture.Uncommon.Shown",
      /*sample=*/0,
      /*expected_count=*/1);
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "SafeBrowsing.Download.WarningShown"));
}

TEST(SafeBrowsingDownloadStatsTest, RecordDangerousDownloadWarningBypassed) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordDangerousDownloadWarningBypassed(
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      base::FilePath(FILE_PATH_LITERAL("file.apk")),
      /*is_https=*/true, /*has_user_gesture=*/false);
  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.Warning.FileType.DangerousFileType.Bypassed",
      /*sample=*/kApkFileTypeUmaValue, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.Warning.DownloadIsHttps.DangerousFileType.Bypassed",
      /*sample=*/1, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SBClientDownload.Warning.DownloadHasUserGesture.DangerousFileType."
      "Bypassed",
      /*sample=*/0, /*expected_bucket_count=*/1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SafeBrowsing.Download.WarningBypassed"));
}

TEST(SafeBrowsingDownloadStatsTest, RecordDownloadFileTypeAttributes) {
  {
    base::HistogramTester histogram_tester;
    RecordDownloadFileTypeAttributes(DownloadFileType::ALLOW_ON_USER_GESTURE,
                                     /*has_user_gesture=*/false,
                                     /*visited_referrer_before=*/false,
                                     /*latest_bypass_time=*/std::nullopt);
    histogram_tester.ExpectUniqueSample(
        "SBClientDownload.UserGestureFileType.Attributes",
        /*sample=*/UserGestureFileTypeAttributes::TOTAL_TYPE_CHECKED,
        /*expected_bucket_count=*/1);
  }
  {
    base::HistogramTester histogram_tester;
    RecordDownloadFileTypeAttributes(
        DownloadFileType::ALLOW_ON_USER_GESTURE,
        /*has_user_gesture=*/true,
        /*visited_referrer_before=*/true,
        /*latest_bypass_time=*/base::Time::Now() - base::Hours(1));
    histogram_tester.ExpectBucketCount(
        "SBClientDownload.UserGestureFileType.Attributes",
        /*sample=*/UserGestureFileTypeAttributes::TOTAL_TYPE_CHECKED,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "SBClientDownload.UserGestureFileType.Attributes",
        /*sample=*/UserGestureFileTypeAttributes::HAS_USER_GESTURE,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "SBClientDownload.UserGestureFileType.Attributes",
        /*sample=*/UserGestureFileTypeAttributes::HAS_REFERRER_VISIT,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "SBClientDownload.UserGestureFileType.Attributes",
        /*sample=*/
        UserGestureFileTypeAttributes::HAS_BOTH_USER_GESTURE_AND_REFERRER_VISIT,
        /*expected_count=*/1);
  }
}

}  // namespace safe_browsing
