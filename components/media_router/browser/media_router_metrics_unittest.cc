// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_metrics.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/media_router/common/media_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Bucket;
using testing::ElementsAre;

namespace media_router {

namespace {

// Tests that calling |recording_cb| with a TimeDelta records it in
// |histogram_name|.
void TestRecordTimeDeltaMetric(
    base::RepeatingCallback<void(const base::TimeDelta&)> recording_cb,
    const std::string& histogram_name) {
  base::HistogramTester tester;
  const base::TimeDelta delta = base::TimeDelta::FromMilliseconds(10);

  tester.ExpectTotalCount(histogram_name, 0);
  recording_cb.Run(delta);
  tester.ExpectUniqueSample(histogram_name, delta.InMilliseconds(), 1);
}

// Tests that calling |recording_cb| with boolean values records them in
// |histogram_name|.
void TestRecordBooleanMetric(base::RepeatingCallback<void(bool)> recording_cb,
                             const std::string& histogram_name) {
  base::HistogramTester tester;
  tester.ExpectTotalCount(histogram_name, 0);

  recording_cb.Run(true);
  recording_cb.Run(false);
  recording_cb.Run(true);

  tester.ExpectTotalCount(histogram_name, 3);
  EXPECT_THAT(tester.GetAllSamples(histogram_name),
              ElementsAre(Bucket(false, 1), Bucket(true, 2)));
}

}  // namespace

TEST(MediaRouterMetricsTest, RecordMediaRouterDialogOrigin) {
  base::HistogramTester tester;
  const MediaRouterDialogOpenOrigin origin1 =
      MediaRouterDialogOpenOrigin::TOOLBAR;
  const MediaRouterDialogOpenOrigin origin2 =
      MediaRouterDialogOpenOrigin::CONTEXTUAL_MENU;

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramIconClickLocation, 0);
  MediaRouterMetrics::RecordMediaRouterDialogOrigin(origin1);
  MediaRouterMetrics::RecordMediaRouterDialogOrigin(origin2);
  MediaRouterMetrics::RecordMediaRouterDialogOrigin(origin1);
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramIconClickLocation, 3);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramIconClickLocation),
      ElementsAre(Bucket(static_cast<int>(origin1), 2),
                  Bucket(static_cast<int>(origin2), 1)));
}

TEST(MediaRouterMetricsTest, RecordMediaRouterDialogPaint) {
  TestRecordTimeDeltaMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordMediaRouterDialogPaint),
      MediaRouterMetrics::kHistogramUiDialogPaint);
}

TEST(MediaRouterMetricsTest, RecordMediaRouterDialogLoaded) {
  TestRecordTimeDeltaMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordMediaRouterDialogLoaded),
      MediaRouterMetrics::kHistogramUiDialogLoadedWithData);
}

TEST(MediaRouterMetricsTest, RecordCloseDialogLatency) {
  TestRecordTimeDeltaMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordCloseDialogLatency),
      MediaRouterMetrics::kHistogramCloseLatency);
}

TEST(MediaRouterMetricsTest, RecordMediaRouterInitialUserAction) {
  base::HistogramTester tester;
  const MediaRouterUserAction action1 = MediaRouterUserAction::START_LOCAL;
  const MediaRouterUserAction action2 = MediaRouterUserAction::CLOSE;
  const MediaRouterUserAction action3 = MediaRouterUserAction::STATUS_REMOTE;

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramUiFirstAction, 0);
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action3);
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action2);
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action3);
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action1);
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramUiFirstAction, 4);
  EXPECT_THAT(tester.GetAllSamples(MediaRouterMetrics::kHistogramUiFirstAction),
              ElementsAre(Bucket(static_cast<int>(action1), 1),
                          Bucket(static_cast<int>(action2), 1),
                          Bucket(static_cast<int>(action3), 2)));
}

TEST(MediaRouterMetricsTest, RecordRouteCreationOutcome) {
  base::HistogramTester tester;
  const MediaRouterRouteCreationOutcome outcome1 =
      MediaRouterRouteCreationOutcome::SUCCESS;
  const MediaRouterRouteCreationOutcome outcome2 =
      MediaRouterRouteCreationOutcome::FAILURE_NO_ROUTE;

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramRouteCreationOutcome,
                          0);
  MediaRouterMetrics::RecordRouteCreationOutcome(outcome2);
  MediaRouterMetrics::RecordRouteCreationOutcome(outcome1);
  MediaRouterMetrics::RecordRouteCreationOutcome(outcome2);
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramRouteCreationOutcome,
                          3);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramRouteCreationOutcome),
      ElementsAre(Bucket(static_cast<int>(outcome1), 1),
                  Bucket(static_cast<int>(outcome2), 2)));
}

