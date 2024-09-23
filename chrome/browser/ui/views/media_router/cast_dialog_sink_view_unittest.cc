// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_sink_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace media_router {
namespace {

UIMediaSink CreateAvailableSink() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_available";
  sink.state = UIMediaSinkState::AVAILABLE;
  sink.cast_modes = {TAB_MIRROR};
  sink.friendly_name = u"Example tv";
  return sink;
}

UIMediaSink CreateNonfreezableSink() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"status text 1";
  sink.cast_modes = {TAB_MIRROR};
  sink.route = MediaRoute("route_id", MediaSource("https://example.com"),
                          sink.id, "", true);
  sink.friendly_name = u"Example tv 1";
  return sink;
}

UIMediaSink CreateFreezableSink() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"status text 2";
  sink.cast_modes = {TAB_MIRROR};
  sink.route = MediaRoute("route_id", MediaSource("https://example.com"),
                          sink.id, "", true);
  sink.freeze_info.can_freeze = true;
  sink.friendly_name = u"Example tv 2";
  return sink;
}

UIMediaSink CreateFreezableFrozenSink() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"status text 3";
  sink.cast_modes = {TAB_MIRROR};
  sink.route = MediaRoute("route_id", MediaSource("https://example.com"),
                          sink.id, "", true);
  sink.freeze_info.can_freeze = true;
  sink.freeze_info.is_frozen = true;
  sink.friendly_name = u"Example tv 3";
  return sink;
}

UIMediaSink CreateFreezableSinkWithTabSource() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"status text 4";
  sink.cast_modes = {TAB_MIRROR};
  sink.route =
      MediaRoute("route_id", MediaSource::ForAnyTab(), sink.id, "", true);
  sink.freeze_info.can_freeze = true;
  sink.friendly_name = u"Example tv 4";
  return sink;
}

UIMediaSink CreateFrozenSinkWithTabSource() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"status text 5";
  sink.cast_modes = {TAB_MIRROR};
  sink.route =
      MediaRoute("route_id", MediaSource::ForAnyTab(), sink.id, "", true);
  sink.freeze_info.can_freeze = true;
  sink.freeze_info.is_frozen = true;
  sink.friendly_name = u"Example tv 5";
  return sink;
}

UIMediaSink CreateFreezableSinkWithDesktopSource() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"status text 6";
  sink.cast_modes = {DESKTOP_MIRROR};
  sink.route = MediaRoute("route_id", MediaSource::ForDesktop("desktop1", true),
                          sink.id, "", true);
  sink.freeze_info.can_freeze = true;
  sink.friendly_name = u"Example tv 6";
  return sink;
}

UIMediaSink CreateFrozenSinkWithDesktopSource() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.status_text = u"status text 7";
  sink.cast_modes = {DESKTOP_MIRROR};
  sink.route = MediaRoute("route_id", MediaSource::ForDesktop("desktop2", true),
                          sink.id, "", true);
  sink.freeze_info.can_freeze = true;
  sink.freeze_info.is_frozen = true;
  sink.friendly_name = u"Example tv 7";
  return sink;
}

}  // namespace

class CastDialogSinkViewTest : public ChromeViewsTestBase {
 public:
  CastDialogSinkViewTest() = default;

  CastDialogSinkViewTest(const CastDialogSinkViewTest&) = delete;
  CastDialogSinkViewTest& operator=(const CastDialogSinkViewTest&) = delete;

  ~CastDialogSinkViewTest() override = default;

 protected:
  TestingProfile profile_;
};

TEST_F(CastDialogSinkViewTest, FreezableSink) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastFreezeUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  UIMediaSink sink_1 = CreateFreezableSink();
  CastDialogSinkView sink_view_1(
      &profile_, sink_1, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  EXPECT_EQ(nullptr, sink_view_1.cast_sink_button_for_test());
  EXPECT_NE(nullptr, sink_view_1.freeze_button_for_test());
  EXPECT_NE(nullptr, sink_view_1.stop_button_for_test());
  EXPECT_NE(nullptr, sink_view_1.title_for_test());
  EXPECT_NE(nullptr, sink_view_1.subtitle_for_test());
  EXPECT_EQ(sink_1.friendly_name, sink_view_1.title_for_test()->GetText());
  EXPECT_EQ(
      sink_1.status_text,
      sink_view_1.subtitle_for_test()->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_VIEW_STOP),
            sink_view_1.stop_button_for_test()->GetText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE),
            sink_view_1.freeze_button_for_test()->GetText());

  UIMediaSink sink_2 = CreateFreezableFrozenSink();
  CastDialogSinkView sink_view_2(
      &profile_, sink_2, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  EXPECT_EQ(nullptr, sink_view_2.cast_sink_button_for_test());
  EXPECT_NE(nullptr, sink_view_2.freeze_button_for_test());
  EXPECT_NE(nullptr, sink_view_2.stop_button_for_test());
  EXPECT_NE(nullptr, sink_view_2.title_for_test());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_PAUSED),
      sink_view_2.subtitle_for_test()->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_VIEW_RESUME),
            sink_view_2.freeze_button_for_test()->GetText());
}

TEST_F(CastDialogSinkViewTest, NonfreezableSink) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastFreezeUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  UIMediaSink sink = CreateNonfreezableSink();
  CastDialogSinkView sink_view(
      &profile_, sink, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  EXPECT_EQ(nullptr, sink_view.cast_sink_button_for_test());
  EXPECT_EQ(nullptr, sink_view.freeze_button_for_test());
  EXPECT_NE(nullptr, sink_view.stop_button_for_test());
  EXPECT_NE(nullptr, sink_view.title_for_test());
  EXPECT_NE(nullptr, sink_view.subtitle_for_test());
}

