// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_metrics.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
  const base::TimeDelta delta = base::Milliseconds(10);

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

// Tests that |record_cb| records metrics for each MediaRouteProvider in a
// histogram specific to the provider.
void TestRouteResultCodeHistogramsWithProviders(
    base::RepeatingCallback<void(mojom::RouteRequestResultCode,
                                 std::optional<mojom::MediaRouteProviderId>)>
        record_cb,
    mojom::MediaRouteProviderId provider1,
    const std::string& histogram_provider1,
    mojom::MediaRouteProviderId provider2,
    const std::string& histogram_provider2) {
  base::HistogramTester tester;
  tester.ExpectTotalCount(histogram_provider1, 0);
  tester.ExpectTotalCount(histogram_provider2, 0);

  record_cb.Run(mojom::RouteRequestResultCode::SINK_NOT_FOUND, provider1);
  record_cb.Run(mojom::RouteRequestResultCode::OK, provider2);
  record_cb.Run(mojom::RouteRequestResultCode::SINK_NOT_FOUND, provider1);
  record_cb.Run(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND, provider2);
  record_cb.Run(mojom::RouteRequestResultCode::OK, provider1);

  tester.ExpectTotalCount(histogram_provider1, 3);
  EXPECT_THAT(
      tester.GetAllSamples(histogram_provider1),
      ElementsAre(
          Bucket(static_cast<int>(mojom::RouteRequestResultCode::OK), 1),
          Bucket(
              static_cast<int>(mojom::RouteRequestResultCode::SINK_NOT_FOUND),
              2)));

  tester.ExpectTotalCount(histogram_provider2, 2);
  EXPECT_THAT(
      tester.GetAllSamples(histogram_provider2),
      ElementsAre(
          Bucket(static_cast<int>(mojom::RouteRequestResultCode::OK), 1),
          Bucket(
              static_cast<int>(mojom::RouteRequestResultCode::ROUTE_NOT_FOUND),
              1)));
}

void TestRouteResultCodeHistograms(
    base::RepeatingCallback<void(mojom::RouteRequestResultCode,
                                 std::optional<mojom::MediaRouteProviderId>)>
        record_cb,
    const std::string& base_histogram_name) {
  TestRouteResultCodeHistogramsWithProviders(
      record_cb, mojom::MediaRouteProviderId::WIRED_DISPLAY,
      base_histogram_name + ".WiredDisplay", mojom::MediaRouteProviderId::DIAL,
      base_histogram_name + ".DIAL");

  TestRouteResultCodeHistogramsWithProviders(
      record_cb, mojom::MediaRouteProviderId::CAST,
      base_histogram_name + ".Cast", mojom::MediaRouteProviderId::ANDROID_CAF,
      base_histogram_name + ".AndroidCaf");
}

}  // namespace

TEST(MediaRouterMetricsTest, RecordMediaRouterDialogActivationLocation) {
  base::HistogramTester tester;
  const MediaRouterDialogActivationLocation activation_location1 =
      MediaRouterDialogActivationLocation::TOOLBAR;
  const MediaRouterDialogActivationLocation activation_location2 =
      MediaRouterDialogActivationLocation::CONTEXTUAL_MENU;

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramIconClickLocation, 0);
  MediaRouterMetrics::RecordMediaRouterDialogActivationLocation(
      activation_location1);
  MediaRouterMetrics::RecordMediaRouterDialogActivationLocation(
      activation_location2);
  MediaRouterMetrics::RecordMediaRouterDialogActivationLocation(
      activation_location1);
  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramIconClickLocation, 3);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramIconClickLocation),
      ElementsAre(Bucket(static_cast<int>(activation_location1), 2),
                  Bucket(static_cast<int>(activation_location2), 1)));
}

TEST(MediaRouterMetricsTest, RecordMediaRouterDialogLoaded) {
  TestRecordTimeDeltaMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordCastDialogLoaded),
      MediaRouterMetrics::kHistogramUiCastDialogLoadedWithData);
  TestRecordTimeDeltaMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordGmcDialogLoaded),
      MediaRouterMetrics::kHistogramUiGmcDialogLoadedWithData);
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
  MediaRouterMetrics::RecordMediaSinkType(SinkIconType::CAST);
  MediaRouterMetrics::RecordMediaSinkType(SinkIconType::GENERIC);

  tester.ExpectTotalCount(MediaRouterMetrics::kHistogramMediaSinkType, 5);
  EXPECT_THAT(
      tester.GetAllSamples(MediaRouterMetrics::kHistogramMediaSinkType),
      ElementsAre(Bucket(static_cast<int>(SinkIconType::CAST), 2),
                  Bucket(static_cast<int>(SinkIconType::CAST_AUDIO), 1),
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

TEST(MediaRouterMetricsTest, RecordIconStateAtDialogOpen) {
  TestRecordBooleanMetric(
      base::BindRepeating(&MediaRouterMetrics::RecordIconStateAtDialogOpen),
      MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen);
}

TEST(MediaRouterMetricsTest, RecordCreateRouteResultCode) {
  TestRouteResultCodeHistograms(
      base::BindRepeating(&MediaRouterMetrics::RecordCreateRouteResultCode),
      "MediaRouter.Provider.CreateRoute.Result");
}

TEST(MediaRouterMetricsTest, RecordJoinRouteResultCode) {
  TestRouteResultCodeHistograms(
      base::BindRepeating(&MediaRouterMetrics::RecordJoinRouteResultCode),
      "MediaRouter.Provider.JoinRoute.Result");
}

TEST(MediaRouterMetricsTest, RecordTerminateRouteResultCode) {
  TestRouteResultCodeHistograms(
      base::BindRepeating(
          &MediaRouterMetrics::RecordMediaRouteProviderTerminateRoute),
      "MediaRouter.Provider.TerminateRoute.Result");
}

}  // namespace media_router
