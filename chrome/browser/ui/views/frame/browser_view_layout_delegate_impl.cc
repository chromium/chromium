// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout_delegate_impl.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "ui/views/view.h"

// static
std::unique_ptr<BrowserViewLayoutDelegate>
BrowserViewLayoutDelegateImplBase::CreateDelegate(BrowserView& browser_view) {
  if (base::FeatureList::IsEnabled(features::kDesktopNewTopAreaLayoutFeature)) {
    return std::make_unique<BrowserViewLayoutDelegateImplNew>(browser_view);
  }
  return std::make_unique<BrowserViewLayoutDelegateImplOld>(browser_view);
}

BrowserViewLayoutDelegateImplBase::BrowserViewLayoutDelegateImplBase(
    BrowserView& browser_view)
    : browser_view_(browser_view) {}
BrowserViewLayoutDelegateImplBase::~BrowserViewLayoutDelegateImplBase() =
    default;

bool BrowserViewLayoutDelegateImplBase::ShouldDrawTabStrip() const {
  return browser_view_->ShouldDrawTabStrip();
}

bool BrowserViewLayoutDelegateImplBase::GetBorderlessModeEnabled() const {
  return browser_view_->IsBorderlessModeEnabled();
}

int BrowserViewLayoutDelegateImplBase::GetTopInsetInBrowserView() const {
  // BrowserView should fill the full window when window controls overlay
  // is enabled or when immersive fullscreen with tabs is enabled.
  if (browser_view_->IsWindowControlsOverlayEnabled() ||
      browser_view_->IsBorderlessModeEnabled()) {
    return 0;
  }
#if BUILDFLAG(IS_MAC)
  if (browser_view_->UsesImmersiveFullscreenTabbedMode() &&
      browser_view_->immersive_mode_controller()->IsEnabled()) {
    return 0;
  }
#endif

  return browser_view_->browser_widget()->GetTopInset() - browser_view_->y();
}

bool BrowserViewLayoutDelegateImplBase::IsToolbarVisible() const {
  return browser_view_->IsToolbarVisible();
}

bool BrowserViewLayoutDelegateImplBase::IsBookmarkBarVisible() const {
  return browser_view_->IsBookmarkBarVisible();
}

bool BrowserViewLayoutDelegateImplBase::IsContentsSeparatorEnabled() const {
  // Web app windows manage their own separator.
  // TODO(crbug.com/40102629): Make PWAs set the visibility of the ToolbarView
  // based on whether it is visible instead of setting the height to 0px. This
  // will enable BrowserViewLayout to hide the contents separator on its own
  // using the same logic used by normal BrowserElementsViews.
  return !browser_view_->browser()->app_controller();
}

bool BrowserViewLayoutDelegateImplBase::IsActiveTabSplit() const {
  // Use the model state as this can be called during active tab change
  // when the multi contents view hasn't been fully setup and this
  // inconsistency would cause unnecessary re-layout of content view during
  // tab switch.
  return browser_view_->browser()->tab_strip_model()->IsActiveTabSplit();
}

const ImmersiveModeController*
BrowserViewLayoutDelegateImplBase::GetImmersiveModeController() const {
  return browser_view_->immersive_mode_controller();
}

ExclusiveAccessBubbleViews*
BrowserViewLayoutDelegateImplBase::GetExclusiveAccessBubble() const {
  return browser_view_->exclusive_access_bubble();
}

bool BrowserViewLayoutDelegateImplBase::IsTopControlsSlideBehaviorEnabled()
    const {
  return browser_view_->GetTopControlsSlideBehaviorEnabled();
}

float BrowserViewLayoutDelegateImplBase::GetTopControlsSlideBehaviorShownRatio()
    const {
  return browser_view_->GetTopControlsSlideBehaviorShownRatio();
}

bool BrowserViewLayoutDelegateImplBase::SupportsWindowFeature(
    Browser::WindowFeature feature) const {
  return browser_view_->browser()->SupportsWindowFeature(feature);
}

gfx::NativeView BrowserViewLayoutDelegateImplBase::GetHostViewForAnchoring()
    const {
  return browser_view_->GetWidgetForAnchoring()->GetNativeView();
}

bool BrowserViewLayoutDelegateImplBase::HasFindBarController() const {
  return browser_view_->browser()->GetFeatures().HasFindBarController();
}

void BrowserViewLayoutDelegateImplBase::MoveWindowForFindBarIfNecessary()
    const {
  auto* const controller =
      browser_view_->browser()->GetFeatures().GetFindBarController();
  return controller->find_bar()->MoveWindowIfNecessary();
}

