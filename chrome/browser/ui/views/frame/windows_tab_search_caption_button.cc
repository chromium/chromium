// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/windows_tab_search_caption_button.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

WindowsTabSearchCaptionButton::WindowsTabSearchCaptionButton(
    GlassBrowserFrameView* frame_view,
    ViewID button_type,
    const std::u16string& accessible_name)
    : WindowsCaptionButton(views::Button::PressedCallback(),
                           frame_view,
                           button_type,
                           accessible_name),
      tab_search_bubble_host_(std::make_unique<TabSearchBubbleHost>(
          this,
          frame_view->browser_view()->GetProfile())) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetProperty(views::kElementIdentifierKey, kTabSearchButtonElementId);
  views::FocusRing::Get(this)->SetColorId(
      kColorTabSearchCaptionButtonFocusRing);
}

WindowsTabSearchCaptionButton::~WindowsTabSearchCaptionButton() = default;

BEGIN_METADATA(WindowsTabSearchCaptionButton, WindowsCaptionButton)
END_METADATA
