// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_FRAME_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_FRAME_TOOLBAR_VIEW_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class AppMenuButton;
class AvatarToolbarButton;
class BrowserView;
class ExtensionsToolbarContainer;
class PageActionIconContainerView;
class ReloadButton;
class ToolbarButton;
class WebAppMenuButton;
class WebAppOriginText;

// A container for web app buttons in the title bar.
class WebAppFrameToolbarView : public views::AccessiblePaneView,
                               public BrowserActionsContainer::Delegate,
                               public CommandObserver,
                               public views::ButtonListener,
                               public ContentSettingImageView::Delegate,
                               public ImmersiveModeController::Observer,
                               public PageActionIconView::Delegate,
                               public ToolbarButtonProvider,
                               public views::WidgetObserver,
                               public ui::MaterialDesignControllerObserver {
 public:
  static const char kViewClassName[];

  // Timing parameters for the origin fade animation.
  // These control how long it takes for the origin text and menu button
  // highlight to fade in, pause then fade out.
  static constexpr base::TimeDelta kOriginFadeInDuration =
      base::TimeDelta::FromMilliseconds(800);
  static constexpr base::TimeDelta kOriginPauseDuration =
      base::TimeDelta::FromMilliseconds(2500);
  static constexpr base::TimeDelta kOriginFadeOutDuration =
      base::TimeDelta::FromMilliseconds(800);

  // The total duration of the origin fade animation.
  static base::TimeDelta OriginTotalDuration();

  // |active_color| and |inactive_color| indicate the colors to use
  // for button icons when the window is focused and blurred respectively.
  WebAppFrameToolbarView(views::Widget* widget,
                         BrowserView* browser_view,
                         SkColor active_color,
                         SkColor inactive_color,
                         base::Optional<int> left_margin = base::nullopt,
                         base::Optional<int> right_margin = base::nullopt);
  ~WebAppFrameToolbarView() override;

  void UpdateStatusIconsVisibility();

  void UpdateCaptionColors();

  // Sets the container to paints its buttons the active/inactive color.
  void SetPaintAsActive(bool active);

  // Sets own bounds equal to the available space and returns the bounds of the
  // remaining inner space as a pair of (leading x, trailing x).
  std::pair<int, int> LayoutInContainer(int leading_x,
                                        int trailing_x,
                                        int y,
                                        int available_height);

  SkColor active_color_for_testing() const { return active_color_; }

  // views::AccessiblePaneView:
  const char* GetClassName() const override;

  // BrowserActionsContainer::Delegate:
  views::LabelButton* GetOverflowReferenceView() override;
  base::Optional<int> GetMaxBrowserActionsWidth() const override;
  bool CanShowIconInToolbar() const override;
  std::unique_ptr<ToolbarActionsBar> CreateToolbarActionsBar(
      ToolbarActionsBarDelegate* delegate,
      Browser* browser,
      ToolbarActionsBar* main_bar) const override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ContentSettingImageView::Delegate:
  SkColor GetContentSettingInkDropColor() const override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;
  void OnContentSettingImageBubbleShown(
      ContentSettingImageModel::ImageType type) const override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;

  // PageActionIconView::Delegate:
  SkColor GetPageActionInkDropColor() const override;
  content::WebContents* GetWebContentsForPageActionIconView() override;

  // ToolbarButtonProvider:
  BrowserActionsContainer* GetBrowserActionsContainer() override;
  ToolbarActionView* GetToolbarActionViewForId(const std::string& id) override;
  views::View* GetDefaultExtensionDialogAnchorView() override;
  PageActionIconView* GetPageActionIconView(PageActionIconType type) override;
  AppMenuButton* GetAppMenuButton() override;
  gfx::Rect GetFindBarBoundingBox(int contents_bottom) const override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;
  views::View* GetAnchorView(PageActionIconType type) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  AvatarToolbarButton* GetAvatarToolbarButton() override;
  ToolbarButton* GetBackButton() override;
  ReloadButton* GetReloadButton() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

  static void DisableAnimationForTesting();
  views::View* GetRightContainerForTesting();
  views::View* GetPageActionIconContainerForTesting();

 protected:
  // views::AccessiblePaneView:
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  friend class WebAppNonClientFrameViewAshTest;
  friend class ImmersiveModeControllerAshWebAppBrowserTest;
  friend class WebAppAshInteractiveUITest;

  // Duration to wait before starting the opening animation.
  static constexpr base::TimeDelta kTitlebarAnimationDelay =
      base::TimeDelta::FromMilliseconds(750);

  // Methods for coordinate the titlebar animation (origin text slide, menu
  // highlight and icon fade in).
  bool ShouldAnimate() const;
  void StartTitlebarAnimation();
  void FadeInContentSettingIcons();

  class ContentSettingsContainer;

  views::View* GetContentSettingContainerForTesting();

  const std::vector<ContentSettingImageView*>&
  GetContentSettingViewsForTesting() const;

  SkColor GetCaptionColor() const;
  void UpdateChildrenColor();

  void GenerateMinimalUIButtonImages();

  // Whether we're waiting for the widget to become visible.
  bool pending_widget_visibility_ = true;

  ScopedObserver<views::Widget, views::WidgetObserver> scoped_widget_observer_{
      this};

  ScopedObserver<ui::MaterialDesignController,
                 ui::MaterialDesignControllerObserver>
      md_observer_{this};

  // Timers for synchronising their respective parts of the titlebar animation.
  base::OneShotTimer animation_start_delay_;
  base::OneShotTimer icon_fade_in_delay_;

  // The containing browser view.
  BrowserView* const browser_view_;

  // Button and text colors.
  bool paint_as_active_ = true;
  SkColor active_color_;
  SkColor inactive_color_;

  class ToolbarButtonContainer;

  // All remaining members are owned by the views hierarchy.

  // These three fields are only created when the display mode is minimal-ui.
  ToolbarButtonContainer* left_container_ = nullptr;
  ToolbarButton* back_ = nullptr;
  ReloadButton* reload_ = nullptr;

  // Empty container used by the parent frame to layout additional elements.
  views::View* center_container_ = nullptr;

  ToolbarButtonContainer* right_container_ = nullptr;
  WebAppOriginText* web_app_origin_text_ = nullptr;
  ContentSettingsContainer* content_settings_container_ = nullptr;
  PageActionIconContainerView* page_action_icon_container_view_ = nullptr;
  BrowserActionsContainer* browser_actions_container_ = nullptr;
  ExtensionsToolbarContainer* extensions_container_ = nullptr;
  WebAppMenuButton* web_app_menu_button_ = nullptr;


  DISALLOW_COPY_AND_ASSIGN(WebAppFrameToolbarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_FRAME_TOOLBAR_VIEW_H_
