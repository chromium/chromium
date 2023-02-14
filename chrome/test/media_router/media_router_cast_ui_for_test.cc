// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_cast_ui_for_test.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/test/button_test_api.h"

namespace media_router {

namespace {

ui::MouseEvent CreateMousePressedEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                        gfx::Point(0, 0), ui::EventTimeForNow(),
                        ui::EF_LEFT_MOUSE_BUTTON, 0);
}

}  // namespace

// static
MediaRouterCastUiForTest* MediaRouterCastUiForTest::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  // No-op if an instance already exists for the WebContents.
  MediaRouterCastUiForTest::CreateForWebContents(web_contents);
  return MediaRouterCastUiForTest::FromWebContents(web_contents);
}

MediaRouterCastUiForTest::~MediaRouterCastUiForTest() {
  CHECK(!watch_callback_);
}

void MediaRouterCastUiForTest::SetUp() {
  feature_list_.InitAndDisableFeature(kGlobalMediaControlsCastStartStop);
}

void MediaRouterCastUiForTest::ShowDialog() {
  dialog_controller_->ShowMediaRouterDialog(
      MediaRouterDialogActivationLocation::TOOLBAR);
  base::RunLoop().RunUntilIdle();
}

bool MediaRouterCastUiForTest::IsDialogShown() const {
  return dialog_controller_->IsShowingMediaRouterDialog();
}

void MediaRouterCastUiForTest::HideDialog() {
  dialog_controller_->HideMediaRouterDialog();
  base::RunLoop().RunUntilIdle();
}

void MediaRouterCastUiForTest::ChooseSourceType(
    CastDialogView::SourceType source_type) {
  CastDialogView* dialog_view = GetDialogView();
  CHECK(dialog_view);

  views::test::ButtonTestApi(dialog_view->sources_button_for_test())
      .NotifyClick(CreateMousePressedEvent());
  int source_index;
  switch (source_type) {
    case CastDialogView::kTab:
      source_index = 0;
      break;
    case CastDialogView::kDesktop:
      source_index = 1;
      break;
  }
  dialog_view->sources_menu_model_for_test()->ActivatedAt(source_index);
}

CastDialogView::SourceType MediaRouterCastUiForTest::GetChosenSourceType()
    const {
  const CastDialogView* dialog_view = GetDialogView();
  CHECK(dialog_view);
  return dialog_view->selected_source_;
}

MediaRoute::Id MediaRouterCastUiForTest::GetRouteIdForSink(
    const std::string& sink_name) const {
  CastDialogSinkButton* sink_button =
      static_cast<CastDialogSinkButton*>(GetSinkButton(sink_name));
  if (!sink_button->sink().route) {
    return "";
  }
  return sink_button->sink().route->media_route_id();
}

std::string MediaRouterCastUiForTest::GetStatusTextForSink(
    const std::string& sink_name) const {
  CastDialogSinkButton* sink_button =
      static_cast<CastDialogSinkButton*>(GetSinkButton(sink_name));
  return base::UTF16ToUTF8(sink_button->sink().status_text);
}

std::string MediaRouterCastUiForTest::GetIssueTextForSink(
    const std::string& sink_name) const {
  CastDialogSinkButton* sink_button =
      static_cast<CastDialogSinkButton*>(GetSinkButton(sink_name));
  if (!sink_button->sink().issue) {
    NOTREACHED() << "Issue not found for sink " << sink_name;
    return "";
  }
  return sink_button->sink().issue->info().title;
}

void MediaRouterCastUiForTest::WaitForSink(const std::string& sink_name) {
  ObserveDialog(WatchType::kSink, sink_name);
}

void MediaRouterCastUiForTest::WaitForSinkAvailable(
    const std::string& sink_name) {
  ObserveDialog(WatchType::kSinkAvailable, sink_name);
}

void MediaRouterCastUiForTest::WaitForAnyIssue() {
  ObserveDialog(WatchType::kAnyIssue);
}

void MediaRouterCastUiForTest::WaitForAnyRoute() {
  ObserveDialog(WatchType::kAnyRoute);
}

void MediaRouterCastUiForTest::WaitForDialogShown() {
  CHECK(!watch_sink_name_);
  CHECK(!watch_callback_);
  CHECK_EQ(watch_type_, WatchType::kNone);
  if (IsDialogShown())
    return;
  WaitForAnyDialogShown();
}

