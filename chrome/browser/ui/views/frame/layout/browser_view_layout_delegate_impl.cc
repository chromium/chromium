// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate_impl.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/features.h"
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
  return ShouldDrawTabStrip() && tabs::IsVerticalTabsFeatureEnabled() &&
         browser_view_->browser()
             ->browser_window_features()
             ->vertical_tab_strip_state_controller()
             ->ShouldDisplayVerticalTabs();
}

bool BrowserViewLayoutDelegateImpl::ShouldDrawWebAppFrameToolbar() const {
  return !GetBorderlessModeEnabled() &&
         GetFrameView()->ShouldShowWebAppFrameToolbar();
}

bool BrowserViewLayoutDelegateImpl::GetBorderlessModeEnabled() const {
  return browser_view_->IsBorderlessModeEnabled();
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

int BrowserViewLayoutDelegateImpl::GetTopInsetInBrowserView() const {
  // BrowserView should fill the full window when window controls overlay
  // is enabled or when immersive fullscreen with tabs is enabled.
  if (browser_view_->IsWindowControlsOverlayEnabled() ||
      browser_view_->IsBorderlessModeEnabled()) {
    return 0;
  }
#if BUILDFLAG(IS_MAC)
  if (browser_view_->UsesImmersiveFullscreenTabbedMode() &&
      GetImmersiveModeController()->IsEnabled()) {
    return 0;
  }
#endif

  if (auto* const frame_view = GetFrameView()) {
    return frame_view->GetTopInset(false) - browser_view_->y();
  }

  return 0;
}

void BrowserViewLayoutDelegateImpl::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  return GetFrameView()->LayoutWebAppWindowTitle(available_space,
                                                 window_title_label);
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

bool BrowserViewLayoutDelegateImpl::SupportsWindowFeature(
    Browser::WindowFeature feature) const {
  return browser_view_->browser()->SupportsWindowFeature(feature);
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

gfx::Rect
BrowserViewLayoutDelegateImpl::GetBoundsForTabStripRegionInBrowserView() const {
  const gfx::Size tabstrip_minimum_size =
      browser_view().tab_strip_view()->GetMinimumSize();
  const auto layout = GetFrameView()->GetBrowserLayoutParams();
  gfx::RectF bounds_f = gfx::RectF(layout.visual_client_area);
  const float max_bound =
      std::max(layout.leading_exclusion.content.height() +
                   layout.leading_exclusion.vertical_padding,
               layout.trailing_exclusion.content.height() +
                   layout.trailing_exclusion.vertical_padding);
  bounds_f.set_height(
      std::max(max_bound, static_cast<float>(tabstrip_minimum_size.height())));
  const int tab_margin = TabStyle::Get()->GetBottomCornerRadius();
  bounds_f.Inset(gfx::InsetsF::TLBR(
      0.0f,
      layout.leading_exclusion.content.width() +
          std::max(0.0f,
                   layout.leading_exclusion.horizontal_padding - tab_margin),
      0.0f,
      layout.trailing_exclusion.content.width() +
          std::max(0.0f,
                   layout.trailing_exclusion.horizontal_padding - tab_margin)));
  views::View::ConvertRectToTarget(browser_view().parent(), &browser_view(),
                                   &bounds_f);
  return gfx::ToEnclosingRect(bounds_f);
}

gfx::Rect
BrowserViewLayoutDelegateImpl::GetBoundsForToolbarInVerticalTabBrowserView()
    const {
  const gfx::Size toolbar_preferred_size =
      browser_view().toolbar()->GetPreferredSize();
  const auto layout = GetFrameView()->GetBrowserLayoutParams();
  gfx::RectF bounds_f(gfx::RectF(layout.visual_client_area));
  const float max_bound =
      std::max(layout.leading_exclusion.content.height() +
                   layout.leading_exclusion.vertical_padding,
               layout.trailing_exclusion.content.height() +
                   layout.trailing_exclusion.vertical_padding);
  bounds_f.set_height(
      std::max(max_bound, static_cast<float>(toolbar_preferred_size.height())));
  bounds_f.Inset(
      gfx::InsetsF::TLBR(0.0f,
                         layout.leading_exclusion.content.width() +
                             layout.leading_exclusion.horizontal_padding,
                         0.0f,
                         layout.trailing_exclusion.content.width() +
                             layout.trailing_exclusion.horizontal_padding));
  views::View::ConvertRectToTarget(browser_view().parent(), &browser_view(),
                                   &bounds_f);
  return gfx::ToEnclosingRect(bounds_f);
}

gfx::Rect
BrowserViewLayoutDelegateImpl::GetBoundsForWebAppFrameToolbarInBrowserView()
    const {
  if (!GetFrameView()->ShouldShowWebAppFrameToolbar()) {
    return gfx::Rect();
  }

  const gfx::Size web_app_frame_toolbar_preferred_size =
      browser_view().web_app_frame_toolbar()->GetPreferredSize();

  const auto layout = GetFrameView()->GetBrowserLayoutParams();
  gfx::RectF bounds_f = gfx::RectF(layout.visual_client_area);
  // Note: on Mac in fullscreen these exclusions have zero width, but may still
  // have nonzero height to ensure that the top area has the same height as it
  // would have had if they were present; see https://crbug.com/450817281 for
  // why this is needed.
  const float max_bound =
      std::max(layout.leading_exclusion.content.height() +
                   layout.leading_exclusion.vertical_padding,
               layout.trailing_exclusion.content.height() +
                   layout.trailing_exclusion.vertical_padding);
  bounds_f.set_height(std::max(
      max_bound,
      static_cast<float>(web_app_frame_toolbar_preferred_size.height())));
  bounds_f.Inset(
      gfx::InsetsF::TLBR(0.0f,
                         layout.leading_exclusion.content.width() +
                             layout.leading_exclusion.horizontal_padding,
                         0.0f,
                         layout.trailing_exclusion.content.width() +
                             layout.trailing_exclusion.horizontal_padding));

  views::View::ConvertRectToTarget(browser_view().parent(), &browser_view(),
                                   &bounds_f);
  return gfx::ToEnclosingRect(bounds_f);
}
