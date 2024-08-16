// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/browser_frame_header_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "chromeos/ui/frame/highlight_border_overlay.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"

class ProfileIndicatorIcon;
class TabIconView;

namespace chromeos {
class FrameCaptionButtonContainerView;
class HighlightBorderOverlay;
}  // namespace chromeos

// Provides the BrowserNonClientFrameView for Chrome OS.
class BrowserNonClientFrameViewChromeOS
    : public BrowserNonClientFrameView,
      public BrowserFrameHeaderChromeOS::AppearanceProvider,
      public display::DisplayObserver,
      public TabIconViewModel,
      public aura::WindowObserver,
      public ImmersiveModeController::Observer,
      public apps::AppRegistryCache::Observer {
  METADATA_HEADER(BrowserNonClientFrameViewChromeOS, BrowserNonClientFrameView)

 public:
  BrowserNonClientFrameViewChromeOS(BrowserFrame* frame,
                                    BrowserView* browser_view);
  BrowserNonClientFrameViewChromeOS(const BrowserNonClientFrameViewChromeOS&) =
      delete;
  BrowserNonClientFrameViewChromeOS& operator=(
      const BrowserNonClientFrameViewChromeOS&) = delete;
  ~BrowserNonClientFrameViewChromeOS() override;

  static BrowserNonClientFrameViewChromeOS* Get(aura::Window* window);

  void Init();

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;
  int GetTopInset(bool restored) const override;
  void UpdateThrobber(bool running) override;
  bool CanUserExitFullscreen() const override;
  SkColor GetCaptionColor(BrowserFrameActiveState active_state) const override;
  SkColor GetFrameColor(BrowserFrameActiveState active_state) const override;
  void UpdateMinimumSize() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void WindowControlsOverlayEnabledChanged() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;
  void UpdateWindowRoundedCorners() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout(PassKey) override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;
  views::View::Views GetChildrenInZOrder() override;

  // BrowserFrameHeaderChromeOS::AppearanceProvider:
  SkColor GetTitleColor() override;
  SkColor GetFrameHeaderColor(bool active) override;
  gfx::ImageSkia GetFrameHeaderImage(bool active) override;
  int GetFrameHeaderImageYInset() override;
  gfx::ImageSkia GetFrameHeaderOverlayImage(bool active) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  void OnTabletModeToggled(bool enabled);

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  ui::ImageModel GetFaviconForTabIconView() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  chromeos::FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

 protected:
  // BrowserNonClientFrameView:
  void PaintAsActiveChanged() override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void AddedToWidget() override;

 private:
  friend class BrowserNonClientFrameViewChromeOSTestApi;
  FRIEND_TEST_ALL_PREFIXES(ImmersiveModeBrowserViewTestNoWebUiTabStrip,
                           ImmersiveFullscreen);

  // App is a PWA and has borderless in its manifest. This doesn't yet mean
  // that the `window-management` permission has been granted and borderless
  // mode would be activated.
  bool AppIsPwaWithBorderlessDisplayMode() const;

  // Returns true if `GetShowCaptionButtonsWhenNotInOverview()` returns true
  // and this browser window is not showing in overview.
  bool GetShowCaptionButtons() const;

  // Returns whether we should show caption buttons. False for fullscreen,
  // tablet mode and webUI tab strip in most cases. The exceptions are if this
  // is a packaged app, as they have immersive mode enabled, and floated windows
  // in tablet mode.
  bool GetShowCaptionButtonsWhenNotInOverview() const;

  // Distance between the edge of the NonClientFrameView and the web app frame
  // toolbar.
  int GetToolbarLeftInset() const;

  // Distance between the edges of the NonClientFrameView and the tab strip.
  int GetTabStripLeftInset() const;
  int GetTabStripRightInset() const;

  // Returns true if there is anything to paint. Some fullscreen windows do
  // not need their frames painted.
  bool GetShouldPaint() const;

  // Helps to hide or show the header as needed when the window is added to or
  // removed from overview.
  void OnAddedToOrRemovedFromOverview();

  // Creates the frame header for the browser window.
  std::unique_ptr<chromeos::FrameHeader> CreateFrameHeader();

  // Triggers the web-app origin and icon animations, assumes the web-app UI
  // elements exist.
  void StartWebAppAnimation();

  // Updates the kTopViewInset window property after a layout.
  void UpdateTopViewInset();

  // Returns true if |profile_indicator_icon_| should be shown.
  bool GetShowProfileIndicatorIcon() const;

  // Updates the icon that indicates a teleported window.
  void UpdateProfileIcons();

  void LayoutProfileIndicator();

  void UpdateBorderlessModeEnabled();

  // Returns whether this window is currently in the overview list.
  bool GetOverviewMode() const;

  // Returns whether this window is currently in, or is about to be in, tab
  // fullscreen (not immersive fullscreen). Returns false for immersive
  // fullscreen.
  bool GetHideCaptionButtonsForFullscreen() const;

  // Called any time the frame color may have changed.
  void OnUpdateFrameColor();

  // Called any time the theme has changed and may need to be animated.
  void MaybeAnimateThemeChanged();

  // Returns whether the associated window is currently floated or not.
  bool IsFloated() const;

  // Helper to check whether we should enable immersive mode.`on_tablet_enabled`
  // is set to true only when it is called when tablet mode is just toggled on
  // notified from OnTabletModeToggled.
  bool ShouldEnableImmersiveModeController(bool on_tablet_enabled) const;

  // Helper to check whether we should enable fullscreen mode.
  // `on_tablet_enabled` is set to true only when tablet mode is just toggled
  // on notified from OnTabletModeToggled.
  bool ShouldEnableFullscreenMode(bool on_tablet_enabled) const;

  // True if the the associated browser window should be using the WebUI tab
  // strip.
  bool UseWebUITabStrip() const;

  // Returns the top level aura::Window for this browser window.
  const aura::Window* GetFrameWindow() const;
  aura::Window* GetFrameWindow();

  // Generates a nine patch layer painted with a highlight border.
  std::unique_ptr<HighlightBorderOverlay> highlight_border_overlay_;

  // View which contains the window controls.
  raw_ptr<chromeos::FrameCaptionButtonContainerView> caption_button_container_ =
      nullptr;

  // For popups, the window icon.
  raw_ptr<TabIconView> window_icon_ = nullptr;

  // This is used for teleported windows (in multi-profile mode).
  raw_ptr<ProfileIndicatorIcon> profile_indicator_icon_ = nullptr;

  // Helper class for painting the header.
  std::unique_ptr<chromeos::FrameHeader> frame_header_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observation_{this};

  std::optional<display::ScopedDisplayObserver> display_observer_;

  gfx::Size last_minimum_size_;

  // Callback to invoke to animate back in the layer associated with the
  // `contents_web_view()` native view following a theme changed event.
  base::CancelableOnceCallback<void(bool)> theme_changed_animation_callback_;

  base::WeakPtrFactory<BrowserNonClientFrameViewChromeOS> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_H_