void MediaRouterCastUiForTest::WaitForDialogHidden() {
  if (!IsDialogShown())
    return;

  ObserveDialog(WatchType::kDialogHidden);
}

void MediaRouterCastUiForTest::OnDialogCreated() {
  MediaRouterUiForTestBase::OnDialogCreated();
  GetDialogView()->KeepShownForTesting();
}

MediaRouterCastUiForTest::MediaRouterCastUiForTest(
    content::WebContents* web_contents)
    : MediaRouterUiForTestBase(web_contents),
      content::WebContentsUserData<MediaRouterCastUiForTest>(*web_contents) {}

void MediaRouterCastUiForTest::OnDialogModelUpdated(
    CastDialogView* dialog_view) {
  if (!watch_callback_ || watch_type_ == WatchType::kDialogShown ||
      watch_type_ == WatchType::kDialogHidden) {
    return;
  }

  const std::vector<CastDialogSinkButton*>& sink_buttons =
      dialog_view->sink_buttons_for_test();
  if (base::ranges::any_of(
          sink_buttons, [&, this](CastDialogSinkButton* sink_button) {
            switch (watch_type_) {
              case WatchType::kSink:
                return sink_button->sink().friendly_name ==
                       base::UTF8ToUTF16(*watch_sink_name_);
              case WatchType::kSinkAvailable:
                return sink_button->sink().friendly_name ==
                           base::UTF8ToUTF16(*watch_sink_name_) &&
                       sink_button->sink().state ==
                           UIMediaSinkState::AVAILABLE &&
                       sink_button->GetEnabled();
              case WatchType::kAnyIssue:
                return sink_button->sink().issue.has_value();
              case WatchType::kAnyRoute:
                return sink_button->sink().route.has_value();
              case WatchType::kNone:
              case WatchType::kDialogShown:
              case WatchType::kDialogHidden:
                NOTREACHED() << "Invalid WatchType";
                return false;
            }
          })) {
    std::move(*watch_callback_).Run();
    watch_callback_.reset();
    watch_sink_name_.reset();
    watch_type_ = WatchType::kNone;
    dialog_view->RemoveObserver(this);
  }
}

void MediaRouterCastUiForTest::OnDialogWillClose(CastDialogView* dialog_view) {
  if (watch_type_ == WatchType::kDialogHidden) {
    std::move(*watch_callback_).Run();
    watch_callback_.reset();
    watch_type_ = WatchType::kNone;
  }
  CHECK(!watch_callback_);
  if (dialog_view)
    dialog_view->RemoveObserver(this);
}

CastDialogSinkButton* MediaRouterCastUiForTest::GetSinkButton(
    const std::string& sink_name) const {
  const CastDialogView* dialog_view = GetDialogView();
  CHECK(dialog_view);
  const std::vector<CastDialogSinkButton*>& sink_buttons =
      dialog_view->sink_buttons_for_test();
  return GetSinkButtonWithName(sink_buttons, sink_name);
}

void MediaRouterCastUiForTest::ObserveDialog(
    WatchType watch_type,
    absl::optional<std::string> sink_name) {
  CHECK(!watch_sink_name_);
  CHECK(!watch_callback_);
  CHECK_EQ(watch_type_, WatchType::kNone);
  base::RunLoop run_loop;
  watch_sink_name_ = std::move(sink_name);
  watch_callback_ = run_loop.QuitClosure();
  watch_type_ = watch_type;

  CastDialogView* dialog_view = GetDialogView();
  CHECK(dialog_view);
  dialog_view->AddObserver(this);
  // Check if the current dialog state already meets the condition that we are
  // waiting for.
  OnDialogModelUpdated(dialog_view);

  run_loop.Run();
}

const CastDialogView* MediaRouterCastUiForTest::GetDialogView() const {
  return dialog_controller_->GetCastDialogCoordinatorForTesting()
      .GetCastDialogView();
}

CastDialogView* MediaRouterCastUiForTest::GetDialogView() {
  return dialog_controller_->GetCastDialogCoordinatorForTesting()
      .GetCastDialogView();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaRouterCastUiForTest);

}  // namespace media_router
