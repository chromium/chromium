// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_

#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "ui/views/window/non_client_view.h"

class BrowserFrame;
class BrowserView;
class HostedAppButtonContainer;

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView,
                                  public ProfileAttributesStorage::Observer,
                                  public TabStripObserver {
 public:
  // Type used for functions whose return values depend on the active state of
  // the frame.
  enum ActiveState {
    kUseCurrent,  // Use current frame active state.
    kActive,      // Treat frame as active regardless of current state.
    kInactive,    // Treat frame as inactive regardless of current state.
  };

  // The minimum total height users should have to use as a drag handle to move
  // the window with.
  static constexpr int kMinimumDragHeight = 8;

  BrowserNonClientFrameView(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameView() override;

  BrowserView* browser_view() const { return browser_view_; }
  BrowserFrame* frame() const { return frame_; }

  // Called when BrowserView creates all it's child views.
  virtual void OnBrowserViewInitViewsComplete();

  // Called on Mac after the browser window is fullscreened or unfullscreened.
  virtual void OnFullscreenStateChanged();

  // Returns whether the caption buttons are drawn at the leading edge (i.e. the
  // left in LTR mode, or the right in RTL mode).
  virtual bool CaptionButtonsOnLeadingEdge() const;

  // Retrieves the bounds, in non-client view coordinates within which the
  // TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const = 0;

  // Returns the inset of the topmost view in the client view from the top of
  // the non-client view. The topmost view depends on the window type. The
  // topmost view is the tab strip for tabbed browser windows, the toolbar for
  // popups, the web contents for app windows and varies for fullscreen windows.
  // If |restored| is true, this is calculated as if the window was restored,
  // regardless of its current state.
  virtual int GetTopInset(bool restored) const = 0;

  // Returns the amount that the theme background should be inset.
  virtual int GetThemeBackgroundXInset() const = 0;

  // Updates the top UI state to be hidden or shown in fullscreen according to
  // the preference's state. Currently only used on Mac.
  virtual void UpdateFullscreenTopUI(bool needs_check_tab_fullscreen);

  // Returns whether the top UI should hide.
  virtual bool ShouldHideTopUIForFullscreen() const;

  // Returns whether the user is allowed to exit fullscreen on their own (some
  // special modes lock the user in fullscreen).
  virtual bool CanUserExitFullscreen() const;

  // Determines whether the top frame is condensed vertically, as when the
  // window is maximized. If true, the top frame is just the height of a tab,
  // rather than having extra vertical space above the tabs.
  virtual bool IsFrameCondensed() const;

  // Returns whether the shapes of background tabs are visible against the
  // frame, given an active state of |active|.
  virtual bool HasVisibleBackgroundTabShapes(
      ActiveState active_state = kUseCurrent) const;

  // Returns whether the shapes of background tabs are visible against the frame
  // for either active or inactive windows.
  bool EverHasVisibleBackgroundTabShapes() const;

  // Returns the color of the browser frame, which is also the color of the
  // tabstrip background.
  SkColor GetFrameColor(ActiveState active_state = kUseCurrent) const;

  // Returns COLOR_TOOLBAR_TOP_SEPARATOR[,_INACTIVE] depending on the activation
  // state of the window.
  SkColor GetToolbarTopSeparatorColor() const;

  // Returns the tab background color based on both the |state| of the tab and
  // the activation state of the window.
  SkColor GetTabBackgroundColor(TabState state,
                                ActiveState active_state = kUseCurrent) const;

  // Returns the tab foreground color of the for the text based on both the
  // |state| of the tab and the activation state of the window.
  SkColor GetTabForegroundColor(TabState state) const;

  // For non-transparent windows, returns the resource ID to use behind
  // background tabs.  |has_custom_image| will be set to true if this has been
  // customized by the theme in some way.  Note that because of fallback during
  // image generation, |has_custom_image| may be true even when the returned
  // background resource ID has not been directly overridden (i.e.
  // ThemeProvider::HasCustomImage() returns false).
  int GetTabBackgroundResourceId(ActiveState active_state,
                                 bool* has_custom_image) const;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;

  // Provided for mus. Updates the client-area of the WindowTreeHostMus.
  virtual void UpdateClientArea();

  // Provided for mus to update the minimum window size property.
  virtual void UpdateMinimumSize();

  // Whether the special painting mode for one tab is allowed, regardless of how
  // many tabs there are right now.
  virtual bool IsSingleTabModeAvailable() const;

  // Returns whether or not strokes should be drawn around and under the tabs.
  virtual bool ShouldDrawStrokes() const;

  // views::NonClientFrameView:
  using views::NonClientFrameView::ShouldPaintAsActive;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  int NonClientHitTest(const gfx::Point& point) override;
  void ResetWindowControls() override;

  // TabStripObserver:
  void OnSingleTabModeChanged() override;

  HostedAppButtonContainer* hosted_app_button_container_for_testing() {
    return hosted_app_button_container_;
  }

 protected:
  // Whether the frame should be painted with theming.
  // By default, tabbed browser windows are themed but popup and app windows are
  // not.
  virtual bool ShouldPaintAsThemed() const;

  // Converts an ActiveState to a bool representing whether the frame should be
  // treated as active.
  bool ShouldPaintAsActive(ActiveState active_state) const;

  // Whether the frame should be painted with the special mode for one tab.
  bool ShouldPaintAsSingleTabMode() const;

  // Compute aspects of the frame needed to paint the frame background.
  gfx::ImageSkia GetFrameImage(ActiveState active_state = kUseCurrent) const;
  gfx::ImageSkia GetFrameOverlayImage(
      ActiveState active_state = kUseCurrent) const;

  // views::NonClientFrameView:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ActivationChanged(bool active) override;
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;

  void set_hosted_app_button_container(
      HostedAppButtonContainer* hosted_app_button_container) {
    hosted_app_button_container_ = hosted_app_button_container;
  }
  HostedAppButtonContainer* hosted_app_button_container() {
    return hosted_app_button_container_;
  }
  const HostedAppButtonContainer* hosted_app_button_container() const {
    return hosted_app_button_container_;
  }

 private:
  void MaybeObserveTabstrip();

  // Gets a theme provider that should be non-null even before we're added to a
  // view hierarchy.
  const ui::ThemeProvider* GetThemeProviderForProfile() const;

  // Draws a taskbar icon for non-guest sessions, erases it otherwise.
  void UpdateTaskbarDecoration();

  // Returns the color of the given |color_id| from the theme provider or the
  // default theme properties.
  SkColor GetThemeOrDefaultColor(int color_id) const;

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // Menu button and page status icons. Only used by hosted app windows.
  HostedAppButtonContainer* hosted_app_button_container_ = nullptr;

  ScopedObserver<TabStrip, BrowserNonClientFrameView> tab_strip_observer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameView);
};

namespace chrome {

// Provided by a browser_non_client_frame_view_factory_*.cc implementation
BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