TEST(MediaRouterMetricsTest, RecordPresentationUrlType) {
  base::HistogramTester tester;

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramPresentationUrlType, 0);
  MediaRouterMetrics::RecordPresentationUrlType(GURL("cast:DEADBEEF"));
  MediaRouterMetrics::RecordPresentationUrlType(GURL("dial:AppName"));
  MediaRouterMetrics::RecordPresentationUrlType(GURL("cast-dial:AppName"));
  MediaRouterMetrics::RecordPresentationUrlType(GURL("https://example.com"));
  MediaRouterMetrics::RecordPresentationUrlType(GURL("http://example.com"));
  MediaRouterMetrics::RecordPresentationUrlType(
      GURL("https://google.com/cast#__castAppId__=DEADBEEF"));
  MediaRouterMetrics::RecordPresentationUrlType(GURL("remote-playback:foo"));
  MediaRouterMetrics::RecordPresentationUrlType(GURL("test:test"));

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramPresentationUrlType, 8);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramPresentationUrlType),
      ElementsAre(
          Bucket(static_cast<int>(PresentationUrlType::kOther), 1),
          Bucket(static_cast<int>(PresentationUrlType::kCast), 1),
          Bucket(static_cast<int>(PresentationUrlType::kCastDial), 1),
          Bucket(static_cast<int>(PresentationUrlType::kCastLegacy), 1),
          Bucket(static_cast<int>(PresentationUrlType::kDial), 1),
          Bucket(static_cast<int>(PresentationUrlType::kHttp), 1),
          Bucket(static_cast<int>(PresentationUrlType::kHttps), 1),
          Bucket(static_cast<int>(PresentationUrlType::kRemotePlayback), 1)));
}

TEST(MediaRouterMetricsTest, RecordMediaSinkType) {
  base::HistogramTester tester;
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramMediaSinkType, 0);

  MediaRouterMetrics::RecordMediaSinkType(SinkIconType::WIRED_DISPLAY);
  MediaRouterMetrics::RecordMediaSinkType(SinkIconType::CAST);
  MediaRouterMetrics::RecordMediaSinkType(SinkIconType::CAST_AUDIO);
  MediaRouterMetrics::RecordMediaSinkType(SinkIconType::HANGOUT);
  MediaRouterMetrics::RecordMediaSinkType(SinkIconType::CAST);
  MediaRouterMetrics::RecordMediaSinkType(SinkIconType::GENERIC);

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramMediaSinkType, 6);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramMediaSinkType),
      ElementsAre(Bucket(static_cast<int>(SinkIconType::CAST), 2),
                  Bucket(static_cast<int>(SinkIconType::CAST_AUDIO), 1),
                  Bucket(static_cast<int>(SinkIconType::HANGOUT), 1),
                  Bucket(static_cast<int>(SinkIconType::WIRED_DISPLAY), 1),
                  Bucket(static_cast<int>(SinkIconType::GENERIC), 1)));
}

TEST(MediaRouterMetricsTest, RecordDeviceCount) {
  base::HistogramTester tester;
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramUiDeviceCount, 0);

  MediaRouterMetrics::RecordDeviceCount(30);
  MediaRouterMetrics::RecordDeviceCount(0);

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramUiDeviceCount, 2);
  EXPECT_THAT(tester.GetAllSamples(MediaRouterMetrics::kHistogramUiDeviceCount),
              ElementsAre(Bucket(0, 1), Bucket(30, 1)));
}

TEST(MediaRouterMetricsTest, RecordStartRouteDeviceIndex) {
  base::HistogramTester tester;
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramStartLocalPosition, 0);

  MediaRouterMetrics::RecordStartRouteDeviceIndex(30);
  MediaRouterMetrics::RecordStartRouteDeviceIndex(0);

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramStartLocalPosition, 2);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramStartLocalPosition),
      ElementsAre(Bucket(0, 1), Bucket(30, 1)));
}

TEST(MediaRouterMetricsTest, RecordStartLocalSessionLatency) {
  TestRecordTimeDeltaMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordStartLocalSessionLatency),
      MediaRouterMetrics::kHistogramStartLocalLatency);
}

TEST(MediaRouterMetricsTest, RecordStartLocalSessionSuccessful) {
  TestRecordBooleanMetric(
      base::BindRepeating(
          &MediaRouterMetrics::RecordStartLocalSessionSuccessful),
      MediaRouterMetrics::kHistogramStartLocalSessionSuccessful);
}

TEST(MediaRouterMetricsTest, RecordStopRoute) {
  base::HistogramTester tester;
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramStopRoute, 0);

  MediaRouterMetrics::RecordStopLocalRoute();
  MediaRouterMetrics::RecordStopRemoteRoute();
  MediaRouterMetrics::RecordStopLocalRoute();

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramStopRoute, 3);
  EXPECT_THAT(tester.GetAllSamples(MediaRouterMetrics::kHistogramStopRoute),
              ElementsAre(Bucket(/* Local route */ 0, 2),
                          Bucket(/* Remote route */ 1, 1)));
}

TEST(MediaRouterMetricsTest, RecordIconStateAtDialogOpen) {
  TestRecordBooleanMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordIconStateAtDialogOpen),
      MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen);
}

TEST(MediaRouterMetricsTest, RecordIconStateAtInit) {
  TestRecordBooleanMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordIconStateAtInit),
      MediaRouterMetrics::kHistogramUiIconStateAtInit);
}

TEST(MediaRouterMetricsTest, RecordCloudPrefAtDialogOpen) {
  TestRecordBooleanMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordCloudPrefAtDialogOpen),
      MediaRouterMetrics::kHistogramCloudPrefAtDialogOpen);
}

TEST(MediaRouterMetricsTest, RecordCloudPrefAtInit) {
  TestRecordBooleanMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordCloudPrefAtInit),
      MediaRouterMetrics::kHistogramCloudPrefAtInit);
}

}  // namespace media_router
