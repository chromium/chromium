// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_preview_metrics {

using base::Bucket;
using testing::ElementsAre;

TEST(MediaPreviewMetricsTest, CustomDurationHistogramPermissionPrompt) {
  base::HistogramTester tester;

  Context context(UiLocation::kPermissionPrompt, PreviewType::kCameraAndMic);
  std::string metric_name =
      "MediaPreviews.UI.Permissions.CameraAndMic.Duration";

  tester.ExpectTotalCount(metric_name, 0);

  // Add one sample to bucket with min_value =  2.
  RecordMediaPreviewDuration(context, base::Seconds(3));
  tester.ExpectUniqueSample(metric_name, /*sample=*/2,
                            /*expected_bucket_count=*/1);

  // Add one sample to bucket with min_value =  0.
  RecordMediaPreviewDuration(context, base::Milliseconds(5.34));
  EXPECT_THAT(tester.GetAllSamples(metric_name),
              ElementsAre(Bucket(0, 1), Bucket(2, 1)));

  // Add one sample to bucket with min_value =  1.
  RecordMediaPreviewDuration(context, base::Seconds(1.4));
  EXPECT_THAT(tester.GetAllSamples(metric_name),
              ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1)));
}

TEST(MediaPreviewMetricsTest, CustomDurationHistogramPageInfo) {
  base::HistogramTester tester;

  Context context(UiLocation::kPageInfo, PreviewType::kCamera);
  std::string metric_name = "MediaPreviews.UI.PageInfo.Camera.Duration";

  tester.ExpectTotalCount(metric_name, 0);

  // Add one sample to bucket with min_value = 8.
  RecordMediaPreviewDuration(context, base::Seconds(10));
  tester.ExpectUniqueSample(metric_name, /*sample=*/8,
                            /*expected_bucket_count=*/1);

  // Add one sample to bucket with min_value = 512 (overflow bucket).
  RecordMediaPreviewDuration(context, base::Minutes(10));
  EXPECT_THAT(tester.GetAllSamples(metric_name),
              ElementsAre(Bucket(8, 1), Bucket(512, 1)));
}

}  // namespace media_preview_metrics
