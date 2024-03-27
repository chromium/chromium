// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace chromeos {

ReadWriteCardsView::ReadWriteCardsView(
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
    : read_write_cards_ui_controller_(read_write_cards_ui_controller) {}

ReadWriteCardsView::~ReadWriteCardsView() = default;

void ReadWriteCardsView::SetContextMenuBounds(
    const gfx::Rect& context_menu_bounds) {
  if (context_menu_bounds_ == context_menu_bounds) {
    return;
  }

  context_menu_bounds_ = context_menu_bounds;
  UpdateBounds();
}

void ReadWriteCardsView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  read_write_cards_ui_controller_->MaybeRelayout();
}

void ReadWriteCardsView::ChildPreferredSizeChanged(views::View* child) {
  read_write_cards_ui_controller_->MaybeRelayout();
}

BEGIN_METADATA(ReadWriteCardsView)
END_METADATA

}  // namespace chromeos
