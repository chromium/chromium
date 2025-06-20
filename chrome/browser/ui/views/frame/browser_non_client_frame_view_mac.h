// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_

#import <CoreGraphics/CGBase.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/prefs/pref_member.h"

namespace base {
class OneShotTimer;
}

namespace views {
class Label;
}

namespace remote_cocoa::mojom {
enum class ToolbarVisibilityStyle;
}

@class FullscreenToolbarController;

class CaptionButtonPlaceholderContainer;

class BrowserNonClientFrameViewMac : public BrowserNonClientFrameView,
                                     public web_app::WebAppRegistrarObserver {
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
  gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;
  int GetTopInset(bool restored) const override;
  void UpdateFullscreenTopUI() override;
  bool ShouldHideTopUIForFullscreen() const override;
  void UpdateThrobber(bool running) override;
  void PaintAsActiveChanged() override;
  void OnThemeChanged() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void UpdateMinimumSize() override;
  void WindowControlsOverlayEnabledChanged() override;

  // views::View:
  gfx::Size GetMinimumSize() const override;
  void PaintChildren(const views::PaintInfo& info) override;

  // web_app::WebAppRegistrarObserver
  void OnAlwaysShowToolbarInFullscreenChanged(const webapps::AppId& app_id,
                                              bool show) override;
  void OnAppRegistrarDestroyed() override;

  // Creates an inset from the caption button size which controls for which edge
  // the captions buttons exists on. Used to position the tab strip region view
  // and the caption button placeholder container. Returns the distance from the
  // leading edge of the frame to the first tab in the tabstrip not including
  // the corner radius.
  gfx::Insets GetCaptionButtonInsets() const;

  // Used by TabContainerOverlayView to paint the tab strip background.
  void PaintThemedFrame(gfx::Canvas* canvas) override;

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout(PassKey) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewMacTest,
                           GetCenteredTitleBounds);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewMacTest,
                           GetCaptionButtonPlaceholderBounds);

  static gfx::Rect GetCenteredTitleBounds(gfx::Rect frame,
                                          gfx::Rect available_space,
                                          int preferred_title_width);
  static gfx::Rect GetCaptionButtonPlaceholderBounds(
      const gfx::Rect& frame,
      const gfx::Insets& caption_button_insets);

  // Calculate the y offset the top UI needs to shift down due to showing the
  // slide down menu bar at the very top in full screen.
  int TopUIFullscreenYOffset() const;
  void LayoutWindowControlsOverlay();

  void UpdateCaptionButtonPlaceholderContainerBackground();

  // Toggle the visibility of the web_app_frame_toolbar_view() for PWAs with
  // window controls overlay display override when entering full screen or when
  // toolbar style is changed.
  void ToggleWebAppFrameToolbarViewVisibility();

  // Emits the duration of the current fullscreen session, if any.
  void EmitFullscreenSessionHistograms();

  // Used to keep track of the update of kShowFullscreenToolbar preference.
  BooleanPrefMember show_fullscreen_toolbar_;
  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::WebAppRegistrarObserver>
      always_show_toolbar_in_fullscreen_observation_{this};

  // A placeholder container that lies on top of the traffic lights to indicate
  // NonClientArea. Only for PWAs with window controls overlay display override.
  raw_ptr<CaptionButtonPlaceholderContainer>
      caption_button_placeholder_container_ = nullptr;

  FullscreenToolbarController* __strong fullscreen_toolbar_controller_;

  // Mark the start of a fullscreen session. Applies to both immersive and
  // standard fullscreen.
  std::optional<base::TimeTicks> fullscreen_session_start_;

  // Fires after 24 hours to emit the duration of the current fullscreen
  // session, if any.
  std::unique_ptr<base::OneShotTimer> fullscreen_session_timer_;

  // Used to track the current toolbar style.
  std::optional<remote_cocoa::mojom::ToolbarVisibilityStyle>
      current_toolbar_style_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_MAC_H_
