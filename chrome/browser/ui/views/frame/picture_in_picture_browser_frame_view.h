// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/window_frame_provider.h"
#endif

namespace views {
class FrameBackground;
class Label;
}

namespace {
class WindowEventObserver;
}

class PictureInPictureBrowserFrameView
    : public BrowserNonClientFrameView,
      public ChromeLocationBarModelDelegate,
      public LocationIconView::Delegate,
      public IconLabelBubbleView::Delegate,
      public ContentSettingImageView::Delegate,
#if BUILDFLAG(IS_MAC)
      public device::GeolocationManager::PermissionObserver,
#endif
      public views::WidgetObserver {
 public:
  METADATA_HEADER(PictureInPictureBrowserFrameView);

  PictureInPictureBrowserFrameView(BrowserFrame* frame,
                                   BrowserView* browser_view);
  PictureInPictureBrowserFrameView(const PictureInPictureBrowserFrameView&) =
      delete;
  PictureInPictureBrowserFrameView& operator=(
      const PictureInPictureBrowserFrameView&) = delete;
  ~PictureInPictureBrowserFrameView() override;

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override {}
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnThemeChanged() override;
  void Layout() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
#if BUILDFLAG(IS_LINUX)
  gfx::Insets MirroredFrameBorderInsets() const override;
  gfx::Insets GetInputInsets() const override;
  SkRRect GetRestoredClipRegion() const override;
#endif

  // ChromeLocationBarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;
  bool GetURL(GURL* url) const override;
  bool ShouldTrimDisplayUrlAfterHostName() const override;
  bool ShouldDisplayURL() const override;

  // LocationIconView::Delegate:
  content::WebContents* GetWebContents() override;
  bool IsEditingOrEmpty() const override;
  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const override;
  bool ShowPageInfoDialog() override;
  LocationBarModel* GetLocationBarModel() const override;
  ui::ImageModel GetLocationIcon(LocationIconView::Delegate::IconFetchedCallback
                                     on_icon_fetched) const override;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  // ContentSettingImageView::Delegate:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

#if BUILDFLAG(IS_MAC)
  // GeolocationManager::PermissionObserver:
  void OnSystemPermissionUpdated(
      device::LocationSystemPermissionStatus new_status) override;
#endif

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // PictureInPictureBrowserFrameView:
  // Convert the bounds of a child view of |controls_container_view_| to use
  // the system's coordinate system.
  gfx::Rect ConvertControlViewBounds(views::View* control_view) const;

  // Gets the bounds of the controls.
  gfx::Rect GetLocationIconViewBounds() const;
  gfx::Rect GetContentSettingViewBounds(size_t index) const;
  gfx::Rect GetBackToTabControlsBounds() const;
  gfx::Rect GetCloseControlsBounds() const;

  LocationIconView* GetLocationIconView();

  // Updates the state of the images showing the content settings status.
  void UpdateContentSettingsIcons();

  // Updates the top bar title and icons according to whether user wants to
  // interact with the window. The top bar should be highlighted in all these
  // cases:
  // - PiP window is hovered with mouse
  // - PiP window is in focus with keyboard navigation
  // - PiP window is in focus with any other format of activation
  // - Dialogs are opened in the PiP window
  void UpdateTopBarView(bool render_active);

  // Returns the insets of the window frame borders.
  gfx::Insets FrameBorderInsets() const;

  // Returns the insets of the window frame borders for resizing.
  gfx::Insets ResizeBorderInsets() const;

  // Returns the height of the top bar area, including the window top border.
  int GetTopAreaHeight() const;

  // Called when mouse entered or exited the pip window.
  void OnMouseEnteredOrExitedWindow(bool entered);

#if BUILDFLAG(IS_LINUX)
  // Sets the window frame provider so that it will be used for drawing.
  void SetWindowFrameProvider(ui::WindowFrameProvider* window_frame_provider);

  // Returns whether a client-side shadow should be drawn for the window.
  bool ShouldDrawFrameShadow() const;

  // Gets the shadow metrics (radius, offset, and number of shadows) even if
  // shadows are not drawn.
  static gfx::ShadowValues GetShadowValues();
#endif

  views::View* GetBackToTabButtonForTesting();

 private:
  // A model required to use LocationIconView.
  std::unique_ptr<LocationBarModel> location_bar_model_;

  raw_ptr<views::BoxLayoutView> controls_container_view_ = nullptr;

  // An icon to the left of the window title, which reuses the location icon in
  // the location bar of a normal browser. Since the web contents to PiP is
  // guaranteed to be secure, this icon should always be the HTTPS lock.
  raw_ptr<LocationIconView> location_icon_view_ = nullptr;

  raw_ptr<views::Label> window_title_ = nullptr;

  // The content setting views for icons and bubbles.
  std::vector<ContentSettingImageView*> content_setting_views_;

  raw_ptr<CloseImageButton> close_image_button_ = nullptr;
  raw_ptr<views::View> back_to_tab_button_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  bool mouse_inside_window_ = false;

#if BUILDFLAG(IS_LINUX)
  // Used to draw window frame borders and shadow on Linux when GTK theme is
  // enabled.
  raw_ptr<ui::WindowFrameProvider> window_frame_provider_ = nullptr;

  // Used to draw window frame borders and shadow on Linux when classic theme is
  // enabled.
  std::unique_ptr<views::FrameBackground> frame_background_;
#endif

  // Userd to monitor key and mouse event from native window.
  std::unique_ptr<WindowEventObserver> window_event_observer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_H_
