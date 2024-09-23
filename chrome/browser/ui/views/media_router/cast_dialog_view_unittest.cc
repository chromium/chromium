// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::NiceMock;
using testing::WithArg;

namespace media_router {

namespace {

UIMediaSink CreateAvailableSink() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_available";
  sink.state = UIMediaSinkState::AVAILABLE;
  sink.cast_modes = {TAB_MIRROR};
  return sink;
}

UIMediaSink CreateConnectedSink() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.cast_modes = {TAB_MIRROR};
  sink.route = MediaRoute("route_id", MediaSource("https://example.com"),
                          sink.id, "", true);
  return sink;
}

UIMediaSink CreateFreezableSink() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_freezable";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.cast_modes = {TAB_MIRROR};
  sink.route =
      MediaRoute("route_id_freezable", MediaSource("https://example.com"),
                 sink.id, "", true);
  sink.freeze_info.can_freeze = true;
  return sink;
}

UIMediaSink CreateFreezableFrozenSink() {
  UIMediaSink sink{mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_freezable_frozen";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.cast_modes = {TAB_MIRROR};
  sink.route =
      MediaRoute("route_id_freezable_frozen",
                 MediaSource("https://example.com"), sink.id, "", true);
  sink.freeze_info.can_freeze = true;
  sink.freeze_info.is_frozen = true;
  return sink;
}

CastDialogModel CreateModelWithSinks(std::vector<UIMediaSink> sinks) {
  CastDialogModel model;
  model.set_dialog_header(u"Dialog header");
  model.set_media_sinks(std::move(sinks));
  return model;
}

ui::MouseEvent CreateMouseEvent() {
  return ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(0, 0),
                        gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
}

}  // namespace

class MockCastDialogController : public CastDialogController {
 public:
  MOCK_METHOD(void,
              AddObserver,
              (CastDialogController::Observer * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (CastDialogController::Observer * observer),
              (override));
  MOCK_METHOD(void,
              StartCasting,
              (const std::string& sink_id, MediaCastMode cast_mode),
              (override));
  MOCK_METHOD(void, StopCasting, (const std::string& route_id), (override));
  MOCK_METHOD(void, ClearIssue, (const Issue::Id& issue_id), (override));
  MOCK_METHOD(void, FreezeRoute, (const std::string& route_id), (override));
  MOCK_METHOD(void, UnfreezeRoute, (const std::string& route_id), (override));
  MOCK_METHOD(std::unique_ptr<MediaRouteStarter>,
              TakeMediaRouteStarter,
              (),
              (override));
  MOCK_METHOD(void, RegisterDestructor, (base::OnceClosure), (override));
};

class CastDialogViewTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the dialog.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void InitializeDialogWithModel(const CastDialogModel& model) {
    EXPECT_CALL(controller_, AddObserver(_));
    cast_dialog_coordinator_.Show(anchor_widget_->GetContentsView(),
                                  views::BubbleBorder::TOP_RIGHT, &controller_,
                                  &profile_, base::Time::Now(),
                                  MediaRouterDialogActivationLocation::PAGE);

    dialog_ = cast_dialog_coordinator_.GetCastDialogView();
    dialog_->OnModelUpdated(model);
  }

  void SinkPressedAtIndex(int index) {
    NotifyButtonOfClick(sink_views().at(index)->cast_sink_button_for_test());
  }

  void StopPressedAtIndex(int index) {
    NotifyButtonOfClick(sink_views().at(index)->stop_button_for_test());
  }

  void FreezePressedAtIndex(int index) {
    NotifyButtonOfClick(sink_views().at(index)->freeze_button_for_test());
  }

  void NotifyButtonOfClick(views::Button* button) {
    ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(0, 0),
                               gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(mouse_event);
    // The request to cast/stop is sent asynchronously, so we must call
    // RunUntilIdle().
    base::RunLoop().RunUntilIdle();
  }

