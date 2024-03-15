// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "ui/views/view_utils.h"

namespace chromeos::mahi {

MahiMenuController::MahiMenuController(
    ReadWriteCardsUiController& read_write_cards_ui_controller)
    : read_write_cards_ui_controller_(read_write_cards_ui_controller) {}

MahiMenuController::~MahiMenuController() = default;

void MahiMenuController::OnContextMenuShown(Profile* profile) {}

void MahiMenuController::OnTextAvailable(const gfx::Rect& anchor_bounds,
                                         const std::string& selected_text,
                                         const std::string& surrounding_text) {
  if (!chromeos::MahiManager::IsEnabledWithCorrectFeatureKey()) {
    return;
  }

  // Only shows mahi menu for distillable pages or when the switch
  // `kUseFakeMahiManager` is enabled.
  if (!::mahi::MahiWebContentsManager::Get()->IsFocusedPageDistillable() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kUseFakeMahiManager)) {
    return;
  }

  if (selected_text.empty()) {
    menu_widget_ = MahiMenuView::CreateWidget(anchor_bounds);
    menu_widget_->ShowInactive();
    return;
  }

  // If there is selected text, we will show the condensed Mahi view alongside
  // quick answers.
  // TODO(b/324647147): Use condensed view here instead of `MahiMenuView`.
  read_write_cards_ui_controller_.SetMahiView(std::make_unique<MahiMenuView>());
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

  read_write_cards_ui_controller_.RemoveMahiView();
}

base::WeakPtr<MahiMenuController> MahiMenuController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos::mahi
