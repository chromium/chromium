// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic/glic_and_actor_buttons_container.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace {

// Corner radius for the button background and highlight.
constexpr int kCornerRadius = 12;

}  // namespace

GlicAndActorButtonsContainer::GlicAndActorButtonsContainer() {
  ink_drop_container_view_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());
  ink_drop_container_view_->SetAutoMatchParentBounds(false);
  ink_drop_container_view_->SetPaintToLayer();
  ink_drop_container_view_->layer()->SetFillsBoundsOpaquely(false);

  views::InkDrop::Install(this, std::make_unique<views::InkDropHost>(this));
  views::InkDropHost* host = GetInkDropHost();
  host->SetMode(views::InkDropHost::InkDropMode::ON);
  host->SetLayerRegion(views::LayerRegion::kAbove);
  host->GetInkDrop()->SetShowHighlightOnHover(false);

  if (GetColorProvider() && GetWidget() && !GetWidget()->IsClosed()) {
    UpdateInkDrop();
  }

  SetCollapseMargins(true);
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
}

GlicAndActorButtonsContainer::~GlicAndActorButtonsContainer() = default;

void GlicAndActorButtonsContainer::AddLayerToRegion(ui::Layer* layer,
                                                    views::LayerRegion region) {
  ink_drop_container_view_->AddLayerToRegion(layer, region);
}

void GlicAndActorButtonsContainer::RemoveLayerFromRegions(ui::Layer* layer) {
  ink_drop_container_view_->RemoveLayerFromRegions(layer);
}

void GlicAndActorButtonsContainer::Layout(PassKey) {
  LayoutSuperclass<views::FlexLayoutView>(this);
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(gfx::Insets::VH(4, 8));
  ink_drop_container_view_->SetBoundsRect(bounds);
}

glic::TabStripGlicButton* GlicAndActorButtonsContainer::InsertGlicButton(
    glic::TabStripGlicButton* glic_button) {
  // Insert after ink drop container.
  return AddChildViewAt(glic_button, 1ul);
}

void GlicAndActorButtonsContainer::SetHighlighted(bool highlighted) {
  if (!GetWidget() || GetWidget()->IsClosed()) {
    return;
  }

  views::InkDropState state = highlighted ? views::InkDropState::ACTIVATED
                                          : views::InkDropState::DEACTIVATED;
  views::InkDropHost* host = GetInkDropHost();
  if (host->GetInkDrop()->GetTargetInkDropState() == state) {
    return;
  }
  host->AnimateToState(state, nullptr);
  UpdateInkDrop();
}

void GlicAndActorButtonsContainer::SetBackgroundColor(ui::ColorId color_id) {
  SetBackground(views::CreateRoundedRectBackground(
      color_id, gfx::RoundedCornersF(kCornerRadius), gfx::Insets::VH(4, 8)));
  UpdateInkDrop();
}

views::InkDropHost* GlicAndActorButtonsContainer::GetInkDropHost() {
  return views::InkDrop::Get(this);
}

void GlicAndActorButtonsContainer::UpdateInkDrop() {
  if (!GetColorProvider() || !GetWidget() || GetWidget()->IsClosed()) {
    return;
  }

  // Ripple is not used, but we create a transparent one here so that
  // CreateInkDropRipple doesn't create its own with a default color.
  GetInkDropHost()->SetCreateRippleCallback(base::BindRepeating(
      [](views::View* host) -> std::unique_ptr<views::InkDropRipple> {
        return std::make_unique<views::FloodFillInkDropRipple>(
            views::InkDrop::Get(host), gfx::Size(), gfx::Point(),
            SK_ColorTRANSPARENT, SK_AlphaTRANSPARENT);
      },
      this));

  const ChromeColorIds hover_color_id = kColorTabStripControlButtonInkDrop;
  GetInkDropHost()->SetCreateHighlightCallback(base::BindRepeating(
      [](views::View* host,
         views::InkDropContainerView* ink_drop_container_view,
         ChromeColorIds hover_color_id) {
        const auto* color_provider = host->GetColorProvider();
        SkColor hover_color = color_provider
                                  ? color_provider->GetColor(hover_color_id)
                                  : gfx::kPlaceholderColor;

        const float hover_alpha = SkColorGetA(hover_color);

        auto ink_drop_highlight = std::make_unique<views::InkDropHighlight>(
            ink_drop_container_view->size(), kCornerRadius,
            gfx::RectF(ink_drop_container_view->GetLocalBounds()).CenterPoint(),
            SkColorSetA(hover_color, SK_AlphaOPAQUE));

        ink_drop_highlight->set_visible_opacity(hover_alpha / SK_AlphaOPAQUE);
        return ink_drop_highlight;
      },
      this, ink_drop_container_view_, hover_color_id));
}

BEGIN_METADATA(GlicAndActorButtonsContainer)
END_METADATA
