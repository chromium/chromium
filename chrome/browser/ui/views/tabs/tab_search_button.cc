// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

BEGIN_METADATA(TabSearchButton, NewTabButton)
END_METADATA