  const std::vector<raw_ptr<CastDialogSinkView, DanglingUntriaged>>&
  sink_views() {
    return dialog_->sink_views_for_test();
  }

  views::ScrollView* scroll_view() { return dialog_->scroll_view_for_test(); }

  views::View* no_sinks_view() { return dialog_->no_sinks_view_for_test(); }

  views::View* permission_rejected_view() {
    return dialog_->permission_rejected_view_for_test();
  }

  views::Button* sources_button() { return dialog_->sources_button_for_test(); }

  HoverButton* access_code_cast_button() {
    return dialog_->access_code_cast_button_for_test();
  }

  ui::SimpleMenuModel* sources_menu_model() {
    return dialog_->sources_menu_model_for_test();
  }

  views::MenuRunner* sources_menu_runner() {
    return dialog_->sources_menu_runner_for_test();
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  NiceMock<MockCastDialogController> controller_;
  CastDialogCoordinator cast_dialog_coordinator_;
  raw_ptr<CastDialogView, DanglingUntriaged> dialog_ = nullptr;
  TestingProfile profile_;
};

TEST_F(CastDialogViewTest, PopulateDialog) {
  CastDialogModel model = CreateModelWithSinks({CreateAvailableSink()});
  InitializeDialogWithModel(model);

  EXPECT_TRUE(dialog_->ShouldShowCloseButton());
  EXPECT_EQ(model.dialog_header(), dialog_->GetWindowTitle());
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kNone),
            dialog_->buttons());
}

TEST_F(CastDialogViewTest, StartCasting) {
  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink(),
                                          CreateAvailableSink()};
  media_sinks[0].id = "sink0";
  media_sinks[1].id = "sink1";
  CastDialogModel model = CreateModelWithSinks(std::move(media_sinks));
  InitializeDialogWithModel(model);

  EXPECT_CALL(controller_, StartCasting(model.media_sinks()[0].id, TAB_MIRROR));
  SinkPressedAtIndex(0);
}

TEST_F(CastDialogViewTest, FreezeUiStartCasting) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastFreezeUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink(),
                                          CreateAvailableSink()};
  media_sinks[0].id = "sink0";
  media_sinks[1].id = "sink1";
  CastDialogModel model = CreateModelWithSinks(std::move(media_sinks));
  InitializeDialogWithModel(model);

  EXPECT_CALL(controller_, StartCasting(model.media_sinks()[0].id, TAB_MIRROR));
  SinkPressedAtIndex(0);
}

TEST_F(CastDialogViewTest, StopCasting) {
  CastDialogModel model =
      CreateModelWithSinks({CreateAvailableSink(), CreateConnectedSink()});
  InitializeDialogWithModel(model);
  EXPECT_CALL(controller_,
              StopCasting(model.media_sinks()[1].route->media_route_id()));
  StopPressedAtIndex(1);
}

TEST_F(CastDialogViewTest, FreezeRoute) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastFreezeUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  CastDialogModel model = CreateModelWithSinks(
      {CreateFreezableSink(), CreateFreezableFrozenSink()});
  InitializeDialogWithModel(model);

  EXPECT_CALL(controller_,
              FreezeRoute(model.media_sinks()[0].route->media_route_id()))
      .Times(1);
  EXPECT_CALL(controller_,
              UnfreezeRoute(model.media_sinks()[1].route->media_route_id()))
      .Times(1);
  FreezePressedAtIndex(0);
  FreezePressedAtIndex(1);
}

TEST_F(CastDialogViewTest, FreezeNoRoute) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastFreezeUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  CastDialogModel model = CreateModelWithSinks({CreateFreezableSink()});
  InitializeDialogWithModel(model);

  // Now that the CastDialogSinkView has been created with the proper buttons,
  // change the associated sink. This simulates an edge case where a sink may
  // change status while the CastDialogSinkView has not been destroyed.
  sink_views().at(0)->set_sink_for_test(CreateAvailableSink());

  // When there is no associated route, FreezePressed should be a no-op.
  EXPECT_CALL(controller_,
              FreezeRoute(model.media_sinks()[0].route->media_route_id()))
      .Times(0);
  EXPECT_CALL(controller_,
              UnfreezeRoute(model.media_sinks()[0].route->media_route_id()))
      .Times(0);
  FreezePressedAtIndex(0);
}

