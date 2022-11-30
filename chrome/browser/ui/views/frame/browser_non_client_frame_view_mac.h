// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_

#include "base/memory/raw_ptr.h"

#import <CoreGraphics/CGBase.h>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/prefs/pref_member.h"

namespace views {
class Label;
}

@class FullscreenToolbarController;

class CaptionButtonPlaceholderContainer;
class WindowControlsOverlayInputRoutingMac;

class BrowserNonClientFrameViewMac : public BrowserNonClientFrameView,
                                     public web_app::AppRegistrarObserver {
 public:
  // Mac implementation of BrowserNonClientFrameView.
  BrowserNonClientFrameViewMac(BrowserFrame* frame, BrowserView* browser_view);

  BrowserNonClientFrameViewMac(const BrowserNonClientFrameViewMac&) = delete;
  BrowserNonClientFrameViewMac& operator=(const BrowserNonClientFrameViewMac&) =
      delete;

  ~BrowserNonClientFrameViewMac() override;

  // BrowserNonClientFrameView:
  void OnFullscreenStateChanged() override;
  bool CaptionButtonsOnLeadingEdge() const override;
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateFullscreenTopUI() override;
  bool ShouldHideTopUIForFullscreen() const override;
  void UpdateThrobber(bool running) override;
  void PaintAsActiveChanged() override;
  void UpdateFrameColor() override;
  void OnThemeChanged() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;
  void UpdateMinimumSize() override;
  void WindowControlsOverlayEnabledChanged() override;

  // views::View:
  gfx::Size GetMinimumSize() const override;
  void AddedToWidget() override;
  void PaintChildren(const views::PaintInfo& info) override;

  // web_app::AppRegistrarObserver
  void OnAlwaysShowToolbarInFullscreenChanged(const web_app::AppId& app_id,
                                              bool show) override;
  void OnAppRegistrarDestroyed() override;

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewMacTest,
                           GetCenteredTitleBounds);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewMacTest,
                           GetWebAppFrameToolbarAvailableBounds);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewMacTest,
                           GetCaptionButtonPlaceholderBounds);

  static gfx::Rect GetCenteredTitleBounds(int frame_width,
                                          int frame_height,
                                          int left_inset_x,
                                          int right_inset_x,
                                          int title_width);

  static gfx::Rect GetWebAppFrameToolbarAvailableBounds(
      bool is_rtl,
      const gfx::Size& frame,
      int y,
      int caption_button_container_width);
  static gfx::Rect GetCaptionButtonPlaceholderBounds(bool is_rtl,
                                                     const gfx::Size& frame,
                                                     int y,
                                                     int width);

  void PaintThemedFrame(gfx::Canvas* canvas);

  CGFloat FullscreenBackingBarHeight() const;

  // Calculate the y offset the top UI needs to shift down due to showing the
  // slide down menu bar at the very top in full screen.
  int TopUIFullscreenYOffset() const;
  void LayoutTitleBarForWebApp();
  void LayoutWindowControlsOverlay();

  void UpdateCaptionButtonPlaceholderContainerBackground();

  void AddRoutingForWindowControlsOverlayViews();

  // Toggle the visibility of the web_app_frame_toolbar_view() for PWAs with
  // window controls overlay display override when entering full screen or when
  // toolbar style is changed.
  void ToggleWebAppFrameToolbarViewVisibility();

  // Returns the current value of the "always show toolbar in fullscreen"
  // preference, either reading the value from the kShowFullscreenToolbar
  // preference or if this is a window for an app, from the settings for that
  // app.
  bool AlwaysShowToolbarInFullscreen() const;

  // Used to keep track of the update of kShowFullscreenToolbar preference.
  BooleanPrefMember show_fullscreen_toolbar_;
  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::AppRegistrarObserver>
      always_show_toolbar_in_fullscreen_observation_{this};

  raw_ptr<views::Label> window_title_ = nullptr;

  // A placeholder container that lies on top of the traffic lights to indicate
  // NonClientArea. Only for PWAs with window controls overlay display override.
  raw_ptr<CaptionButtonPlaceholderContainer>
      caption_button_placeholder_container_ = nullptr;

  // PWAs with window controls overlay display override covers the browser
  // window with WebContentsViewCocoa natively even if the views::view 'looks'
  // right and so events end up in the client area.
  // WindowControlsOverlayInputRoutingMac overlays a NSView the non client
  // area so events can be routed to the right view in the non client area. Two
  // separate WindowControlsOverlayInputRoutingMac instances are needed
  // since there are two dis jointed areas of non client area.
  std::unique_ptr<WindowControlsOverlayInputRoutingMac>
      caption_buttons_overlay_input_routing_view_;
  std::unique_ptr<WindowControlsOverlayInputRoutingMac>
      web_app_frame_toolbar_overlay_routing_view_;

  base::scoped_nsobject<FullscreenToolbarController>
      fullscreen_toolbar_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
