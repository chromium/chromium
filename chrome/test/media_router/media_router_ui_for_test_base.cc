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
#include "ui/views/test/button_test_api.h"

namespace media_router {

void MediaRouterUiForTestBase::TearDown() {
  if (IsDialogShown()) {
    HideDialog();
  }
  torn_down_ = true;
}

// static
void MediaRouterUiForTestBase::ClickOnButton(views::Button* button) {
  views::test::ButtonTestApi(button).NotifyClick(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(0, 0), gfx::Point(0, 0),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  base::RunLoop().RunUntilIdle();
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
