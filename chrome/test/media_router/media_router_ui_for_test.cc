// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_ui_for_test.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/ui/media_router/media_router_file_dialog.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"

namespace media_router {

namespace {

ui::MouseEvent CreateMousePressedEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                        gfx::Point(0, 0), ui::EventTimeForNow(),
                        ui::EF_LEFT_MOUSE_BUTTON, 0);
}

ui::MouseEvent CreateMouseReleasedEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(0, 0),
                        gfx::Point(0, 0), ui::EventTimeForNow(),
                        ui::EF_LEFT_MOUSE_BUTTON, 0);
}

// Routes observer that calls a callback once there are no routes.
class NoRoutesObserver : public MediaRoutesObserver {
 public:
  NoRoutesObserver(MediaRouter* router, base::OnceClosure callback)
      : MediaRoutesObserver(router), callback_(std::move(callback)) {}
  ~NoRoutesObserver() override = default;

  void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids) override {
    if (callback_ && routes.empty())
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

// File dialog with a preset file URL.
class TestMediaRouterFileDialog : public MediaRouterFileDialog {
 public:
  TestMediaRouterFileDialog(MediaRouterFileDialogDelegate* delegate, GURL url)
      : MediaRouterFileDialog(nullptr), delegate_(delegate), file_url_(url) {}
  ~TestMediaRouterFileDialog() override {}

  GURL GetLastSelectedFileUrl() override { return file_url_; }

  void OpenFileDialog(Browser* browser) override {
    delegate_->FileDialogFileSelected(ui::SelectedFileInfo());
  }

 private:
  MediaRouterFileDialogDelegate* delegate_;
  GURL file_url_;
};

// File dialog which fails on open.
class TestFailMediaRouterFileDialog : public MediaRouterFileDialog {
 public:
  TestFailMediaRouterFileDialog(MediaRouterFileDialogDelegate* delegate,
                                const IssueInfo& issue)
      : MediaRouterFileDialog(nullptr), delegate_(delegate), issue_(issue) {}
  ~TestFailMediaRouterFileDialog() override {}

  void OpenFileDialog(Browser* browser) override {
    delegate_->FileDialogSelectionFailed(issue_);
  }

 private:
  MediaRouterFileDialogDelegate* delegate_;
  const IssueInfo issue_;
};

}  // namespace

// static
MediaRouterUiForTest* MediaRouterUiForTest::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  // No-op if an instance already exists for the WebContents.
  MediaRouterUiForTest::CreateForWebContents(web_contents);
  return MediaRouterUiForTest::FromWebContents(web_contents);
}

MediaRouterUiForTest::~MediaRouterUiForTest() {
  CHECK(!watch_callback_);
}

void MediaRouterUiForTest::TearDown() {
  if (IsDialogShown())
    HideDialog();
}

void MediaRouterUiForTest::ShowDialog() {
  dialog_controller_->ShowMediaRouterDialog();
  base::RunLoop().RunUntilIdle();
}

void MediaRouterUiForTest::HideDialog() {
  dialog_controller_->HideMediaRouterDialog();
  base::RunLoop().RunUntilIdle();
}

bool MediaRouterUiForTest::IsDialogShown() const {
  return dialog_controller_->IsShowingMediaRouterDialog();
}

void MediaRouterUiForTest::ChooseSourceType(
    CastDialogView::SourceType source_type) {
  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);

  dialog_view->ButtonPressed(dialog_view->sources_button_for_test(),
                             CreateMousePressedEvent());
  int source_index;
  switch (source_type) {
    case CastDialogView::kTab:
      source_index = 0;
      break;
    case CastDialogView::kDesktop:
      source_index = 1;
      break;
    case CastDialogView::kLocalFile:
      source_index = 2;
      break;
  }
  dialog_view->sources_menu_model_for_test()->ActivatedAt(source_index);
}

CastDialogView::SourceType MediaRouterUiForTest::GetChosenSourceType() const {
  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);
  return dialog_view->selected_source_;
}

void MediaRouterUiForTest::StartCasting(const std::string& sink_name) {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_name);
  CHECK(sink_button->GetEnabled());
  sink_button->OnMousePressed(CreateMousePressedEvent());
  sink_button->OnMouseReleased(CreateMouseReleasedEvent());
  base::RunLoop().RunUntilIdle();
}

void MediaRouterUiForTest::StopCasting(const std::string& sink_name) {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_name);
  sink_button->OnMousePressed(CreateMousePressedEvent());
  sink_button->OnMouseReleased(CreateMouseReleasedEvent());
  base::RunLoop().RunUntilIdle();
}

void MediaRouterUiForTest::StopCasting() {
  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);
  for (CastDialogSinkButton* sink_button :
       dialog_view->sink_buttons_for_test()) {
    if (sink_button->sink().state == UIMediaSinkState::CONNECTED) {
      sink_button->OnMousePressed(CreateMousePressedEvent());
      sink_button->OnMouseReleased(CreateMouseReleasedEvent());
      base::RunLoop().RunUntilIdle();
      return;
    }
  }
  NOTREACHED() << "Sink was not found";
}

void MediaRouterUiForTest::WaitForSink(const std::string& sink_name) {
  ObserveDialog(WatchType::kSink, sink_name);
}

void MediaRouterUiForTest::WaitForSinkAvailable(const std::string& sink_name) {
  ObserveDialog(WatchType::kSinkAvailable, sink_name);
}

void MediaRouterUiForTest::WaitForAnyIssue() {
  ObserveDialog(WatchType::kAnyIssue);
}

void MediaRouterUiForTest::WaitForAnyRoute() {
  ObserveDialog(WatchType::kAnyRoute);
}

