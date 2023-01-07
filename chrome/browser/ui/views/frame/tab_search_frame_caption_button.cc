// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_search_frame_caption_button.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

TabSearchFrameCaptionButton::TabSearchFrameCaptionButton(Profile* profile)
    : FrameCaptionButton(views::Button::PressedCallback(),
                         views::CAPTION_BUTTON_ICON_CUSTOM,
                         HTCLIENT),
      tab_search_bubble_host_(
          std::make_unique<TabSearchBubbleHost>(this, profile)) {
  SetImage(views::CAPTION_BUTTON_ICON_CUSTOM,
           views::FrameCaptionButton::Animate::kNo,
           vector_icons::kCaretDownIcon);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_SEARCH));
  SetProperty(views::kElementIdentifierKey, kTabSearchButtonElementId);
}

TabSearchFrameCaptionButton::~TabSearchFrameCaptionButton() = default;

// static.
bool TabSearchFrameCaptionButton::IsTabSearchCaptionButtonEnabled(
    Browser* browser) {
  return browser->is_type_normal() &&
         base::FeatureList::IsEnabled(
             features::kChromeOSTabSearchCaptionButton);
}

gfx::Rect TabSearchFrameCaptionButton::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = FrameCaptionButton::GetAnchorBoundsInScreen();
  bounds.Inset(GetInkdropInsets(size()));
  return bounds;
}

BEGIN_METADATA(TabSearchFrameCaptionButton, views::FrameCaptionButton)
END_METADATA
