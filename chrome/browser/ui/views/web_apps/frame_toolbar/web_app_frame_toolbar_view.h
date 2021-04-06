// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_VIEW_H_

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class View;
class Widget;
}  // namespace views

class BrowserView;
class ContentSettingImageView;
class PageActionIconController;
class WebAppNavigationButtonContainer;
class WebAppToolbarButtonContainer;

// A container for web app buttons in the title bar.
class WebAppFrameToolbarView : public views::AccessiblePaneView,
                               public ToolbarButtonProvider {
 public:
  METADATA_HEADER(WebAppFrameToolbarView);
  WebAppFrameToolbarView(views::Widget* widget, BrowserView* browser_view);
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

  SkColor active_color_for_testing() const { return active_foreground_color_; }

  // ToolbarButtonProvider:
  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() override;
  gfx::Size GetToolbarButtonSize() const override;
  views::View* GetDefaultExtensionDialogAnchorView() override;
  PageActionIconView* GetPageActionIconView(PageActionIconType type) override;
  AppMenuButton* GetAppMenuButton() override;
  gfx::Rect GetFindBarBoundingBox(int contents_bottom) override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;
  views::View* GetAnchorView(PageActionIconType type) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  AvatarToolbarButton* GetAvatarToolbarButton() override;
  ToolbarButton* GetBackButton() override;
  ReloadButton* GetReloadButton() override;

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
  friend class WebAppNonClientFrameViewAshTest;
  friend class ImmersiveModeControllerChromeosWebAppBrowserTest;
  friend class WebAppAshInteractiveUITest;

  views::View* GetContentSettingContainerForTesting();

  const std::vector<ContentSettingImageView*>&
  GetContentSettingViewsForTesting() const;

  void UpdateChildrenColor();

  // The containing browser view.
  BrowserView* const browser_view_;

  // Button and text colors.
  bool paint_as_active_ = true;
  SkColor active_background_color_ = gfx::kPlaceholderColor;
  SkColor active_foreground_color_ = gfx::kPlaceholderColor;
  SkColor inactive_background_color_ = gfx::kPlaceholderColor;
  SkColor inactive_foreground_color_ = gfx::kPlaceholderColor;

  // All remaining members are owned by the views hierarchy.

  // The navigation container is only created when display mode is minimal-ui.
  WebAppNavigationButtonContainer* left_container_ = nullptr;

  // Empty container used by the parent frame to layout additional elements.
  views::View* center_container_ = nullptr;

  WebAppToolbarButtonContainer* right_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_FRAME_TOOLBAR_VIEW_H_
