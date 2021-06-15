// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/overlay_dialog.h"

#include "components/arc/compat_mode/style/arc_color_provider.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/flex_layout_view.h"

namespace arc {

namespace {

std::unique_ptr<views::View> MakeScrimBackgroundView() {
  const SkColor kScrimColor = GetShieldLayerColor(ShieldLayerType::kShield60);

  return views::Builder<views::FlexLayoutView>()
      .SetInteriorMargin(gfx::Insets(0, 32))
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetBackground(views::CreateSolidBackground(kScrimColor))
      .Build();
}

}  // namespace

void ShowOverlayDialog(aura::Window* base_window,
                       std::unique_ptr<views::View> dialog_view) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(base_window);
  if (!shell_surface_base || shell_surface_base->HasOverlay())
    return;

  auto dialog_container = MakeScrimBackgroundView();

  if (dialog_view) {
    dialog_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));

    dialog_container->AddChildView(std::move(dialog_view));
  }

  exo::ShellSurfaceBase::OverlayParams params(std::move(dialog_container));
  params.translucent = true;
  shell_surface_base->AddOverlay(std::move(params));
}

void CloseOverlayDialogIfAny(aura::Window* base_window) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(base_window);
  if (shell_surface_base && shell_surface_base->HasOverlay())
    shell_surface_base->RemoveOverlay();
}

}  // namespace arc
