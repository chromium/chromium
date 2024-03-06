// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "ui/views/view_utils.h"

namespace chromeos::mahi {

MahiMenuController::MahiMenuController() = default;

MahiMenuController::~MahiMenuController() = default;

void MahiMenuController::OnContextMenuShown(Profile* profile) {}

void MahiMenuController::OnTextAvailable(const gfx::Rect& anchor_bounds,
                                         const std::string& selected_text,
                                         const std::string& surrounding_text) {
  menu_widget_ = MahiMenuView::CreateWidget(anchor_bounds);
  menu_widget_->ShowInactive();
}

void MahiMenuController::OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) {
  if (!menu_widget_ || !menu_widget_->GetContentsView()) {
    return;
  }

  views::AsViewClass<MahiMenuView>(menu_widget_->GetContentsView())
      ->UpdateBounds(anchor_bounds);
}

void MahiMenuController::OnDismiss(bool is_other_command_executed) {
  if (menu_widget_ && !menu_widget_->IsActive()) {
    menu_widget_.reset();
  }
}

base::WeakPtr<MahiMenuController> MahiMenuController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos::mahi
