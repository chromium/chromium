// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_VIEW_H_

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view_targeter_delegate.h"

namespace {
class WebAppNonClientFrameViewChromeOSTest;
}

namespace views {
class View;
class ViewTargeterDelegate;
}  // namespace views

class BrowserView;
class ContentSettingImageView;
class PageActionIconController;
class WebAppNavigationButtonContainer;
class WebAppToolbarButtonContainer;

// A container for web app buttons in the title bar.
class WebAppFrameToolbarView : public views::AccessiblePaneView,
                               public ToolbarButtonProvider,
                               public views::ViewTargeterDelegate {
  METADATA_HEADER(WebAppFrameToolbarView, views::AccessiblePaneView)

 public:
  explicit WebAppFrameToolbarView(BrowserView* browser_view);
  WebAppFrameToolbarView(const WebAppFrameToolbarView&) = delete;
  WebAppFrameToolbarView& operator=(const WebAppFrameToolbarView&) = delete;
  ~WebAppFrameToolbarView() override;

  void UpdateStatusIconsVisibility();

  // Called when the caption colors may have changed; updates the local values
  // and triggers a repaint if necessary.
  void UpdateCaptionColors();

  // Sets the container to paints its buttons the active/inactive color.
  void SetPaintAsActive(bool active);
  bool GetPaintAsActive() const;

  // Sets own bounds equal to the available space and returns the bounds of the
  // remaining inner space as a pair of (leading x, trailing x).
  std::pair<int, int> LayoutInContainer(int leading_x,
                                        int trailing_x,
                                        int y,
                                        int available_height);
  gfx::Rect LayoutInContainer(gfx::Rect available_space);

  // Sets own bounds within the available_space.
  void LayoutForWindowControlsOverlay(gfx::Rect available_space);

  std::optional<SkColor> active_color_for_testing() const {
    return active_foreground_color_;
  }

  // ToolbarButtonProvider:
  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() override;
  gfx::Size GetToolbarButtonSize() const override;
  views::View* GetDefaultExtensionDialogAnchorView() override;
  PageActionIconView* GetPageActionIconView(PageActionIconType type) override;
  AppMenuButton* GetAppMenuButton() override;
  gfx::Rect GetFindBarBoundingBox(int contents_bottom) override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;
  views::View* GetAnchorView(std::optional<PageActionIconType> type) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  AvatarToolbarButton* GetAvatarToolbarButton() override;
  ToolbarButton* GetBackButton() override;
  ReloadButton* GetReloadButton() override;
  IntentChipButton* GetIntentChipButton() override;
  DownloadToolbarButtonView* GetDownloadButton() override;

  // views::ViewTargeterDelegate
  bool DoesIntersectRect(const View* target,
                         const gfx::Rect& rect) const override;

  void OnWindowControlsOverlayEnabledChanged();
  void UpdateBorderlessModeEnabled();
  void SetWindowControlsOverlayToggleVisible(bool visible);

  WebAppNavigationButtonContainer* get_left_container_for_testing() {
    return left_container_;
  }
  WebAppToolbarButtonContainer* get_right_container_for_testing() {
    return right_container_;
  }
  PageActionIconController* GetPageActionIconControllerForTesting();

 protected:
  // views::AccessiblePaneView:
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnThemeChanged() override;

 private:
  friend class ImmersiveModeControllerChromeosWebAppBrowserTest;
  friend class WebAppAshInteractiveUITest;
  friend class WebAppNonClientFrameViewChromeOSTest;

  views::View* GetContentSettingContainerForTesting();

  const std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>&
  GetContentSettingViewsForTesting() const;

  void UpdateCachedColors();

  // `color_changed` is true if this is called after an update to the window's
  // color. It will be false if this is called when the color is initially set
  // for the window.
  void UpdateChildrenColor(bool color_changed);

  // The containing browser view.
  const raw_ptr<BrowserView> browser_view_;

  // Button and text colors.
  bool paint_as_active_ = true;
  std::optional<SkColor> active_background_color_;
  std::optional<SkColor> active_foreground_color_;
  std::optional<SkColor> inactive_background_color_;
  std::optional<SkColor> inactive_foreground_color_;

  // All remaining members are owned by the views hierarchy.

  // The navigation container is only created when display mode is minimal-ui.
  raw_ptr<WebAppNavigationButtonContainer> left_container_ = nullptr;

  // Empty container used by the parent frame to layout additional elements.
  raw_ptr<views::View> center_container_ = nullptr;

  raw_ptr<WebAppToolbarButtonContainer> right_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_VIEW_H_