TEST_F(CastDialogViewTest, ClearIssue) {
  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink()};
  media_sinks[0].issue = Issue::CreateIssueWithIssueInfo(
      IssueInfo("title", IssueInfo::Severity::WARNING, "sinkId1"));
  CastDialogModel model = CreateModelWithSinks(std::move(media_sinks));
  InitializeDialogWithModel(model);
  // When there is an issue, clicking on an available sink should clear the
  // issue instead of starting casting.
  EXPECT_CALL(controller_, StartCasting(_, _)).Times(0);
  EXPECT_CALL(controller_, ClearIssue(model.media_sinks()[0].issue->id()));
  SinkPressedAtIndex(0);
}

TEST_F(CastDialogViewTest, ShowSourcesMenu) {
  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink()};
  media_sinks[0].cast_modes = {TAB_MIRROR, PRESENTATION, DESKTOP_MIRROR};
  CastDialogModel model = CreateModelWithSinks(media_sinks);
  InitializeDialogWithModel(model);
  // Press the button to show the sources menu.
  views::test::ButtonTestApi(sources_button()).NotifyClick(CreateMouseEvent());
  // The items should be "tab" (includes tab mirroring and presentation) and
  // "desktop".
  EXPECT_EQ(2u, sources_menu_model()->GetItemCount());
  EXPECT_EQ(CastDialogView::kTab, sources_menu_model()->GetCommandIdAt(0));
  EXPECT_EQ(CastDialogView::kDesktop, sources_menu_model()->GetCommandIdAt(1));

  // When there are no sinks, the sources button should be disabled.
  model.set_media_sinks({});
  dialog_->OnModelUpdated(model);
  EXPECT_FALSE(sources_button()->GetEnabled());
}

TEST_F(CastDialogViewTest, CastAlternativeSources) {
  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink()};
  media_sinks[0].cast_modes = {TAB_MIRROR, DESKTOP_MIRROR};
  CastDialogModel model = CreateModelWithSinks(std::move(media_sinks));
  InitializeDialogWithModel(model);
  // Press the button to show the sources menu.
  views::test::ButtonTestApi(sources_button()).NotifyClick(CreateMouseEvent());
  // There should be two sources: tab and desktop.
  ASSERT_EQ(2u, sources_menu_model()->GetItemCount());

  EXPECT_CALL(controller_, StartCasting(model.media_sinks()[0].id, TAB_MIRROR));
  sources_menu_model()->ActivatedAt(0);
  SinkPressedAtIndex(0);
  Mock::VerifyAndClearExpectations(&controller_);

  EXPECT_CALL(controller_,
              StartCasting(model.media_sinks()[0].id, DESKTOP_MIRROR));
  sources_menu_model()->ActivatedAt(1);
  SinkPressedAtIndex(0);
}

TEST_F(CastDialogViewTest, DisableUnsupportedSinks) {
  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink(),
                                          CreateAvailableSink()};
  media_sinks[1].id = "sink_2";
  media_sinks[0].cast_modes = {TAB_MIRROR};
  media_sinks[1].cast_modes = {PRESENTATION, DESKTOP_MIRROR};
  CastDialogModel model = CreateModelWithSinks(std::move(media_sinks));
  InitializeDialogWithModel(model);

  views::test::ButtonTestApi test_api(sources_button());
  test_api.NotifyClick(CreateMouseEvent());
  EXPECT_EQ(CastDialogView::kDesktop, sources_menu_model()->GetCommandIdAt(1));
  sources_menu_model()->ActivatedAt(1);
  // Sink at index 0 doesn't support desktop mirroring, so it should be
  // disabled.
  EXPECT_FALSE(sink_views().at(0)->cast_sink_button_for_test()->GetEnabled());
  EXPECT_TRUE(sink_views().at(1)->cast_sink_button_for_test()->GetEnabled());

  test_api.NotifyClick(CreateMouseEvent());
  EXPECT_EQ(CastDialogView::kTab, sources_menu_model()->GetCommandIdAt(0));
  sources_menu_model()->ActivatedAt(0);
  // Both sinks support tab or presentation casting, so they should be enabled.
  EXPECT_TRUE(sink_views().at(0)->cast_sink_button_for_test()->GetEnabled());
  EXPECT_TRUE(sink_views().at(1)->cast_sink_button_for_test()->GetEnabled());
}