TEST_F(CastDialogSinkViewTest, SetEnabledState) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastFreezeUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  UIMediaSink sink_1 = CreateAvailableSink();
  CastDialogSinkView sink_view_1(
      &profile_, sink_1, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());

  EXPECT_TRUE(sink_view_1.cast_sink_button_for_test()->GetEnabled());
  sink_view_1.SetEnabledState(false);
  EXPECT_FALSE(sink_view_1.cast_sink_button_for_test()->GetEnabled());
  sink_view_1.SetEnabledState(true);
  EXPECT_TRUE(sink_view_1.cast_sink_button_for_test()->GetEnabled());

  UIMediaSink sink_2 = CreateFreezableSink();
  CastDialogSinkView sink_view_2(
      &profile_, sink_2, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());

  EXPECT_TRUE(sink_view_2.stop_button_for_test()->GetEnabled());
  sink_view_2.SetEnabledState(false);
  EXPECT_FALSE(sink_view_2.stop_button_for_test()->GetEnabled());
  sink_view_2.SetEnabledState(true);
  EXPECT_TRUE(sink_view_2.stop_button_for_test()->GetEnabled());
}

// CastDialogSinkView will show the stop button, but not a freeze button.
TEST_F(CastDialogSinkViewTest, StopButton) {
  UIMediaSink sink_1 = CreateNonfreezableSink();
  CastDialogSinkView sink_view_1(
      &profile_, sink_1, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  EXPECT_EQ(nullptr, sink_view_1.cast_sink_button_for_test());
  EXPECT_EQ(nullptr, sink_view_1.freeze_button_for_test());
  EXPECT_NE(nullptr, sink_view_1.stop_button_for_test());
  EXPECT_NE(nullptr, sink_view_1.title_for_test());
  EXPECT_NE(nullptr, sink_view_1.subtitle_for_test());

  // Even though the sink is freezable, kAccessCodeCastFreezeUI is not enabled
  // so the freeze button should be a nullptr.
  UIMediaSink sink_2 = CreateFreezableSink();
  CastDialogSinkView sink_view_2(
      &profile_, sink_2, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  EXPECT_EQ(nullptr, sink_view_2.cast_sink_button_for_test());
  EXPECT_EQ(nullptr, sink_view_2.freeze_button_for_test());
  EXPECT_NE(nullptr, sink_view_2.stop_button_for_test());
  EXPECT_NE(nullptr, sink_view_2.title_for_test());
  EXPECT_NE(nullptr, sink_view_2.subtitle_for_test());
}

// Tests that the AccessibleName for the freeze and stop buttons are set
// correctly based on source and device name.
TEST_F(CastDialogSinkViewTest, ButtonsAccessibleName) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastFreezeUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  // Create a range of sinks with different sources so we may test for all
  // accessible strings.
  UIMediaSink sink_1 = CreateFreezableSink();
  UIMediaSink sink_2 = CreateFreezableFrozenSink();
  UIMediaSink sink_3 = CreateFreezableSinkWithTabSource();
  UIMediaSink sink_4 = CreateFrozenSinkWithTabSource();
  UIMediaSink sink_5 = CreateFreezableSinkWithDesktopSource();
  UIMediaSink sink_6 = CreateFrozenSinkWithDesktopSource();

  // Create sink views for each sink.
  CastDialogSinkView sink_view_1(
      &profile_, sink_1, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  CastDialogSinkView sink_view_2(
      &profile_, sink_2, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  CastDialogSinkView sink_view_3(
      &profile_, sink_3, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  CastDialogSinkView sink_view_4(
      &profile_, sink_4, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  CastDialogSinkView sink_view_5(
      &profile_, sink_5, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());
  CastDialogSinkView sink_view_6(
      &profile_, sink_6, views::Button::PressedCallback(),
      views::Button::PressedCallback(), views::Button::PressedCallback(),
      views::Button::PressedCallback());

  EXPECT_EQ(sink_view_1.freeze_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE_GENERIC_ACCESSIBLE_NAME,
                sink_1.friendly_name));
  EXPECT_EQ(sink_view_1.stop_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_STOP_GENERIC_ACCESSIBLE_NAME,
                sink_1.friendly_name));
  EXPECT_EQ(sink_view_2.freeze_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_RESUME_GENERIC_ACCESSIBLE_NAME,
                sink_2.friendly_name));
  EXPECT_EQ(sink_view_3.freeze_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE_TAB_ACCESSIBLE_NAME,
                sink_3.friendly_name));
  EXPECT_EQ(sink_view_3.stop_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_STOP_TAB_ACCESSIBLE_NAME,
                sink_3.friendly_name));
  EXPECT_EQ(sink_view_4.freeze_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_RESUME_TAB_ACCESSIBLE_NAME,
                sink_4.friendly_name));
  EXPECT_EQ(sink_view_5.freeze_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE_SCREEN_ACCESSIBLE_NAME,
                sink_5.friendly_name));
  EXPECT_EQ(sink_view_5.stop_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_STOP_SCREEN_ACCESSIBLE_NAME,
                sink_5.friendly_name));
  EXPECT_EQ(sink_view_6.freeze_button_for_test()
                ->GetViewAccessibility()
                .GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_MEDIA_ROUTER_SINK_VIEW_RESUME_SCREEN_ACCESSIBLE_NAME,
                sink_6.friendly_name));
}

}  // namespace media_router
