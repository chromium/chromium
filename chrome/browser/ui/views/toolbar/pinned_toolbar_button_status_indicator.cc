// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"

#include <string>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/cascading_property.h"
#include "ui/views/view_utils.h"

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

PinnedToolbarButtonStatusIndicator::~PinnedToolbarButtonStatusIndicator() =
    default;

void PinnedToolbarButtonStatusIndicator::SetColor(SkColor color) {
  color_ = color;
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
  flags.setColor(color_.value_or(GetCascadingAccentColor(this)));
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(path, flags);
}

void PinnedToolbarButtonStatusIndicator::OnThemeChanged() {
  View::OnThemeChanged();
  SchedulePaint();
}

BEGIN_METADATA(PinnedToolbarButtonStatusIndicator)
END_METADATA
