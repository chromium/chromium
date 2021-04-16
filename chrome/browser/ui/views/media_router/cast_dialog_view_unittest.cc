// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::WithArg;

namespace media_router {

namespace {

UIMediaSink CreateAvailableSink() {
  UIMediaSink sink;
  sink.id = "sink_available";
  sink.state = UIMediaSinkState::AVAILABLE;
  sink.cast_modes = {TAB_MIRROR};
  return sink;
}

UIMediaSink CreateConnectedSink() {
  UIMediaSink sink;
  sink.id = "sink_connected";
  sink.state = UIMediaSinkState::CONNECTED;
  sink.cast_modes = {TAB_MIRROR};
  sink.route = MediaRoute("route_id", MediaSource("https://example.com"),
                          sink.id, "", true, true);
  return sink;
}

CastDialogModel CreateModelWithSinks(std::vector<UIMediaSink> sinks) {
  CastDialogModel model;
  model.set_dialog_header(u"Dialog header");
  model.set_media_sinks(std::move(sinks));
  return model;
}

ui::MouseEvent CreateMouseEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                        gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
}

}  // namespace

class MockCastDialogController : public CastDialogController {
 public:
  MOCK_METHOD1(AddObserver, void(CastDialogController::Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(CastDialogController::Observer* observer));
  MOCK_METHOD2(StartCasting,
               void(const std::string& sink_id, MediaCastMode cast_mode));
  MOCK_METHOD1(StopCasting, void(const std::string& route_id));
  MOCK_METHOD1(
      ChooseLocalFile,
      void(base::OnceCallback<void(const ui::SelectedFileInfo*)> callback));
  MOCK_METHOD1(ClearIssue, void(const Issue::Id& issue_id));
};

class CastDialogViewTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the dialog.
    anchor_widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void InitializeDialogWithModel(const CastDialogModel& model) {
    EXPECT_CALL(controller_, AddObserver(_))
        .WillOnce(
            WithArg<0>(Invoke([this](CastDialogController::Observer* observer) {
              dialog_ = static_cast<CastDialogView*>(observer);
            })));
    CastDialogView::ShowDialog(anchor_widget_->GetContentsView(),
                               views::BubbleBorder::TOP_RIGHT, &controller_,
                               &profile_, base::Time::Now(),
                               MediaRouterDialogOpenOrigin::PAGE);

    dialog_->OnModelUpdated(model);
  }

  void SinkPressedAtIndex(int index) {
    ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                               gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(sink_buttons().at(index))
        .NotifyClick(mouse_event);
    // The request to cast/stop is sent asynchronously, so we must call
    // RunUntilIdle().
    base::RunLoop().RunUntilIdle();
  }

  const std::vector<CastDialogSinkButton*>& sink_buttons() {
    return dialog_->sink_buttons_for_test();
  }

  views::ScrollView* scroll_view() { return dialog_->scroll_view_for_test(); }

  views::View* no_sinks_view() { return dialog_->no_sinks_view_for_test(); }

  views::Button* sources_button() { return dialog_->sources_button_for_test(); }

  ui::SimpleMenuModel* sources_menu_model() {
    return dialog_->sources_menu_model_for_test();
  }

