// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_MAC_H_

#import <CoreGraphics/CGBase.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/prefs/pref_member.h"
#include "ui/gfx/geometry/rect_f.h"

namespace base {
class OneShotTimer;
}

namespace remote_cocoa::mojom {
enum class ToolbarVisibilityStyle;
}

@class FullscreenToolbarController;

class CaptionButtonPlaceholderContainer;

class BrowserFrameViewMac : public BrowserFrameView,
                            public web_app::WebAppRegistrarObserver {
 public:
  // Mac implementation of BrowserFrameView.
  BrowserFrameViewMac(BrowserWidget* widget, BrowserView* browser_view);

  BrowserFrameViewMac(const BrowserFrameViewMac&) = delete;
  BrowserFrameViewMac& operator=(const BrowserFrameViewMac&) = delete;

  ~BrowserFrameViewMac() override;

  // BrowserFrameView:
  void OnFullscreenStateChanged() override;
  bool CaptionButtonsOnLeadingEdge() const override;
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  gfx::Rect GetBoundsForWebAppFrameToolbar(
      const gfx::Size& toolbar_preferred_size) const override;
  BrowserLayoutParams GetBrowserLayoutParams() const override;
  int GetTopInset(bool restored) const override;
  void UpdateFullscreenTopUI() override;
  bool ShouldHideTopUIInFullscreen() const override;
  void UpdateThrobber(bool running) override;
  void PaintAsActiveChanged() override;
  void OnThemeChanged() override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;

  // views::FrameView:
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

  // Used by TabContainerOverlayView to paint the tab strip background.
  void PaintThemedFrame(gfx::Canvas* canvas);

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout(PassKey) override;

  // BrowserFrameView:
  BoundsAndMargins GetCaptionButtonBounds() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserFrameViewMacTest, GetCenteredTitleBounds);
  FRIEND_TEST_ALL_PREFIXES(BrowserFrameViewMacTest,
                           GetCaptionButtonPlaceholderBounds);

  // Creates an inset from the caption button size which controls for which edge
  // the captions buttons exists on. Used to position elements like the tabstrip
  // that are adjacent to the caption buttons.
  //
  // The `visual_overlap` parameter specifies how much - if any - the adjacent
  // View overlaps the caption button region; the insets will be reduced by that
  // amount. For example, the tabstrip overlaps by the size of the bottom curve
  // of the first tab. In most cases this will be zero.
  gfx::Insets GetCaptionButtonInsets(int visual_overlap = 0) const;

  static gfx::Rect GetCenteredTitleBounds(gfx::Rect frame,
                                          gfx::Rect available_space,
                                          int preferred_title_width);

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

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_MAC_H_
