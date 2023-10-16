// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/compose_dialog_controller.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/compose/compose_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// Default size from Figma spec. The size of the view will follow the requested
// size of the WebUI, once these are connected.
constexpr gfx::Size kInputDialogSize(448, 216);

namespace chrome {

void ShowComposeDialog(content::WebContents& web_contents,
                       const gfx::RectF& element_bounds) {
  ComposeDialogController::GetOrCreate(&web_contents)
      ->ShowComposeDialog(nullptr, element_bounds);
}

}  // namespace chrome

ComposeDialogController::~ComposeDialogController() = default;

// static
ComposeDialogController* ComposeDialogController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  ComposeDialogController::CreateForWebContents(web_contents);
  return ComposeDialogController::FromWebContents(web_contents);
}

// TOOD(b/301371110): This function should accept an argument indicating whether
// it was called from the context menu. If so, open by popping in. Otherwise,
// open with an expanding animation.
void ComposeDialogController::ShowComposeDialog(
    views::View* anchor_view,
    const gfx::RectF& element_bounds_in_screen) {
  gfx::Rect element_centered_bounds = ComputeCenteredDialogBoundsInScreen(
      kInputDialogSize, element_bounds_in_screen);

  auto compose_dialog_view = std::make_unique<compose::ComposeDialogView>(
      anchor_view, views::BubbleBorder::TOP_LEFT, element_centered_bounds,
      &GetWebContents());
  compose_dialog_view_ = compose_dialog_view.get();

  views::BubbleDialogDelegateView::CreateBubble(std::move(compose_dialog_view))
      ->Show();
}

gfx::Rect ComposeDialogController::ComputeCenteredDialogBoundsInScreen(
    const gfx::Size dialog_size,
    const gfx::RectF& element_bounds_in_screen) {
  gfx::Rect centered_dialog_bounds =
      gfx::Rect(dialog_size.width(), dialog_size.height());

  // Compute dialog position such that it is centered on the corresponding
  // element.
  // TODO(b/300940346): This should be replaced by a more complete positioning
  // algorithm that accounts for multiple factors, including obscuring the
  // element, window bounds, screen bounds, and position of a corresponding
  // Nudge UI.
  int centered_dialog_x = element_bounds_in_screen.x() +
                          element_bounds_in_screen.width() / 2 -
                          dialog_size.width() / 2;
  int centered_dialog_y = element_bounds_in_screen.y() +
                          element_bounds_in_screen.height() / 2 -
                          dialog_size.height() / 2;
  centered_dialog_bounds.set_x(centered_dialog_x);
  centered_dialog_bounds.set_y(centered_dialog_y);

  return centered_dialog_bounds;
}

compose::ComposeDialogView* ComposeDialogController::GetComposeDialog() const {
  return compose_dialog_view_;
}

// TODO(b/300939629): Flesh out implementation and cover other closing paths.
void ComposeDialogController::CloseDialog() {
  compose_dialog_view_->Close();
  compose_dialog_view_ = nullptr;
}

ComposeDialogController::ComposeDialogController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ComposeDialogController>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ComposeDialogController);
