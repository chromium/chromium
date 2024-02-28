// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"

LensOverlayController::LensOverlayController(tabs::TabModel* tab_model)
    : tab_model_(tab_model) {
  tab_model_->owning_model()->AddObserver(this);
}

LensOverlayController::~LensOverlayController() = default;

void LensOverlayController::ShowUI() {
  // If UI is already showing or in the process of showing, do nothing.
  if (state_ != State::kOff) {
    return;
  }

  // The UI should only show if the tab is in the foreground.
  if (tab_model_->owning_model()->GetActiveTab() != tab_model_) {
    return;
  }

  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view = tab_model_->contents()
                                            ->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();

  // During initialization and shutdown a capture may not be possible.
  if (!view || !view->IsSurfaceAvailableForCopy()) {
    return;
  }

  state_ = State::kScreenshot;
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindOnce(&LensOverlayController::DidCaptureScreenshot,
                     weak_factory_.GetWeakPtr(), ++screenshot_attempt_id_));
}

void LensOverlayController::DidCaptureScreenshot(int attempt_id,
                                                 const SkBitmap& bitmap) {
  // While capturing a screenshot the overlay was cancelled. Do nothing.
  if (state_ == State::kOff) {
    return;
  }

  // An id mismatch implies this is not the most recent screenshot attempt.
  if (screenshot_attempt_id_ != attempt_id) {
    return;
  }

  state_ = State::kOverlay;
}

void LensOverlayController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }

  if (selection.new_contents == tab_model_->contents()) {
    TabForegrounded();
    return;
  }

  if (selection.old_contents == tab_model_->contents()) {
    TabBackgrounded();
  }
}

void LensOverlayController::TabForegrounded() {}

void LensOverlayController::TabBackgrounded() {
  // In the future we may want a hibernate state. In this case we would stop
  // showing the UI but persist enough information to defrost the original UI
  // state when the tab is foregrounded.
  state_ = State::kOff;
}
