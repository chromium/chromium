// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/overlay_dialog.h"

#include "base/bind.h"
#include "components/arc/compat_mode/style/arc_color_provider.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/views/background.h"

namespace arc {

OverlayDialog::~OverlayDialog() = default;

void OverlayDialog::Show(aura::Window* base_window,
                         base::OnceClosure on_destroying,
                         std::unique_ptr<views::View> dialog_view) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(base_window);
  if (!shell_surface_base)
    return;

  CloseIfAny(base_window);

  auto dialog = base::WrapUnique(
      new OverlayDialog(std::move(on_destroying), std::move(dialog_view)));

  exo::ShellSurfaceBase::OverlayParams params(std::move(dialog));
  params.translucent = true;
  shell_surface_base->AddOverlay(std::move(params));
}

void OverlayDialog::CloseIfAny(aura::Window* base_window) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(base_window);
  if (shell_surface_base && shell_surface_base->HasOverlay())
    shell_surface_base->RemoveOverlay();
}

OverlayDialog::OverlayDialog(base::OnceClosure on_destroying,
                             std::unique_ptr<views::View> dialog_view)
    : scoped_callback_(std::move(on_destroying)) {
  if (dialog_view) {
    SetInteriorMargin(gfx::Insets(0, 32));
    SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    dialog_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));

    AddChildView(std::move(dialog_view));
  }
  const SkColor kScrimColor = GetShieldLayerColor(ShieldLayerType::kShield60);
  SetBackground(views::CreateSolidBackground(kScrimColor));
}

}  // namespace arc
