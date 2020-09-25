// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/common/pref_names.h"
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
  CastDialogMetrics metrics_{init_time, MediaRouterDialogOpenOrigin::TOOLBAR,
                             &profile_};
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
  metrics_.OnStartCasting(start_casting_time, kSinkIndex, TAB_MIRROR);
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

  profile_.GetPrefs()->SetBoolean(::prefs::kShowCastIconInToolbar, true);
  CastDialogMetrics metrics_with_pinned_icon{
      init_time, MediaRouterDialogOpenOrigin::PAGE, &profile_};
  tester_.ExpectBucketCount(
      MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen,
      /* is_pinned */ true, 1);
}

TEST_F(CastDialogMetricsTest, RecordCloudPref) {
  // When a dialog is opened after enabling the cloud services, that should be
  // recorded.
  profile_.GetPrefs()->SetBoolean(prefs::kMediaRouterEnableCloudServices, true);
  CastDialogMetrics metrics_with_cloud_pref_enabled{
      init_time, MediaRouterDialogOpenOrigin::PAGE, &profile_};
  tester_.ExpectBucketCount(MediaRouterMetrics::kHistogramCloudPrefAtDialogOpen,
                            /* enabled */ true, 1);
}

TEST_F(CastDialogMetricsTest, RecordDialogActivationLocationAndCastMode) {
  constexpr int kSinkIndex = 4;
  metrics_.OnSinksLoaded(sink_load_time);
  metrics_.OnStartCasting(start_casting_time, kSinkIndex, TAB_MIRROR);
  tester_.ExpectUniqueSample(
      "MediaRouter.Ui.Dialog.ActivationLocationAndCastMode",
      DialogActivationLocationAndCastMode::kEphemeralIconAndTabMirror, 1);

  CastDialogMetrics metrics_opened_from_page{
      init_time, MediaRouterDialogOpenOrigin::PAGE, &profile_};
  metrics_opened_from_page.OnSinksLoaded(sink_load_time);
  metrics_opened_from_page.OnStartCasting(start_casting_time, kSinkIndex,
                                          PRESENTATION);
  tester_.ExpectBucketCount(
      "MediaRouter.Ui.Dialog.ActivationLocationAndCastMode",
      DialogActivationLocationAndCastMode::kPageAndPresentation, 1);

  profile_.GetPrefs()->SetBoolean(::prefs::kShowCastIconInToolbar, true);
  CastDialogMetrics metrics_with_pinned_icon{
      init_time, MediaRouterDialogOpenOrigin::TOOLBAR, &profile_};
  metrics_with_pinned_icon.OnSinksLoaded(sink_load_time);
  metrics_with_pinned_icon.OnStartCasting(start_casting_time, kSinkIndex,
                                          DESKTOP_MIRROR);
  tester_.ExpectBucketCount(
      "MediaRouter.Ui.Dialog.ActivationLocationAndCastMode",
      DialogActivationLocationAndCastMode::kPinnedIconAndDesktopMirror, 1);
}

}  // namespace media_router
