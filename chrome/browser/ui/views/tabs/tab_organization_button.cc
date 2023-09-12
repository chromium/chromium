// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_organization_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTabOrganizeCornerRadius = 10;
}

TabOrganizationButton::TabOrganizationButton(TabStrip* tab_strip)
    : TabStripControlButton(tab_strip,
                            PressedCallback(),  // Tab organize callback
                            kPaintbrushIcon) {
  SetProperty(views::kElementIdentifierKey, kTabOrganizationButtonElementId);

  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_ORGANIZE));

  UpdateForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  UpdateForegroundFrameInactiveColorId(
      kColorNewTabButtonForegroundFrameInactive);
  UpdateBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  UpdateBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  SetPaintTransparentForCustomImageTheme(false);

  UpdateColors();
}

TabOrganizationButton::~TabOrganizationButton() = default;

int TabOrganizationButton::GetCornerRadius() const {
  return kTabOrganizeCornerRadius;
}

BEGIN_METADATA(TabOrganizationButton, TabStripControlButton)
END_METADATA
