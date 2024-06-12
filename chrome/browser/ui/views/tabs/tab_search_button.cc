// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kCRTabSearchCornerRadius = 10;
constexpr int kCRTabSearchFlatCornerRadius = 4;
}  // namespace

TabSearchButton::TabSearchButton(TabStripController* tab_strip_controller,
                                 Edge flat_edge)
    : TabStripControlButton(tab_strip_controller,
                            PressedCallback(),
                            vector_icons::kExpandMoreIcon,
                            flat_edge),
      tab_search_bubble_host_(std::make_unique<TabSearchBubbleHost>(
          this,
          tab_strip_controller->GetProfile())) {
  SetProperty(views::kElementIdentifierKey, kTabSearchButtonElementId);

  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_SEARCH));
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SEARCH));

  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

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
  return kCRTabSearchCornerRadius;
}

int TabSearchButton::GetFlatCornerRadius() const {
  return kCRTabSearchFlatCornerRadius;
}

BEGIN_METADATA(TabSearchButton)
END_METADATA
