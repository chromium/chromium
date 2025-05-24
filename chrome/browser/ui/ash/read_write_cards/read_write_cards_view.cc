// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_view.h"

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_shadow.h"

namespace chromeos {

ReadWriteCardsView::ReadWriteCardsView(
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
    : view_shadow_(std::make_unique<views::ViewShadow>(this, /*elevation=*/2)),
      read_write_cards_ui_controller_(read_write_cards_ui_controller) {
  context_menu_bounds_ = read_write_cards_ui_controller_->context_menu_bounds();

  view_shadow_->SetRoundedCornerRadius(
      GetLayoutProvider()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kMenuRadius));

  CHECK(layer()) << "A layer should be created by the constructor of "
                    "ViewShadow with SetPaintToLayer()";
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kMenuRadius)));
  layer()->SetIsFastRoundedCorner(true);
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