TEST_F(CastDialogViewTest, ShowNoDeviceView) {
  CastDialogModel model;
  InitializeDialogWithModel(model);
  // The no-device view should be shown when there are no sinks.
  EXPECT_TRUE(no_sinks_view()->GetVisible());
  EXPECT_FALSE(scroll_view());

  std::vector<UIMediaSink> media_sinks = {CreateConnectedSink()};
  model.set_media_sinks(std::move(media_sinks));
  dialog_->OnModelUpdated(model);
  // The scroll view should be shown when there are sinks.
  EXPECT_FALSE(no_sinks_view());
  EXPECT_TRUE(scroll_view()->GetVisible());
}

TEST_F(CastDialogViewTest, SwitchToNoDeviceView) {
  // Start with one sink. The sink list scroll view should be shown.
  CastDialogModel model = CreateModelWithSinks({CreateAvailableSink()});
  InitializeDialogWithModel(model);
  EXPECT_TRUE(scroll_view()->GetVisible());
  EXPECT_FALSE(no_sinks_view());

  // Remove the sink. The no-device view should be shown.
  model.set_media_sinks({});
  dialog_->OnModelUpdated(model);
  EXPECT_TRUE(no_sinks_view()->GetVisible());
  EXPECT_FALSE(scroll_view());
}

TEST_F(CastDialogViewTest, ShowPermissionRejectedView) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media_router::kShowCastPermissionRejectedError);

  // No sink, permission pending/granted.
  CastDialogModel model;
  InitializeDialogWithModel(model);
  EXPECT_TRUE(no_sinks_view());
  EXPECT_FALSE(scroll_view());
  EXPECT_FALSE(permission_rejected_view());

  // No sink, permission rejected.
  model.set_is_permission_rejected(true);
  InitializeDialogWithModel(model);
  EXPECT_FALSE(no_sinks_view());
  EXPECT_FALSE(scroll_view());
  EXPECT_TRUE(permission_rejected_view() &&
              permission_rejected_view()->GetVisible());
}

TEST_F(CastDialogViewTest, ShowAccessCodeCastButtonDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, false);

  CastDialogModel model = CreateModelWithSinks({CreateAvailableSink()});
  InitializeDialogWithModel(model);
  EXPECT_FALSE(access_code_cast_button());
}

TEST_F(CastDialogViewTest, ShowAccessCodeCastButtonEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  CastDialogModel model = CreateModelWithSinks({CreateAvailableSink()});
  InitializeDialogWithModel(model);

  EXPECT_TRUE(access_code_cast_button());
}

// This test demonstrates that when the access code casting feature is
// available to the user, that the sources button is available even if no
// sinks are available.
TEST_F(CastDialogViewTest, AccessCodeEmptySinksSourcesAvailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastUI);
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, false);

  CastDialogModel model;
  InitializeDialogWithModel(model);

  // With policy disabled, button is still disabled even with feature enabled.
  EXPECT_FALSE(sources_button()->GetEnabled());

  // But with policy enabled, button is now enabled even with no sinks.
  profile_.GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);
  dialog_->OnModelUpdated(model);
  EXPECT_TRUE(sources_button()->GetEnabled());
}

}  // namespace media_router
