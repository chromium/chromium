// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace media_router {

namespace {

const base::Time init_time = base::Time::Now();
const base::Time paint_time = init_time + base::TimeDelta::FromMilliseconds(50);
const base::Time sink_load_time =
    init_time + base::TimeDelta::FromMilliseconds(300);
const base::Time start_casting_time =
    init_time + base::TimeDelta::FromMilliseconds(2000);
const base::Time close_dialog_time =
    init_time + base::TimeDelta::FromMilliseconds(3000);

}  // namespace

class CastDialogMetricsTest : public testing::Test {
 public:
  CastDialogMetricsTest() = default;
  ~CastDialogMetricsTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::HistogramTester tester_;
  CastDialogMetrics metrics_{init_time, &profile_};
};

TEST_F(CastDialogMetricsTest, OnSinksLoaded) {
  metrics_.OnSinksLoaded(sink_load_time);
  tester_.ExpectUniqueSample(
      MediaRouterMetrics::kHistogramUiDialogLoadedWithData,
      (sink_load_time - init_time).InMilliseconds(), 1);
}

TEST_F(CastDialogMetricsTest, OnPaint) {
  metrics_.OnPaint(paint_time);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramUiDialogPaint,
                             (paint_time - init_time).InMilliseconds(), 1);
}

TEST_F(CastDialogMetricsTest, OnStartCasting) {
  constexpr int kSinkIndex = 4;
  metrics_.OnSinksLoaded(sink_load_time);
  metrics_.OnStartCasting(start_casting_time, kSinkIndex);
  tester_.ExpectUniqueSample(
      MediaRouterMetrics::kHistogramStartLocalLatency,
      (start_casting_time - sink_load_time).InMilliseconds(), 1);
}

TEST_F(CastDialogMetricsTest, OnStopCasting) {
  metrics_.OnStopCasting(/* is_local_route*/ false);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramStopRoute,
                             /* Remote route */ 1, 1);
}

TEST_F(CastDialogMetricsTest, OnCloseDialog) {
  metrics_.OnPaint(paint_time);
  metrics_.OnCloseDialog(close_dialog_time);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramCloseLatency,
                             (close_dialog_time - paint_time).InMilliseconds(),
                             1);
}

TEST_F(CastDialogMetricsTest, OnRecordSinkCount) {
  constexpr int kSinkCount = 3;
  metrics_.OnRecordSinkCount(kSinkCount);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramUiDeviceCount,
                             kSinkCount, 1);
}

TEST_F(CastDialogMetricsTest, RecordFirstAction) {
  metrics_.OnStopCasting(true);
  metrics_.OnCastModeSelected();
  metrics_.OnCloseDialog(close_dialog_time);
  // Only the first action should be recorded for the first action metric.
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramUiFirstAction,
                             MediaRouterUserAction::STOP_LOCAL, 1);
}

TEST_F(CastDialogMetricsTest, RecordIconState) {
  tester_.ExpectUniqueSample(
      MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen,
      /* is_pinned */ false, 1);

  profile_.GetPrefs()->SetBoolean(prefs::kShowCastIconInToolbar, true);
  CastDialogMetrics metrics_with_pinned_icon{init_time, &profile_};
  tester_.ExpectBucketCount(
      MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen,
      /* is_pinned */ true, 1);
}

}  // namespace media_router
