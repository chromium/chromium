// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace {
class HostedAppNonClientFrameViewAshTest;
}

class AppMenuButton;
class BrowserView;
class HostedAppOriginText;
class HostedAppMenuButton;

namespace views {
class MenuButton;
class Widget;
}

// A container for hosted app buttons in the title bar.
class HostedAppButtonContainer : public views::AccessiblePaneView,
                                 public BrowserActionsContainer::Delegate,
                                 public ContentSettingImageView::Delegate,
                                 public ImmersiveModeController::Observer,
                                 public PageActionIconView::Delegate,
                                 public ToolbarButtonProvider,
                                 public views::WidgetObserver {
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
  HostedAppButtonContainer(views::Widget* widget,
                           BrowserView* browser_view,
                           SkColor active_color,
                           SkColor inactive_color,
                           base::Optional<int> right_margin = base::nullopt);
  ~HostedAppButtonContainer() override;

  void UpdateContentSettingViewsVisibility();

  // Sets the container to paints its buttons the active/inactive color.
  void SetPaintAsActive(bool active);

  // Sets own bounds to be right aligned and vertically centered in the given
  // space, returns a new trailing_x value.
  int LayoutInContainer(int leading_x,
                        int trailing_x,
                        int y,
                        int available_height);

  SkColor active_color_for_testing() const { return active_color_; }

  // views::AccessiblePaneView:
  const char* GetClassName() const override;

  // BrowserActionsContainer::Delegate:
  views::MenuButton* GetOverflowReferenceView() override;
  base::Optional<int> GetMaxBrowserActionsWidth() const override;
  std::unique_ptr<ToolbarActionsBar> CreateToolbarActionsBar(
      ToolbarActionsBarDelegate* delegate,
      Browser* browser,
      ToolbarActionsBar* main_bar) const override;

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
  PageActionIconContainerView* GetPageActionIconContainerView() override;
  AppMenuButton* GetAppMenuButton() override;
  gfx::Rect GetFindBarBoundingBox(int contents_height) const override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

 protected:
  // views::AccessiblePaneView:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

 private:
  friend class HostedAppNonClientFrameViewAshTest;
  friend class HostedAppGlassBrowserFrameViewTest;
  friend class ImmersiveModeControllerAshHostedAppBrowserTest;
  friend class HostedAppAshInteractiveUITest;

  // Duration to wait before starting the opening animation.
  static constexpr base::TimeDelta kTitlebarAnimationDelay =
      base::TimeDelta::FromMilliseconds(750);

  // Methods for coordinate the titlebar animation (origin text slide, menu
  // highlight and icon fade in).
  bool ShouldAnimate() const;
  void StartTitlebarAnimation();
  void FadeInContentSettingIcons();
  static void DisableAnimationForTesting();

  class ContentSettingsContainer;

  views::View* GetContentSettingContainerForTesting();

  const std::vector<ContentSettingImageView*>&
  GetContentSettingViewsForTesting() const;

  SkColor GetIconColor() const;
  void UpdateChildrenColor();

  // Whether we're waiting for the widget to become visible.
  bool pending_widget_visibility_ = true;

  ScopedObserver<views::Widget, views::WidgetObserver> scoped_widget_observer_;

  // Timers for synchronising their respective parts of the titlebar animation.
  base::OneShotTimer animation_start_delay_;
  base::OneShotTimer icon_fade_in_delay_;

  // The containing browser view.
  BrowserView* browser_view_;

  // Button and text colors.
  bool paint_as_active_ = true;
  const SkColor active_color_;
  const SkColor inactive_color_;

  // Owned by the views hierarchy.
  HostedAppOriginText* hosted_app_origin_text_ = nullptr;
  ContentSettingsContainer* content_settings_container_ = nullptr;
  PageActionIconContainerView* page_action_icon_container_view_ = nullptr;
  BrowserActionsContainer* browser_actions_container_ = nullptr;
  HostedAppMenuButton* app_menu_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HostedAppButtonContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_BUTTON_CONTAINER_H_
