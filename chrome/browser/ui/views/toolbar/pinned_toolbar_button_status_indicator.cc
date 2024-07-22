// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"

#include <string>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/cascading_property.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

// static
PinnedToolbarButtonStatusIndicator* PinnedToolbarButtonStatusIndicator::Install(
    View* parent) {
  auto indicator = base::WrapUnique<PinnedToolbarButtonStatusIndicator>(
      new PinnedToolbarButtonStatusIndicator());
  indicator->SetPaintToLayer();
  indicator->layer()->SetFillsBoundsOpaquely(false);
  indicator->SetVisible(false);
  return parent->AddChildView(std::move(indicator));
}

PinnedToolbarButtonStatusIndicator*
PinnedToolbarButtonStatusIndicator::GetStatusIndicator(View* parent) {
  for (auto& child : parent->children()) {
    if (views::IsViewClass<PinnedToolbarButtonStatusIndicator>(child)) {
      return views::AsViewClass<PinnedToolbarButtonStatusIndicator>(child);
    }
  }
  return nullptr;
}

PinnedToolbarButtonStatusIndicator::~PinnedToolbarButtonStatusIndicator() =
    default;

void PinnedToolbarButtonStatusIndicator::SetColorId(
    ui::ColorId active_color_id,
    ui::ColorId inactive_color_id) {
  active_color_id_ = active_color_id;
  inactive_color_id_ = inactive_color_id;
  SchedulePaint();
}

void PinnedToolbarButtonStatusIndicator::Show() {
  SetVisible(true);
}

void PinnedToolbarButtonStatusIndicator::Hide() {
  SetVisible(false);
}

PinnedToolbarButtonStatusIndicator::PinnedToolbarButtonStatusIndicator() {
  // Don't allow the view to process events.
  SetCanProcessEventsWithinSubtree(false);
}

void PinnedToolbarButtonStatusIndicator::OnPaint(gfx::Canvas* canvas) {
  canvas->SaveLayerAlpha(SK_AlphaOPAQUE);
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(GetLocalBounds()), height() / 2,
                    height() / 2);

  cc::PaintFlags flags;
  std::optional<ui::ColorId> color_id = GetWidget()->ShouldPaintAsActive()
                                            ? active_color_id_
                                            : inactive_color_id_;

  flags.setColor(color_id.has_value()
                     ? GetColorProvider()->GetColor(color_id.value())
                     : GetCascadingAccentColor(this));
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(path, flags);
}

void PinnedToolbarButtonStatusIndicator::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &PinnedToolbarButtonStatusIndicator::PaintAsActiveChanged,
          base::Unretained(this)));
}

void PinnedToolbarButtonStatusIndicator::PaintAsActiveChanged() {
  SchedulePaint();
}

void PinnedToolbarButtonStatusIndicator::OnThemeChanged() {
  View::OnThemeChanged();
  SchedulePaint();
}

BEGIN_METADATA(PinnedToolbarButtonStatusIndicator)
END_METADATA
