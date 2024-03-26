// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {

ReadWriteCardsView::ReadWriteCardsView() = default;

ReadWriteCardsView::~ReadWriteCardsView() = default;

void ReadWriteCardsView::SetContextMenuBounds(
    const gfx::Rect& context_menu_bounds) {
  if (context_menu_bounds_ == context_menu_bounds) {
    return;
  }

  context_menu_bounds_ = context_menu_bounds;
  UpdateBounds();
}

BEGIN_METADATA(ReadWriteCardsView)
END_METADATA

}  // namespace chromeos
