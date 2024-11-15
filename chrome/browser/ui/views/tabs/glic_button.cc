// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

GlicButton::GlicButton(TabStripController* tab_strip_controller)
    : TabStripControlButton(tab_strip_controller,
                            PressedCallback(),
                            vector_icons::kErrorOutlineIcon) {
  SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);

  // TODO(iwells): Replace the values here, values are required to compile.
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

GlicButton::~GlicButton() = default;

void GlicButton::NotifyClick(const ui::Event& event) {
  TabStripControlButton::NotifyClick(event);
  // TODO(iwells): Implement. See tab search button as reference.
}

BEGIN_METADATA(GlicButton)
END_METADATA
