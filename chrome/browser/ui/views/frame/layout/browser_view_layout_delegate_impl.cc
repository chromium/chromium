// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate_impl.h"

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/frame/glass_frame_service.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#endif

BrowserViewLayoutDelegateImpl::BrowserViewLayoutDelegateImpl(
    BrowserView& browser_view)
    : browser_view_(browser_view) {
    PrefService* prefs = browser_view_->GetProfile()->GetPrefs();
    tab_search_pinned_to_tab_strip_ =
        prefs->GetBoolean(prefs::kTabSearchPinnedToTabstrip);

    pref_registrar_.Init(prefs);
    pref_registrar_.Add(
        prefs::kTabSearchPinnedToTabstrip,
        base::BindRepeating(
            &BrowserViewLayoutDelegateImpl::OnTabSearchPinnedStateChanged,
            base::Unretained(this)));
}
BrowserViewLayoutDelegateImpl::~BrowserViewLayoutDelegateImpl() = default;

BrowserViewLayoutDelegate::TabStripType
BrowserViewLayoutDelegateImpl::GetTabStripType() const {
  // Can there be a tabstrip at all right now?
  if (!browser_view_->ShouldDrawTabStrip()) {
    return TabStripType::kNone;
  }

  // Determine if there is a vertical tab strip.
  if (browser_view_->browser()->GetType() ==
      BrowserWindowInterface::Type::TYPE_NORMAL) {
    const auto* const controller =
        tabs::VerticalTabStripStateController::From(browser_view_->browser());
    if (controller && controller->ShouldDisplayVerticalTabs()) {
#if BUILDFLAG(IS_MAC)
      // Do not lay out the vertical tabstrip in content-fullscreen on Mac. This
      // check cannot be done in BrowserView because the immersive mode
      // controller itself relies on BrowserView reporting which tab strip it
      // *would* draw, creating a circular dependency/race condition.
      if (fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
        return TabStripType::kNone;
      }
#endif
      return TabStripType::kVertical;
    }
  }

  // Default is horizontal tab strip.
  return TabStripType::kHorizontal;
}

bool BrowserViewLayoutDelegateImpl::IsVerticalTabStripCollapsed() const {
  return browser_view_->IsVerticalTabStripCollapsed();
}

bool BrowserViewLayoutDelegateImpl::ShouldDrawWebAppFrameToolbar() const {
  return browser_view_->ShouldDrawWebAppFrameToolbar();
}

bool BrowserViewLayoutDelegateImpl::GetUnframedModeEnabled() const {
  return browser_view_->IsUnframedModeEnabled();
}

BrowserLayoutParams BrowserViewLayoutDelegateImpl::GetBrowserLayoutParams(
    bool use_browser_bounds) const {
  if (auto* const frame = GetFrameView()) {
    const auto params = frame->GetBrowserLayoutParams();
    if (params.IsEmpty()) {
      // This can happen sometimes right after a browser is created.
      return params;
    }
    const gfx::Rect browser_bounds = browser_view_->bounds();
#if BUILDFLAG(IS_CHROMEOS)
    // Sometimes in kiosk mode the browser bounds briefly fail to line up with
    // the client area. Rather than allowing this to crash in
    // `InLocalCoordinates()`, just use the client bounds instead. The worst
    // outcome in this case is that some minimum size calculations may be off by
    // a few pixels for one frame.
    // See https://crbug.com/506933210 for more information.
    if (use_browser_bounds &&
        !params.visual_client_area.Contains(browser_bounds)) {
      use_browser_bounds = false;
    }
#endif
    return params.InLocalCoordinates(
        use_browser_bounds ? browser_bounds : params.visual_client_area);
  }

  return BrowserLayoutParams();
}

BrowserViewLayoutDelegateImpl::WindowState
BrowserViewLayoutDelegateImpl::GetBrowserWindowState() const {
  if (browser_view_->IsFullscreen()) {
#if BUILDFLAG(IS_MAC)
    if (fullscreen_utils::IsAlwaysShowToolbarEnabled(
            browser_view_->browser()) &&
        !fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
      return WindowState::kFullscreenWithToolbar;
    }
#endif
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
  return !web_app::AppBrowserController::From(browser_view_->browser());
}

bool BrowserViewLayoutDelegateImpl::IsActiveTabSplit() const {
  // Use the model state as this can be called during active tab change
  // when the multi contents view hasn't been fully setup and this
  // inconsistency would cause unnecessary re-layout of content view during
  // tab switch.
  auto* const active_tab =
      browser_view_->browser()->GetTabStripModel()->GetActiveTab();
  return active_tab && active_tab->IsSplit();
}

bool BrowserViewLayoutDelegateImpl::IsActiveTabAtLeadingWindowEdge() const {
  if (auto* const frame = GetFrameView()) {
    bool has_leading_search_button =
        tabs::GetTabSearchPosition(browser_view_->browser()) ==
        tabs::TabSearchPosition::kLeadingHorizontalTabstrip;
    // Tab search button can be unpinned so including it in the determination
    // of leading edge of horizontal tab strip.
    has_leading_search_button &= tab_search_pinned_to_tab_strip_;
    if (!frame->CaptionButtonsOnLeadingEdge() && !has_leading_search_button) {
      return browser_view_->browser()->GetTabStripModel()->IsTabInForeground(0);
    }
  }
  return false;
}

const ImmersiveModeController*
BrowserViewLayoutDelegateImpl::GetImmersiveModeController() const {
  return ImmersiveModeController::From(browser_view_->browser());
}

BrowserAnimationController*
BrowserViewLayoutDelegateImpl::GetAnimationController() const {
  return BrowserAnimationController::From(browser_view_->browser());
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
  if (WindowFeatureController::From(browser_view_->browser())
          ->UsesImmersiveFullscreenTabbedMode() &&
      GetImmersiveModeController()->IsEnabled()) {
    return false;
  }
#endif
  return true;
}

int BrowserViewLayoutDelegateImpl::GetExtraInfobarOffset() const {
#if BUILDFLAG(IS_MAC)
  auto* const controller = GetImmersiveModeController();
  if (WindowFeatureController::From(browser_view_->browser())
          ->UsesImmersiveFullscreenMode() &&
      controller->IsEnabled()) {
    return controller->GetExtraInfobarOffset();
  }
#endif
  return 0;
}

bool BrowserViewLayoutDelegateImpl::IsOrganizerPanelVisible() const {
  return organizer_panel::IsOrganizerPanelFeatureEnabled();
}

const BrowserFrameView* BrowserViewLayoutDelegateImpl::GetFrameView() const {
  return browser_view_->browser_widget()
             ? browser_view_->browser_widget()->GetFrameView()
             : nullptr;
}

void BrowserViewLayoutDelegateImpl::OnTabSearchPinnedStateChanged() {
  tab_search_pinned_to_tab_strip_ =
      browser_view_->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kTabSearchPinnedToTabstrip);
}

base::CallbackListSubscription
BrowserViewLayoutDelegateImpl::AddOnGlassModeChangedCallback(
    base::RepeatingCallback<void(bool)> callback,
    bool* current_state_out) {
  if (auto* const glass_frame_service = GlassFrameService::GetInstance()) {
    auto* const browser = browser_view_->browser();
    if (current_state_out) {
      *current_state_out =
          glass_frame_service->IsBrowserWindowEligible(browser);
    }
    return glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
        browser, std::move(callback));
  }
  if (current_state_out) {
    *current_state_out = false;
  }
  return base::CallbackListSubscription();
}
