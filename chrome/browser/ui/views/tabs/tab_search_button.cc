// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view_class_properties.h"

TabSearchButton::TabSearchButton(TabStrip* tab_strip)
    : NewTabButton(tab_strip, PressedCallback()),
      tab_search_bubble_host_(std::make_unique<TabSearchBubbleHost>(
          this,
          tab_strip->controller()->GetProfile())) {
  SetImageHorizontalAlignment(HorizontalAlignment::ALIGN_CENTER);
  SetImageVerticalAlignment(VerticalAlignment::ALIGN_MIDDLE);
  SetProperty(views::kElementIdentifierKey, kTabSearchButtonElementId);
}

TabSearchButton::~TabSearchButton() = default;

void TabSearchButton::NotifyClick(const ui::Event& event) {
  // Run pressed callback via MenuButtonController, instead of directly. This is
  // safe as the TabSearchBubbleHost will always configure the TabSearchButton
  // with a MenuButtonController.
  static_cast<views::MenuButtonController*>(button_controller())
      ->Activate(&event);
}

void TabSearchButton::FrameColorsChanged() {
  NewTabButton::FrameColorsChanged();
  // Icon color needs to be updated here as this is called when the hosting
  // window switches between active and inactive states. In each state the
  // foreground color of the tab controls is expected to change.
  SetImageModel(
      Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          base::FeatureList::IsEnabled(features::kTabSearchChevronIcon)
              ? vector_icons::kCaretDownIcon
              : kTabSearchIcon,
          GetForegroundColor()));
}

void TabSearchButton::PaintIcon(gfx::Canvas* canvas) {
  // Call ImageButton::PaintButtonContents() to paint the TabSearchButton's
  // VectorIcon.
  views::ImageButton::PaintButtonContents(canvas);
}

int TabSearchButton::GetCornerRadius() const {
  static int corner_radius = -1;

  if (corner_radius != -1) {
    return corner_radius;
  }

  if (features::IsChromeRefresh2023()) {
    corner_radius = 10;
  } else {
    corner_radius = NewTabButton::GetCornerRadius();
  }
  return corner_radius;
}

SkPath TabSearchButton::GetBorderPath(const gfx::Point& origin,
                                      float scale,
                                      bool extend_to_top) const {
  gfx::PointF scaled_origin(origin);
  scaled_origin.Scale(scale);
  float radius = GetCornerRadius() * scale;

  SkPath path;
  if (extend_to_top) {
    // revert to old NewTabButton radius
    radius = NewTabButton::GetCornerRadius() * scale;
    path.moveTo(scaled_origin.x(), 0);
    const float diameter = radius * 2;
    path.rLineTo(diameter, 0);
    path.rLineTo(0, scaled_origin.y() + radius);
    path.rArcTo(radius, radius, 0, SkPath::kSmall_ArcSize, SkPathDirection::kCW,
                -diameter, 0);
    path.close();
  } else {
    path.addRRect(SkRRect::MakeRectXY(
        SkRect::MakeXYWH(scaled_origin.x(), scaled_origin.y(), 28 * scale,
                         28 * scale),
        radius, radius));
  }
  return path;
}

BEGIN_METADATA(TabSearchButton, NewTabButton)
END_METADATA
