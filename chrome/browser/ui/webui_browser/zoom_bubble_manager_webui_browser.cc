// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/zoom_bubble_manager_webui_browser.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

ZoomBubbleManagerWebUIBrowser::ZoomBubbleManagerWebUIBrowser(
    WebUIBrowserWindow* window)
    : window_(window) {}

ZoomBubbleManagerWebUIBrowser::~ZoomBubbleManagerWebUIBrowser() = default;

views::BubbleAnchor ZoomBubbleManagerWebUIBrowser::GetZoomBubbleAnchor() {
  // TODO(webium): Align with BrowserView's PageAction based anchoring &
  // immersive mode consideration once those are implemented for WebUI browser.
  // Currently, anchoring to location bar which is best suited.

  ui::TrackedElement* location_bar = BrowserElements::From(window_->browser())
                                         ->GetElement(kLocationBarElementId);
  if (location_bar) {
    return views::BubbleAnchor(location_bar);
  }

  return views::BubbleAnchor();
}

gfx::NativeView ZoomBubbleManagerWebUIBrowser::GetNativeView() {
  return window_->widget()->GetNativeView();
}

void ZoomBubbleManagerWebUIBrowser::UpdateLegacyPageActionIcon() {}

std::u16string ZoomBubbleManagerWebUIBrowser::GetZoomActionAccessibleName() {
  // TODO(webium): Fetch accessible name from the page action view once
  // it's supported for webium.
  return l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM_SET_DEFAULT);
}
