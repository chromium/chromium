// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_ui_for_test_base.h"

#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "components/media_router/browser/media_router_factory.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/views/view.h"

namespace media_router {

namespace {

ui::MouseEvent CreateMouseEvent(ui::EventType type) {
  return ui::MouseEvent(type, gfx::Point(0, 0), gfx::Point(0, 0),
                        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
}

ui::MouseEvent CreateMousePressedEvent() {
  return CreateMouseEvent(ui::ET_MOUSE_PRESSED);
}

ui::MouseEvent CreateMouseReleasedEvent() {
  return CreateMouseEvent(ui::ET_MOUSE_RELEASED);
}

}  // namespace

void MediaRouterUiForTestBase::TearDown() {
  if (IsDialogShown()) {
    HideDialog();
  }
  torn_down_ = true;
}

void MediaRouterUiForTestBase::StartCasting(const std::string& sink_name) {
  StartCasting(GetSinkButton(sink_name));
}

void MediaRouterUiForTestBase::StopCasting(const std::string& sink_name) {
  StopCasting(GetSinkButton(sink_name));
}

void MediaRouterUiForTestBase::StartCasting(views::View* sink_button) {
  CHECK(sink_button->GetEnabled());
  sink_button->OnMousePressed(CreateMousePressedEvent());
  sink_button->OnMouseReleased(CreateMouseReleasedEvent());
  base::RunLoop().RunUntilIdle();
}

void MediaRouterUiForTestBase::StopCasting(views::View* sink_button) {
  sink_button->OnMousePressed(CreateMousePressedEvent());
  sink_button->OnMouseReleased(CreateMouseReleasedEvent());
  base::RunLoop().RunUntilIdle();
}

// static
CastDialogSinkButton* MediaRouterUiForTestBase::GetSinkButtonWithName(
    const std::vector<raw_ptr<CastDialogSinkView>>& sink_views,
    const std::string& sink_name) {
  auto it = base::ranges::find(sink_views, base::UTF8ToUTF16(sink_name),
                               [](CastDialogSinkView* sink_view) {
                                 return sink_view->sink().friendly_name;
                               });
  if (it == sink_views.end()) {
    NOTREACHED() << "Sink view not found for sink: " << sink_name;
    return nullptr;
  } else {
    return it->get()->cast_sink_button_for_test();
  }
}

void MediaRouterUiForTestBase::OnDialogCreated() {
  if (watch_type_ == WatchType::kDialogShown) {
    std::move(*watch_callback_).Run();
    watch_callback_.reset();
    watch_type_ = WatchType::kNone;
  }
}

MediaRouterUiForTestBase::~MediaRouterUiForTestBase() {
  DCHECK(torn_down_);
}

MediaRouterUiForTestBase::MediaRouterUiForTestBase(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      dialog_controller_(static_cast<MediaRouterDialogControllerViews*>(
          MediaRouterDialogController::GetOrCreateForWebContents(
              web_contents))) {
  dialog_controller_->SetDialogCreationCallbackForTesting(base::BindRepeating(
      &MediaRouterUiForTestBase::OnDialogCreated, weak_factory_.GetWeakPtr()));
}

void MediaRouterUiForTestBase::WaitForAnyDialogShown() {
  base::RunLoop run_loop;
  watch_callback_ = run_loop.QuitClosure();
  watch_type_ = WatchType::kDialogShown;
  run_loop.Run();
}

}  // namespace media_router
