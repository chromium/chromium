// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace {

std::unique_ptr<views::HighlightPathGenerator>
GetListItemHighlightPathGenerator() {
  return std::make_unique<views::RoundRectHighlightPathGenerator>(
      /*insets=*/gfx::Insets(0),
      /*corner_radius=*/projects_panel::kListItemCornerRadius);
}

}  // namespace

namespace projects_panel {

void ConfigureInkDropForButton(views::Button* view) {
  auto* ink_drop = views::InkDrop::Get(view);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->SetLayerRegion(views::LayerRegion::kBelow);
  ink_drop->SetBaseColor(kColorProjectsPanelButtonHoverBackground);
  ink_drop->GetInkDrop()->SetHoverHighlightFadeDuration(
      projects_panel::kListItemHoverFadeAnimationDuration);
  ink_drop->SetHighlightOpacity(1.0f);
  views::HighlightPathGenerator::Install(view,
                                         GetListItemHighlightPathGenerator());
  views::FocusRing::Install(view);
  views::FocusRing::Get(view)->SetPathGenerator(
      GetListItemHighlightPathGenerator());
  views::FocusRing::Get(view)->SetHaloInset(
      projects_panel::kListItemFocusRingHaloInset);
}

}  // namespace projects_panel