  views::MenuRunner* sources_menu_runner() {
    return dialog_->sources_menu_runner_for_test();
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  MockCastDialogController controller_;
  CastDialogView* dialog_ = nullptr;
  TestingProfile profile_;
};

TEST_F(CastDialogViewTest, ShowAndHideDialog) {
  EXPECT_FALSE(CastDialogView::IsShowing());
  EXPECT_EQ(nullptr, CastDialogView::GetCurrentDialogWidget());

  EXPECT_CALL(controller_, AddObserver(_));
  CastDialogView::ShowDialog(anchor_widget_->GetContentsView(),
                             views::BubbleBorder::TOP_RIGHT, &controller_,
                             &profile_, base::Time::Now(),
                             MediaRouterDialogOpenOrigin::PAGE);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(CastDialogView::IsShowing());
  EXPECT_NE(nullptr, CastDialogView::GetCurrentDialogWidget());

  EXPECT_CALL(controller_, RemoveObserver(_));
  CastDialogView::HideDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(CastDialogView::IsShowing());
  EXPECT_EQ(nullptr, CastDialogView::GetCurrentDialogWidget());
}

TEST_F(CastDialogViewTest, PopulateDialog) {
  CastDialogModel model = CreateModelWithSinks({CreateAvailableSink()});
  InitializeDialogWithModel(model);

  EXPECT_TRUE(dialog_->ShouldShowCloseButton());
  EXPECT_EQ(model.dialog_header(), dialog_->GetWindowTitle());
  EXPECT_EQ(ui::DIALOG_BUTTON_NONE, dialog_->GetDialogButtons());
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

TEST_F(CastDialogViewTest, StopCasting) {
  CastDialogModel model =
      CreateModelWithSinks({CreateAvailableSink(), CreateConnectedSink()});
  InitializeDialogWithModel(model);
  EXPECT_CALL(controller_,
              StopCasting(model.media_sinks()[1].route->media_route_id()));
  SinkPressedAtIndex(1);
}

TEST_F(CastDialogViewTest, ClearIssue) {
  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink()};
  media_sinks[0].issue = Issue(IssueInfo("title", IssueInfo::Action::DISMISS,
                                         IssueInfo::Severity::WARNING));
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
  // The items should be "tab" (includes tab mirroring and presentation),
  // "desktop", and "local file".
  EXPECT_EQ(3, sources_menu_model()->GetItemCount());
  EXPECT_EQ(CastDialogView::kTab, sources_menu_model()->GetCommandIdAt(0));
  EXPECT_EQ(CastDialogView::kDesktop, sources_menu_model()->GetCommandIdAt(1));
  EXPECT_EQ(CastDialogView::kLocalFile,
            sources_menu_model()->GetCommandIdAt(2));

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
  // There should be three sources: tab, desktop, and local file.
  ASSERT_EQ(3, sources_menu_model()->GetItemCount());

  EXPECT_CALL(controller_, StartCasting(model.media_sinks()[0].id, TAB_MIRROR));
  sources_menu_model()->ActivatedAt(0);
  SinkPressedAtIndex(0);
  Mock::VerifyAndClearExpectations(&controller_);

  EXPECT_CALL(controller_,
              StartCasting(model.media_sinks()[0].id, DESKTOP_MIRROR));
  sources_menu_model()->ActivatedAt(1);
  SinkPressedAtIndex(0);
}

TEST_F(CastDialogViewTest, CastLocalFile) {
  const std::string file_name = "example.mp4";
  const std::string file_path = "path/to/" + file_name;
  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink()};
  media_sinks[0].cast_modes = {TAB_MIRROR, LOCAL_FILE};
  CastDialogModel model = CreateModelWithSinks(std::move(media_sinks));
  InitializeDialogWithModel(model);
  views::test::ButtonTestApi(sources_button()).NotifyClick(CreateMouseEvent());

#if defined(OS_WIN)
  ui::SelectedFileInfo file_info{base::FilePath(base::UTF8ToWide(file_name)),
                                 base::FilePath(base::UTF8ToWide(file_path))};
#else
  ui::SelectedFileInfo file_info{base::FilePath(file_name),
                                 base::FilePath(file_path)};
#endif  // defined(OS_WIN)
  EXPECT_CALL(controller_, ChooseLocalFile(_))
      .WillOnce(
          [file_info](base::OnceCallback<void(const ui::SelectedFileInfo*)>
                          file_callback) {
            std::move(file_callback).Run(&file_info);
          });
  ASSERT_EQ(CastDialogView::kLocalFile,
            sources_menu_model()->GetCommandIdAt(2));
  sources_menu_model()->ActivatedAt(2);
  EXPECT_EQ(dialog_->GetWindowTitle(),
            l10n_util::GetStringFUTF16(IDS_MEDIA_ROUTER_CAST_LOCAL_MEDIA_TITLE,
                                       base::UTF8ToUTF16(file_name)));

  EXPECT_CALL(controller_, StartCasting(model.media_sinks()[0].id, LOCAL_FILE));
  SinkPressedAtIndex(0);
}

TEST_F(CastDialogViewTest, CancelLocalFileSelection) {
  std::vector<UIMediaSink> media_sinks = {CreateAvailableSink()};
  media_sinks[0].cast_modes = {TAB_MIRROR, LOCAL_FILE};
  CastDialogModel model = CreateModelWithSinks(std::move(media_sinks));
  InitializeDialogWithModel(model);
  views::test::ButtonTestApi(sources_button()).NotifyClick(CreateMouseEvent());

  // The tab source should be selected by default.
  ASSERT_EQ(CastDialogView::kTab, sources_menu_model()->GetCommandIdAt(0));
  ASSERT_TRUE(sources_menu_model()->IsItemCheckedAt(0));

  // Select the local file source, then cancel file selection by passing a
  // nullptr into the callback.
  EXPECT_CALL(controller_, ChooseLocalFile(_))
      .WillOnce(
          [](base::OnceCallback<void(const ui::SelectedFileInfo*)>
                 file_callback) { std::move(file_callback).Run(nullptr); });
  ASSERT_EQ(CastDialogView::kLocalFile,
            sources_menu_model()->GetCommandIdAt(2));
  sources_menu_model()->ActivatedAt(2);

  // Since we cancelled file selection, "tab" should still be the selected
  // source.
  EXPECT_TRUE(sources_menu_model()->IsItemCheckedAt(0));
  EXPECT_CALL(controller_, StartCasting(model.media_sinks()[0].id, TAB_MIRROR));
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
  EXPECT_FALSE(sink_buttons().at(0)->GetEnabled());
  EXPECT_TRUE(sink_buttons().at(1)->GetEnabled());

  test_api.NotifyClick(CreateMouseEvent());
  EXPECT_EQ(CastDialogView::kTab, sources_menu_model()->GetCommandIdAt(0));
  sources_menu_model()->ActivatedAt(0);
  // Both sinks support tab or presentation casting, so they should be enabled.
  EXPECT_TRUE(sink_buttons().at(0)->GetEnabled());
  EXPECT_TRUE(sink_buttons().at(1)->GetEnabled());
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

}  // namespace media_router
