// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"

#include "ash/public/cpp/shelf_config.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {
namespace {

constexpr double kEightPrecent = 0.08;

}  // namespace

constexpr gfx::Size kMaxDialogSize{768, 768};
// Min height should match --oobe-dialog-min-height;
constexpr gfx::Size kMinDialogSize{464, 384};
constexpr gfx::Insets kMinMargins{48};

// Sizes come from specs except min widths which are taken as maximal zoomed
// display widths of smallest device ChromeTab (960x600).
constexpr gfx::Size kMaxLandscapeDialogSize{1040, 680};
constexpr gfx::Size kMinLandscapeDialogSize{738, 540};
constexpr gfx::Size kMaxPortraitDialogSize{680, 1040};
constexpr gfx::Size kMinPortraitDialogSize{461, 820};

constexpr int kOobeDialogShadowElevation = 12;
constexpr int kOobeDialogCornerRadius = 24;

gfx::Size CalculateOobeDialogSize(const gfx::Size& host_size,
                                  int shelf_height,
                                  bool is_horizontal) {
  gfx::Size margin = ScaleToCeiledSize(host_size, kEightPrecent);
  gfx::Size margins = margin + margin;
  margins.SetToMax(kMinMargins.size());
  margins.Enlarge(0, shelf_height);
  gfx::Size result = host_size - margins;
  if (is_horizontal) {
    result.SetToMin(kMaxLandscapeDialogSize);
    result.SetToMax(kMinLandscapeDialogSize);
  } else {
    result.SetToMin(kMaxPortraitDialogSize);
    result.SetToMax(kMinPortraitDialogSize);
  }
  return result;
}

gfx::Size CalculateOobeDialogSizeForPrimaryDisplay() {
  const gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  const bool is_horizontal = display_size.width() > display_size.height();
  // ShellConfig is only non-existent in test scenarios.
  const int shelf_height = ShelfConfig::Get() ? ShelfConfig::Get()->shelf_size()
                                              : 48 /* default shelf height */;
  return CalculateOobeDialogSize(display_size, shelf_height, is_horizontal);
}

void CalculateOobeDialogBounds(const gfx::Rect& host_bounds,
                               int shelf_height,
                               bool is_horizontal,
                               gfx::Rect* result) {
  // Area to position dialog.
  *result = host_bounds;
  result->Inset(gfx::Insets().set_bottom(shelf_height));

  // Center dialog within an available area.
  result->ClampToCenteredSize(
      CalculateOobeDialogSize(host_bounds.size(), shelf_height, is_horizontal));
}

}  // namespace ash
