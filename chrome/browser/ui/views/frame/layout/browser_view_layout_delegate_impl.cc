// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate_impl.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/common/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif

BrowserViewLayoutDelegateImpl::BrowserViewLayoutDelegateImpl(
    BrowserView& browser_view)
    : browser_view_(browser_view) {}
BrowserViewLayoutDelegateImpl::~BrowserViewLayoutDelegateImpl() = default;

bool BrowserViewLayoutDelegateImpl::ShouldDrawTabStrip() const {
  return browser_view_->ShouldDrawTabStrip();
}

bool BrowserViewLayoutDelegateImpl::ShouldUseTouchableTabstrip() const {
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  return WebUITabStripContainerView::UseTouchableTabStrip(
             browser_view_->browser()) &&
         browser_view_->GetSupportsTabStrip();
#else
  return false;
#endif
}

bool BrowserViewLayoutDelegateImpl::ShouldDrawVerticalTabStrip() const {
  return browser_view_->ShouldDrawVerticalTabStrip();
}

bool BrowserViewLayoutDelegateImpl::IsVerticalTabStripCollapsed() const {
  return browser_view_->IsVerticalTabStripCollapsed();
}

bool BrowserViewLayoutDelegateImpl::ShouldDrawWebAppFrameToolbar() const {
  return browser_view_->ShouldDrawWebAppFrameToolbar();
}

bool BrowserViewLayoutDelegateImpl::GetBorderlessModeEnabled() const {
  return browser_view_->IsUnframedModeEnabled();
}

BrowserLayoutParams BrowserViewLayoutDelegateImpl::GetBrowserLayoutParams(
    bool use_browser_bounds) const {
  const auto params = GetFrameView()->GetBrowserLayoutParams();
  if (params.IsEmpty()) {
    // This can happen sometimes right after a browser is created.
    return params;
  }
  return params.InLocalCoordinates(
      use_browser_bounds ? browser_view_->bounds() : params.visual_client_area);
}

BrowserViewLayoutDelegateImpl::WindowState
BrowserViewLayoutDelegateImpl::GetBrowserWindowState() const {
  if (browser_view_->IsFullscreen()) {
    return WindowState::kFullscreen;
  }
  if (browser_view_->IsMaximized()) {
    return WindowState::kMaximized;
  }
  return WindowState::kNormal;
}

views::LayoutAlignment BrowserViewLayoutDelegateImpl::GetWindowTitleAlignment()
    const {
  return GetFrameView()->GetWindowTitleAlignment();
}

bool BrowserViewLayoutDelegateImpl::IsToolbarVisible() const {
  return browser_view_->IsToolbarVisible();
}

bool BrowserViewLayoutDelegateImpl::IsBookmarkBarVisible() const {
  return browser_view_->IsBookmarkBarVisible();
}

bool BrowserViewLayoutDelegateImpl::IsInfobarVisible() const {
  auto* const container = browser_view_->infobar_container();
  if (!container || container->IsEmpty()) {
    return false;
  }
  if (browser_view_->GetWidget()->IsFullscreen()) {
    return !container->ShouldHideInFullscreen();
  }
  return true;
}

bool BrowserViewLayoutDelegateImpl::IsContentsSeparatorEnabled() const {
  // Web app windows manage their own separator.
  // TODO(crbug.com/40102629): Make PWAs set the visibility of the ToolbarView
  // based on whether it is visible instead of setting the height to 0px. This
  // will enable BrowserViewLayout to hide the contents separator on its own
  // using the same logic used by normal BrowserElementsViews.
  return !browser_view_->browser()->app_controller();
}

bool BrowserViewLayoutDelegateImpl::IsActiveTabSplit() const {
  // Use the model state as this can be called during active tab change
  // when the multi contents view hasn't been fully setup and this
  // inconsistency would cause unnecessary re-layout of content view during
  // tab switch.
  return browser_view_->browser()->tab_strip_model()->IsActiveTabSplit();
}

bool BrowserViewLayoutDelegateImpl::IsActiveTabAtLeadingWindowEdge() const {
  if (auto* const frame = GetFrameView()) {
    const bool has_leading_search_button =
        tabs::GetTabSearchPosition(browser_view_->browser()) ==
        tabs::TabSearchPosition::kLeadingHorizontalTabstrip;
    if (!frame->CaptionButtonsOnLeadingEdge() && !has_leading_search_button) {
      return browser_view_->browser()->tab_strip_model()->IsTabInForeground(0);
    }
  }
  return false;
}

const ImmersiveModeController*
BrowserViewLayoutDelegateImpl::GetImmersiveModeController() const {
  return ImmersiveModeController::From(browser_view_->browser());
}

ExclusiveAccessBubbleViews*
BrowserViewLayoutDelegateImpl::GetExclusiveAccessBubble() const {
  return browser_view_->GetExclusiveAccessBubble();
}

bool BrowserViewLayoutDelegateImpl::IsTopControlsSlideBehaviorEnabled() const {
  return browser_view_->GetTopControlsSlideBehaviorEnabled();
}

float BrowserViewLayoutDelegateImpl::GetTopControlsSlideBehaviorShownRatio()
    const {
  return browser_view_->GetTopControlsSlideBehaviorShownRatio();
}

gfx::NativeView BrowserViewLayoutDelegateImpl::GetHostViewForAnchoring() const {
  return browser_view_->GetWidgetForAnchoring()->GetNativeView();
}

bool BrowserViewLayoutDelegateImpl::HasFindBarController() const {
  return browser_view_->browser()->GetFeatures().HasFindBarController();
}

void BrowserViewLayoutDelegateImpl::MoveWindowForFindBarIfNecessary() const {
  auto* const controller =
      browser_view_->browser()->GetFeatures().GetFindBarController();
  return controller->find_bar()->MoveWindowIfNecessary();
}

bool BrowserViewLayoutDelegateImpl::IsWindowControlsOverlayEnabled() const {
  return browser_view_->IsWindowControlsOverlayEnabled();
}

void BrowserViewLayoutDelegateImpl::UpdateWindowControlsOverlay(
    const gfx::Rect& available_titlebar_area) {
  content::WebContents* web_contents = browser_view_->GetActiveWebContents();
  if (!web_contents) {
    return;
  }

  // The rect passed to WebContents is directly exposed to websites. In case
  // of an empty rectangle, this should be exposed as 0,0 0x0 rather than
  // whatever coordinates might be in rect.
  web_contents->UpdateWindowControlsOverlay(
      available_titlebar_area.IsEmpty()
          ? gfx::Rect()
          : browser_view_->GetMirroredRect(available_titlebar_area));
}

bool BrowserViewLayoutDelegateImpl::ShouldLayoutTabStrip() const {
#if BUILDFLAG(IS_MAC)
  // The tab strip is hosted in a separate widget in immersive fullscreen on
  // macOS.
  if (browser_view_->UsesImmersiveFullscreenTabbedMode() &&
      GetImmersiveModeController()->IsEnabled()) {
    return false;
  }
#endif
  return true;
}

int BrowserViewLayoutDelegateImpl::GetExtraInfobarOffset() const {
#if BUILDFLAG(IS_MAC)
  auto* const controller = GetImmersiveModeController();
  if (browser_view_->UsesImmersiveFullscreenMode() && controller->IsEnabled()) {
    return controller->GetExtraInfobarOffset();
  }
#endif
  return 0;
}

const BrowserFrameView* BrowserViewLayoutDelegateImpl::GetFrameView() const {
  return browser_view_->browser_widget()
             ? browser_view_->browser_widget()->GetFrameView()
             : nullptr;
}
