// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_cast_ui_for_test.h"

#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "ui/gfx/geometry/point.h"

namespace media_router {

MediaRouterCastUiForTest::MediaRouterCastUiForTest(
    content::WebContents* web_contents)
    : MediaRouterUiForTestBase(web_contents) {}

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

  ClickOnButton(dialog_view->sources_button_for_test());
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

void MediaRouterCastUiForTest::StartCasting(const std::string& sink_name) {
  CastDialogSinkView* sink_view = GetSinkView(sink_name);
  ClickOnButton(sink_view->cast_sink_button_for_test());
}

void MediaRouterCastUiForTest::StopCasting(const std::string& sink_name) {
  CastDialogSinkView* sink_view = GetSinkView(sink_name);
  if (sink_view->stop_button_for_test()) {
    ClickOnButton(sink_view->stop_button_for_test());
    return;
  }
  NOTREACHED_IN_MIGRATION() << "No stop button found for sink " << sink_name;
}

MediaRoute::Id MediaRouterCastUiForTest::GetRouteIdForSink(
    const std::string& sink_name) const {
  CastDialogSinkView* sink_view = GetSinkView(sink_name);
  if (!sink_view->sink().route) {
    return "";
  }
  return sink_view->sink().route->media_route_id();
}

std::string MediaRouterCastUiForTest::GetStatusTextForSink(
    const std::string& sink_name) const {
  CastDialogSinkView* sink_view = GetSinkView(sink_name);
  return base::UTF16ToUTF8(sink_view->sink().status_text);
}

std::string MediaRouterCastUiForTest::GetIssueTextForSink(
    const std::string& sink_name) const {
  CastDialogSinkButton* sink_button =
      static_cast<CastDialogSinkButton*>(GetSinkButton(sink_name));
  if (!sink_button->sink().issue) {
    NOTREACHED_IN_MIGRATION() << "Issue not found for sink " << sink_name;
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

void MediaRouterCastUiForTest::OnDialogModelUpdated(
    CastDialogView* dialog_view) {
  if (!watch_callback_ || watch_type_ == WatchType::kDialogShown ||
      watch_type_ == WatchType::kDialogHidden) {
    return;
  }

  const std::vector<raw_ptr<CastDialogSinkView, DanglingUntriaged>>&
      sink_views = dialog_view->sink_views_for_test();
  if (base::ranges::any_of(
          sink_views, [&, this](CastDialogSinkView* sink_view) {
            switch (watch_type_) {
              case WatchType::kSink:
                return sink_view->sink().friendly_name ==
                       base::UTF8ToUTF16(*watch_sink_name_);
              case WatchType::kSinkAvailable:
                return sink_view->sink().friendly_name ==
                           base::UTF8ToUTF16(*watch_sink_name_) &&
                       sink_view->sink().state == UIMediaSinkState::AVAILABLE &&
                       sink_view->cast_sink_button_for_test()->GetEnabled();
              case WatchType::kAnyIssue:
                return sink_view->sink().issue.has_value();
              case WatchType::kAnyRoute:
                return sink_view->sink().route.has_value();
              case WatchType::kNone:
              case WatchType::kDialogShown:
              case WatchType::kDialogHidden:
                NOTREACHED_IN_MIGRATION() << "Invalid WatchType";
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
  return GetSinkView(sink_name)->cast_sink_button_for_test();
}

void MediaRouterCastUiForTest::ObserveDialog(
    WatchType watch_type,
    std::optional<std::string> sink_name) {
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

CastDialogSinkView* MediaRouterCastUiForTest::GetSinkView(
    const std::string& sink_name) const {
  const CastDialogView* dialog_view = GetDialogView();
  CHECK(dialog_view);
  const std::vector<raw_ptr<CastDialogSinkView, DanglingUntriaged>>&
      sink_views = dialog_view->sink_views_for_test();
  auto it = base::ranges::find(sink_views, base::UTF8ToUTF16(sink_name),
                               [](CastDialogSinkView* sink_view) {
                                 return sink_view->sink().friendly_name;
                               });
  if (it == sink_views.end()) {
    NOTREACHED_IN_MIGRATION() << "Sink view not found for sink: " << sink_name;
    return nullptr;
  } else {
    return it->get();
  }
}

}  // namespace media_router
