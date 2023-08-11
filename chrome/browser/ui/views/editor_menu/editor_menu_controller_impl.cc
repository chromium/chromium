// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"

#include "ui/gfx/geometry/rect.h"

namespace chromeos::editor_menu {

namespace {

// TODO(b/295059934): Call EditorMediator APIs before showing views.
EditorMenuControllerImpl::ConsentStatus GetConsentStatus() {
  return EditorMenuControllerImpl::ConsentStatus::kAccepted;
}

}  // namespace

EditorMenuControllerImpl::EditorMenuControllerImpl() = default;

EditorMenuControllerImpl::~EditorMenuControllerImpl() = default;

void EditorMenuControllerImpl::MaybeShowEditorMenu(
    const gfx::Rect& anchor_bounds) {
  // TODO(b/295059934): Call EditorMediator APIs before showing views.
  // APIs are async and requires refactoring.
  const auto status = GetConsentStatus();
  if (status == ConsentStatus::kRejected) {
    return;
  }

  if (status == ConsentStatus::kPending) {
    // TODO(b/295061567): Implement the consent view.
    return;
  }

  // TODO(b/295060733): Create the main view.
  // TODO(b/295059934): Call EditorMediator API to get the parameters.
}

void EditorMenuControllerImpl::DismissEditorMenu() {
  if (editor_menu_widget_) {
    editor_menu_widget_.reset();
  }
}

void EditorMenuControllerImpl::UpdateAnchorBounds(
    const gfx::Rect& anchor_bounds) {
  if (!editor_menu_widget_) {
    return;
  }

  // Update the bounds of the shown view.
  // TODO(b/295060733): The main view.
  // TODO(b/295061567): The consent view.
}

}  // namespace chromeos::editor_menu
