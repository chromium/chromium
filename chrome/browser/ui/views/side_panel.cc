// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_observer.h"

namespace {

constexpr int kBorderThickness = 16;

// This border paints the toolbar color around the side panel content and draws
// a roundrect viewport around the side panel content.
class SidePanelBorder : public views::Border {
 public:
  SidePanelBorder() : Border(gfx::kPlaceholderColor) {}

  SidePanelBorder(const SidePanelBorder&) = delete;
  SidePanelBorder& operator=(const SidePanelBorder&) = delete;

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    // Undo DSF so that we can be sure to draw an integral number of pixels for
    // the border. Integral scale factors should be unaffected by this, but for
    // fractional scale factors this ensures sharp lines.
    gfx::ScopedCanvas scoped(canvas);
    float dsf = canvas->UndoDeviceScaleFactor();

    gfx::RectF scaled_bounds = gfx::ConvertRectToPixels(
        view.GetLocalBounds(), view.layer()->device_scale_factor());

    const float corner_radius = kBorderThickness * dsf / 2;
    gfx::Insets insets_in_pixels =
        gfx::ToFlooredInsets(gfx::ConvertInsetsToPixels(GetInsets(), dsf));
    scaled_bounds.Inset(insets_in_pixels);
    SkRRect rect = SkRRect::MakeRectXY(gfx::RectFToSkRect(scaled_bounds),
                                       corner_radius, corner_radius);

    // Paint the solid color around the side panel content.
    canvas->sk_canvas()->clipRRect(rect, SkClipOp::kDifference, true);
    // TODO(pbos): Handle user themes with toolbar background images here. This
    // would paint a few rows more of the background outside the toolbar and
    // then transition down to COLOR_TOOLBAR below using a gradient.
    canvas->DrawColor(
        view.GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR));

    // Paint the inner border around SidePanel content.
    // TODO(pbos): Revisit adding shadows here.
    cc::PaintFlags flags;
    flags.setStrokeWidth(floor(views::Separator::kThickness * dsf));
    flags.setColor(view.GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(scaled_bounds, corner_radius, flags);
  }

  gfx::Insets GetInsets() const override {
    return gfx::Insets(kBorderThickness + views::Separator::kThickness,
                       kBorderThickness, kBorderThickness, kBorderThickness);
  }
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(GetInsets().width(), GetInsets().height());
  }
};

class BorderView : public views::View {
 public:
  BorderView() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBorder(std::make_unique<SidePanelBorder>());
  }

  void Layout() override {
    // Let BorderView grow slightly taller so that it overlaps the divider into
    // the toolbar or bookmarks bar above it.
    gfx::Rect bounds = parent()->GetLocalBounds();
    bounds.Inset(gfx::Insets(-views::Separator::kThickness, 0, 0, 0));

    SetBoundsRect(bounds);
  }
};

}  // namespace

SidePanel::SidePanel()
    : border_view_(base::FeatureList::IsEnabled(features::kSidePanelBorder)
                       ? AddChildView(std::make_unique<BorderView>())
                       : nullptr) {
  SetVisible(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // TODO(pbos): Reconsider if SetPanelWidth() should add borders, if so move
  // accounting for the border into SetPanelWidth(), otherwise remove this TODO.
  constexpr int kDefaultWidth = 320;
  int default_width = kDefaultWidth;
  if (border_view_)
    default_width += 2 * kBorderThickness;
  SetPanelWidth(default_width);

  if (base::FeatureList::IsEnabled(features::kSidePanelBorder))
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kBorderThickness)));

  AddObserver(this);
}

SidePanel::~SidePanel() {
  RemoveObserver(this);
}

void SidePanel::SetPanelWidth(int width) {
  // Only the width is used by BrowserViewLayout.
  SetPreferredSize(gfx::Size(width, 1));
}

void SidePanel::ChildVisibilityChanged(View* child) {
  UpdateVisibility();
}

void SidePanel::OnChildViewAdded(View* observed_view, View* child) {
  UpdateVisibility();
  // Reorder `border_view_` to be last so that it gets painted on top, even if
  // an added child also paints to a layer.
  if (border_view_)
    ReorderChildView(border_view_, -1);
}

void SidePanel::OnChildViewRemoved(View* observed_view, View* child) {
  UpdateVisibility();
}

void SidePanel::UpdateVisibility() {
  // TODO(pbos): Iterate content instead. Requires moving the owned pointer out
  // of owned contents before resetting it.
  for (const auto* view : children()) {
    if (view == border_view_)
      continue;

    if (view->GetVisible()) {
      SetVisible(true);
      return;
    }
  }
  SetVisible(false);
}

BEGIN_METADATA(SidePanel, views::View)
END_METADATA