bool BrowserViewLayoutDelegateImplBase::IsWindowControlsOverlayEnabled() const {
  return browser_view_->IsWindowControlsOverlayEnabled();
}

void BrowserViewLayoutDelegateImplBase::UpdateWindowControlsOverlay(
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

bool BrowserViewLayoutDelegateImplBase::ShouldLayoutTabStrip() const {
#if BUILDFLAG(IS_MAC)
  // The tab strip is hosted in a separate widget in immersive fullscreen on
  // macOS.
  if (browser_view_->UsesImmersiveFullscreenTabbedMode() &&
      browser_view_->immersive_mode_controller()->IsEnabled()) {
    return false;
  }
#endif
  return true;
}

int BrowserViewLayoutDelegateImplBase::GetExtraInfobarOffset() const {
#if BUILDFLAG(IS_MAC)
  if (browser_view_->UsesImmersiveFullscreenMode() &&
      browser_view_->immersive_mode_controller()->IsEnabled()) {
    return browser_view_->immersive_mode_controller()->GetExtraInfobarOffset();
  }
#endif
  return 0;
}

const BrowserFrameView* BrowserViewLayoutDelegateImplBase::GetFrameView()
    const {
  return browser_view_->browser_widget()->GetFrameView();
}

BrowserViewLayoutDelegateImplOld::BrowserViewLayoutDelegateImplOld(
    BrowserView& browser_view)
    : BrowserViewLayoutDelegateImplBase(browser_view) {}
BrowserViewLayoutDelegateImplOld::~BrowserViewLayoutDelegateImplOld() = default;

gfx::Rect
BrowserViewLayoutDelegateImplOld::GetBoundsForTabStripRegionInBrowserView()
    const {
  const gfx::Size tabstrip_minimum_size =
      browser_view().tab_strip_view()->GetMinimumSize();
  gfx::RectF bounds_f = gfx::RectF(
      GetFrameView()->GetBoundsForTabStripRegion(tabstrip_minimum_size));
  views::View::ConvertRectToTarget(browser_view().parent(), &browser_view(),
                                   &bounds_f);
  return gfx::ToEnclosingRect(bounds_f);
}

gfx::Rect
BrowserViewLayoutDelegateImplOld::GetBoundsForToolbarInVerticalTabBrowserView()
    const {
  // When vertical tabs is enabled, the top element becomes the toolbar.
  // Because of this, it must now be aware of the location of the caption
  // buttons. We can reuse the calculation use by the TabStripRegionView to
  // get this information until we have a way to directly query for the
  // caption button location directly.
  return GetBoundsForTabStripRegionInBrowserView();
}

gfx::Rect
BrowserViewLayoutDelegateImplOld::GetBoundsForWebAppFrameToolbarInBrowserView()
    const {
  const gfx::Size web_app_frame_toolbar_preferred_size =
      browser_view().web_app_frame_toolbar()->GetPreferredSize();
  gfx::RectF bounds_f =
      gfx::RectF(GetFrameView()->GetBoundsForWebAppFrameToolbar(
          web_app_frame_toolbar_preferred_size));
  views::View::ConvertRectToTarget(browser_view().parent(), &browser_view(),
                                   &bounds_f);
  return gfx::ToEnclosingRect(bounds_f);
}

BrowserViewLayoutDelegateImplNew::BrowserViewLayoutDelegateImplNew(
    BrowserView& browser_view)
    : BrowserViewLayoutDelegateImplBase(browser_view) {}
BrowserViewLayoutDelegateImplNew::~BrowserViewLayoutDelegateImplNew() = default;

gfx::Rect
BrowserViewLayoutDelegateImplNew::GetBoundsForTabStripRegionInBrowserView()
    const {
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
BrowserViewLayoutDelegateImplNew::GetBoundsForToolbarInVerticalTabBrowserView()
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
BrowserViewLayoutDelegateImplNew::GetBoundsForWebAppFrameToolbarInBrowserView()
    const {
  if (browser_view().ShouldHideUIForFullscreen()) {
    return gfx::Rect();
  }
  const gfx::Size web_app_frame_toolbar_preferred_size =
      browser_view().web_app_frame_toolbar()->GetPreferredSize();

  const auto layout = GetFrameView()->GetBrowserLayoutParams();
  gfx::RectF bounds_f = gfx::RectF(layout.visual_client_area);
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