void MediaRouterUiForTest::WaitForDialogShown() {
  CHECK(!watch_sink_name_);
  CHECK(!watch_callback_);
  CHECK_EQ(watch_type_, WatchType::kNone);
  if (IsDialogShown())
    return;

  base::RunLoop run_loop;
  watch_callback_ = run_loop.QuitClosure();
  watch_type_ = WatchType::kDialogShown;
  run_loop.Run();
}

void MediaRouterUiForTest::WaitForDialogHidden() {
  if (!IsDialogShown())
    return;

  ObserveDialog(WatchType::kDialogHidden);
}

void MediaRouterUiForTest::WaitUntilNoRoutes() {
  base::RunLoop run_loop;
  NoRoutesObserver no_routes_observer(
      MediaRouterFactory::GetApiForBrowserContext(
          web_contents_->GetBrowserContext()),
      run_loop.QuitClosure());
  run_loop.Run();
}

MediaRoute::Id MediaRouterUiForTest::GetRouteIdForSink(
    const std::string& sink_name) const {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_name);
  if (!sink_button->sink().route) {
    NOTREACHED() << "Route not found for sink " << sink_name;
    return "";
  }
  return sink_button->sink().route->media_route_id();
}

std::string MediaRouterUiForTest::GetStatusTextForSink(
    const std::string& sink_name) const {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_name);
  return base::UTF16ToUTF8(sink_button->sink().status_text);
}

std::string MediaRouterUiForTest::GetIssueTextForSink(
    const std::string& sink_name) const {
  CastDialogSinkButton* sink_button = GetSinkButton(sink_name);
  if (!sink_button->sink().issue) {
    NOTREACHED() << "Issue not found for sink " << sink_name;
    return "";
  }
  return sink_button->sink().issue->info().title;
}

void MediaRouterUiForTest::SetLocalFile(const GURL& file_url) {
  dialog_controller_->ui()->set_media_router_file_dialog_for_test(
      std::make_unique<TestMediaRouterFileDialog>(dialog_controller_->ui(),
                                                  file_url));
}

void MediaRouterUiForTest::SetLocalFileSelectionIssue(const IssueInfo& issue) {
  dialog_controller_->ui()->set_media_router_file_dialog_for_test(
      std::make_unique<TestFailMediaRouterFileDialog>(dialog_controller_->ui(),
                                                      issue));
}

MediaRouterUiForTest::MediaRouterUiForTest(content::WebContents* web_contents)
    : web_contents_(web_contents),
      dialog_controller_(static_cast<MediaRouterDialogControllerViews*>(
          MediaRouterDialogController::GetOrCreateForWebContents(
              web_contents))) {
  dialog_controller_->SetDialogCreationCallbackForTesting(base::BindRepeating(
      &MediaRouterUiForTest::OnDialogCreated, weak_factory_.GetWeakPtr()));
}

void MediaRouterUiForTest::OnDialogModelUpdated(CastDialogView* dialog_view) {
  if (!watch_callback_ || watch_type_ == WatchType::kDialogShown ||
      watch_type_ == WatchType::kDialogHidden) {
    return;
  }

  const std::vector<CastDialogSinkButton*>& sink_buttons =
      dialog_view->sink_buttons_for_test();
  if (std::find_if(sink_buttons.begin(), sink_buttons.end(),
                   [&, this](CastDialogSinkButton* sink_button) {
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
                   }) != sink_buttons.end()) {
    std::move(*watch_callback_).Run();
    watch_callback_.reset();
    watch_sink_name_.reset();
    watch_type_ = WatchType::kNone;
    dialog_view->RemoveObserver(this);
  }
}

void MediaRouterUiForTest::OnDialogWillClose(CastDialogView* dialog_view) {
  if (watch_type_ == WatchType::kDialogHidden) {
    std::move(*watch_callback_).Run();
    watch_callback_.reset();
    watch_type_ = WatchType::kNone;
  }
  CHECK(!watch_callback_);
  if (dialog_view)
    dialog_view->RemoveObserver(this);
}

void MediaRouterUiForTest::OnDialogCreated() {
  if (watch_type_ == WatchType::kDialogShown) {
    std::move(*watch_callback_).Run();
    watch_callback_.reset();
    watch_type_ = WatchType::kNone;
  }
  CastDialogView::GetInstance()->KeepShownForTesting();
}

CastDialogSinkButton* MediaRouterUiForTest::GetSinkButton(
    const std::string& sink_name) const {
  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);
  const std::vector<CastDialogSinkButton*>& sink_buttons =
      dialog_view->sink_buttons_for_test();
  auto it = std::find_if(sink_buttons.begin(), sink_buttons.end(),
                         [sink_name](CastDialogSinkButton* sink_button) {
                           return sink_button->sink().friendly_name ==
                                  base::UTF8ToUTF16(sink_name);
                         });
  if (it == sink_buttons.end()) {
    NOTREACHED() << "Sink button not found for sink: " << sink_name;
    return nullptr;
  } else {
    return *it;
  }
}

void MediaRouterUiForTest::ObserveDialog(
    WatchType watch_type,
    base::Optional<std::string> sink_name) {
  CHECK(!watch_sink_name_);
  CHECK(!watch_callback_);
  CHECK_EQ(watch_type_, WatchType::kNone);
  base::RunLoop run_loop;
  watch_sink_name_ = std::move(sink_name);
  watch_callback_ = run_loop.QuitClosure();
  watch_type_ = watch_type;

  CastDialogView* dialog_view = CastDialogView::GetInstance();
  CHECK(dialog_view);
  dialog_view->AddObserver(this);
  // Check if the current dialog state already meets the condition that we are
  // waiting for.
  OnDialogModelUpdated(dialog_view);

  run_loop.Run();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaRouterUiForTest)

}  // namespace media_router
