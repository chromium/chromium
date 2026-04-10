// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_manager_views.h"

#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

ZoomBubbleManagerViews::ZoomBubbleManagerViews(BrowserView* browser_view)
    : browser_view_(browser_view) {}

ZoomBubbleManagerViews::~ZoomBubbleManagerViews() = default;

views::BubbleAnchor ZoomBubbleManagerViews::GetZoomBubbleAnchor() {
  CHECK(browser_view_);

#if BUILDFLAG(IS_MAC)
  if (fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
    return views::BubbleAnchor();
  }
#endif
  auto* immersive_mode_controller =
      ImmersiveModeController::From(browser_view_->browser());

  // We intentionally do not show the immersive frame for zoom bubble.
  if (!browser_view_->GetWidget()->IsFullscreen() ||
      (browser_view_->IsToolbarVisible() &&
       (!immersive_mode_controller->IsEnabled() ||
        immersive_mode_controller->IsRevealed()))) {
    return browser_view_->toolbar_button_provider()->GetBubbleAnchor(
        kActionZoomNormal);
  }
  return views::BubbleAnchor();
}

gfx::NativeView ZoomBubbleManagerViews::GetNativeView() {
  return browser_view_->GetWidget()->GetNativeView();
}

void ZoomBubbleManagerViews::UpdateLegacyPageActionIcon() {
  browser_view_->browser()->window()->UpdatePageActionIcon(
      PageActionIconType::kZoom);
}

std::u16string ZoomBubbleManagerViews::GetZoomActionAccessibleName() {
  ToolbarButtonProvider* provider = browser_view_->toolbar_button_provider();
  return provider->GetPageActionView(kActionZoomNormal)->GetAccessibleName();
}
