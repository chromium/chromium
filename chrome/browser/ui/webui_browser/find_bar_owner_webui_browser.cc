// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/find_bar_owner_webui_browser.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

FindBarOwnerWebUIBrowser::FindBarOwnerWebUIBrowser(WebUIBrowserWindow* window)
    : window_(window) {}

FindBarOwnerWebUIBrowser::~FindBarOwnerWebUIBrowser() = default;

views::Widget* FindBarOwnerWebUIBrowser::GetOwnerWidget() {
  return window_->widget();
}

gfx::Rect FindBarOwnerWebUIBrowser::GetFindBarBoundingBox() {
  if (!window_->browser()->SupportsWindowFeature(
          Browser::WindowFeature::kFeatureLocationBar)) {
    return gfx::Rect();
  }

  ui::TrackedElement* location_bar = BrowserElements::From(window_->browser())
                                         ->GetElement(kLocationBarElementId);
  if (!location_bar) {
    return gfx::Rect();
  }

  ui::TrackedElement* contents_container =
      BrowserElements::From(window_->browser())
          ->GetElement(kContentsContainerViewElementId);
  CHECK(contents_container);

  gfx::Rect location_bar_bounds = location_bar->GetScreenBounds();
  gfx::Rect contents_container_bounds = contents_container->GetScreenBounds();
  // The bounding box spans vertically from the bottom of the location bar to
  // the bottom of the contents container and has the same width as the location
  // bar.
  gfx::Rect bounding_box_in_screen(
      location_bar_bounds.x(), location_bar_bounds.bottom(),
      location_bar_bounds.width(),
      contents_container_bounds.bottom() - location_bar_bounds.bottom());
  gfx::Rect bounding_box_in_owner =
      bounding_box_in_screen -
      window_->widget()->GetWindowBoundsInScreen().OffsetFromOrigin();
  return bounding_box_in_owner;
}

gfx::Rect FindBarOwnerWebUIBrowser::GetFindBarClippingBox() {
  return window_->widget()->client_view()->bounds();
}

bool FindBarOwnerWebUIBrowser::IsOffTheRecord() const {
  return window_->browser()->profile()->IsOffTheRecord();
}

views::Widget* FindBarOwnerWebUIBrowser::GetWidgetForAnchoring() {
  return window_->widget();
}

std::u16string FindBarOwnerWebUIBrowser::GetFindBarAccessibleWindowTitle() {
  return l10n_util::GetStringFUTF16(
      IDS_FIND_IN_PAGE_ACCESSIBLE_TITLE,
      window_->browser()->GetWindowTitleForCurrentTab(false));
}

void FindBarOwnerWebUIBrowser::OnFindBarVisibilityChanged(
    gfx::Rect visible_bounds) {
  window_->browser()->OnFindBarVisibilityChanged();
}

void FindBarOwnerWebUIBrowser::CloseOverlappingBubbles() {
  if (TranslateBubbleController* controller =
      TranslateBubbleController::From(window_->browser())) {
    controller->CloseBubble();
  }
}
