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
#include "new_tab_button.h"
#include "tab_search_button.h"
#include "tab_strip_control_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kCRTabSearchCornerRadius = 10;
}

TabSearchButton::TabSearchButton(TabStrip* tab_strip)
    : TabStripControlButton(tab_strip,
                            PressedCallback(),
                            features::IsChromeRefresh2023()
                                ? vector_icons::kExpandMoreIcon
                                : vector_icons::kCaretDownIcon),
      tab_search_bubble_host_(std::make_unique<TabSearchBubbleHost>(
          this,
          tab_strip->controller()->GetProfile())) {
  SetProperty(views::kElementIdentifierKey, kTabSearchButtonElementId);

  UpdateForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  UpdateForegroundFrameInactiveColorId(
      kColorNewTabButtonForegroundFrameInactive);
  if (features::IsChromeRefresh2023()) {
    UpdateBackgroundFrameActiveColorId(
        kColorNewTabButtonCRBackgroundFrameActive);
    UpdateBackgroundFrameInactiveColorId(
        kColorNewTabButtonCRBackgroundFrameInactive);
  }

  const bool paint_transparent_for_custom_image_theme =
      features::IsChromeRefresh2023() ? false : true;
  SetPaintTransparentForCustomImageTheme(
      paint_transparent_for_custom_image_theme);

  UpdateColors();
}

TabSearchButton::~TabSearchButton() = default;

void TabSearchButton::NotifyClick(const ui::Event& event) {
  TabStripControlButton::NotifyClick(event);
  // Run pressed callback via MenuButtonController, instead of directly. This is
  // safe as the TabSearchBubbleHost will always configure the TabSearchButton
  // with a MenuButtonController.
  static_cast<views::MenuButtonController*>(button_controller())
      ->Activate(&event);
}

int TabSearchButton::GetCornerRadius() const {
  return features::IsChromeRefresh2023()
             ? kCRTabSearchCornerRadius
             : TabStripControlButton::kButtonSize.width() / 2;
}

BEGIN_METADATA(TabSearchButton, TabStripControlButton)
END_METADATA
