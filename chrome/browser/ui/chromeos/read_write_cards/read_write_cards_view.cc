// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/view_shadow.h"

namespace chromeos {

ReadWriteCardsView::ReadWriteCardsView(
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
    : view_shadow_(std::make_unique<views::ViewShadow>(this, /*elevation=*/2)),
      read_write_cards_ui_controller_(read_write_cards_ui_controller) {
  context_menu_bounds_ = read_write_cards_ui_controller_->context_menu_bounds();
}

ReadWriteCardsView::~ReadWriteCardsView() = default;

void ReadWriteCardsView::SetContextMenuBounds(
    const gfx::Rect& context_menu_bounds) {
  if (context_menu_bounds_ == context_menu_bounds) {
    return;
  }

  context_menu_bounds_ = context_menu_bounds;
  UpdateBoundsForQuickAnswers();
}

void ReadWriteCardsView::UpdateBoundsForQuickAnswers() {}

void ReadWriteCardsView::AddedToWidget() {
  // Make sure the bounds is updated correctly according to
  // `context_menu_bounds_`.
  UpdateBoundsForQuickAnswers();
}

BEGIN_METADATA(ReadWriteCardsView)
END_METADATA

}  // namespace chromeos
