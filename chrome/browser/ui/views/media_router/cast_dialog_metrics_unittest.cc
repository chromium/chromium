// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom-shared.h"
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
const base::Time paint_time = init_time + base::Milliseconds(50);
const base::Time sink_load_time = init_time + base::Milliseconds(300);
const base::Time start_casting_time = init_time + base::Milliseconds(2000);
const base::Time close_dialog_time = init_time + base::Milliseconds(3000);

}  // namespace

class CastDialogMetricsTest : public ChromeViewsTestBase {
 public:
  CastDialogMetricsTest() = default;
  ~CastDialogMetricsTest() override = default;

 protected:
  TestingProfile profile_;
  base::HistogramTester tester_;
  CastDialogMetrics metrics_{
      init_time, MediaRouterDialogActivationLocation::TOOLBAR, &profile_};
};

TEST_F(CastDialogMetricsTest, OnSinksLoaded) {
  metrics_.OnSinksLoaded(sink_load_time);
  tester_.ExpectUniqueSample(
      MediaRouterMetrics::kHistogramUiCastDialogLoadedWithData,
      (sink_load_time - init_time).InMilliseconds(), 1);
}

TEST_F(CastDialogMetricsTest, OnStartCasting) {
  metrics_.OnSinksLoaded(sink_load_time);
  metrics_.OnStartCasting(TAB_MIRROR, SinkIconType::CAST);
  tester_.ExpectUniqueSample("MediaRouter.Sink.SelectedType.CastHarmony",
                             SinkIconType::CAST, 1);
}

TEST_F(CastDialogMetricsTest, OnRecordSinkCount) {
  UIMediaSink sink1{mojom::MediaRouteProviderId::CAST};
  UIMediaSink sink2{mojom::MediaRouteProviderId::CAST};
  UIMediaSink sink3{mojom::MediaRouteProviderId::DIAL};
  CastDialogSinkButton button1{views::Button::PressedCallback(), sink1};
  CastDialogSinkButton button2{views::Button::PressedCallback(), sink2};
  CastDialogSinkButton button3{views::Button::PressedCallback(), sink3};
  std::vector<CastDialogSinkButton*> buttons{&button1, &button2, &button3};
  metrics_.OnRecordSinkCount(buttons);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramUiDeviceCount,
                             buttons.size(), 1);
}

TEST_F(CastDialogMetricsTest, OnRecordSinkCountSinkView) {
  UIMediaSink sink1{mojom::MediaRouteProviderId::CAST};
  UIMediaSink sink2{mojom::MediaRouteProviderId::CAST};
  UIMediaSink sink3{mojom::MediaRouteProviderId::DIAL};
  CastDialogSinkView sink_view1{&profile_,
                                sink1,
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback()};
  CastDialogSinkView sink_view2{&profile_,
                                sink2,
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback()};
  CastDialogSinkView sink_view3{&profile_,
                                sink3,
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback(),
                                views::Button::PressedCallback()};
  // The vector below doesn't contain any dangling pointers, but adding
  // `DanglingUntriaged` was necessary to match `OnRecordSinkCount` signature.
  std::vector<raw_ptr<CastDialogSinkView, DanglingUntriaged>> sink_views{
      &sink_view1, &sink_view2, &sink_view3};
  metrics_.OnRecordSinkCount(sink_views);
  tester_.ExpectUniqueSample(MediaRouterMetrics::kHistogramUiDeviceCount,
                             sink_views.size(), 1);
}

TEST_F(CastDialogMetricsTest, RecordIconState) {
  tester_.ExpectUniqueSample(
      MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen,
      /* is_pinned */ false, 1);

  profile_.GetPrefs()->SetBoolean(::prefs::kShowCastIconInToolbar, true);
  CastDialogMetrics metrics_with_pinned_icon{
      init_time, MediaRouterDialogActivationLocation::PAGE, &profile_};
  tester_.ExpectBucketCount(
      MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen,
      /* is_pinned */ true, 1);
}

TEST_F(CastDialogMetricsTest, RecordDialogActivationLocationAndCastMode) {
  metrics_.OnSinksLoaded(sink_load_time);
  metrics_.OnStartCasting(TAB_MIRROR, SinkIconType::CAST);
  tester_.ExpectUniqueSample(
      "MediaRouter.Ui.Dialog.ActivationLocationAndCastMode",
      DialogActivationLocationAndCastMode::kEphemeralIconAndTabMirror, 1);

  CastDialogMetrics metrics_opened_from_page{
      init_time, MediaRouterDialogActivationLocation::PAGE, &profile_};
  metrics_opened_from_page.OnSinksLoaded(sink_load_time);
  metrics_opened_from_page.OnStartCasting(PRESENTATION, SinkIconType::GENERIC);
  tester_.ExpectBucketCount(
      "MediaRouter.Ui.Dialog.ActivationLocationAndCastMode",
      DialogActivationLocationAndCastMode::kPageAndPresentation, 1);

  profile_.GetPrefs()->SetBoolean(::prefs::kShowCastIconInToolbar, true);
  CastDialogMetrics metrics_with_pinned_icon{
      init_time, MediaRouterDialogActivationLocation::TOOLBAR, &profile_};
  metrics_with_pinned_icon.OnSinksLoaded(sink_load_time);
  metrics_with_pinned_icon.OnStartCasting(DESKTOP_MIRROR, SinkIconType::CAST);
  tester_.ExpectBucketCount(
      "MediaRouter.Ui.Dialog.ActivationLocationAndCastMode",
      DialogActivationLocationAndCastMode::kPinnedIconAndDesktopMirror, 1);
}

}  // namespace media_router
