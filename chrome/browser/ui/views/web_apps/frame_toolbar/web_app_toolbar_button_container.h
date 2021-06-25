// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_TOOLBAR_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_TOOLBAR_BUTTON_CONTAINER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class WebAppContentSettingsContainer;
class BrowserView;
class ToolbarButtonProvider;
class ExtensionsToolbarContainer;
class WebAppMenuButton;
class WebAppOriginText;

class WebAppToolbarButtonContainer : public views::View,
                                     public IconLabelBubbleView::Delegate,
                                     public ContentSettingImageView::Delegate,
                                     public ImmersiveModeController::Observer,
                                     public PageActionIconView::Delegate,
                                     public PageActionIconContainer,
                                     public views::WidgetObserver {
 public:
  METADATA_HEADER(WebAppToolbarButtonContainer);

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

  WebAppToolbarButtonContainer(views::Widget* widget,
                               BrowserView* browser_view,
                               ToolbarButtonProvider* toolbar_button_provider);
  ~WebAppToolbarButtonContainer() override;

  void UpdateStatusIconsVisibility();

  void SetColors(SkColor foreground_color, SkColor background_color);

  views::FlexRule GetFlexRule() const;

  WebAppContentSettingsContainer* content_settings_container() {
    return content_settings_container_;
  }

  PageActionIconController* page_action_icon_controller() {
    return page_action_icon_controller_.get();
  }

  ExtensionsToolbarContainer* extensions_container() {
    return extensions_container_;
  }

  WebAppMenuButton* web_app_menu_button() { return web_app_menu_button_; }

  static void DisableAnimationForTesting();

 private:
  friend class ImmersiveModeControllerChromeosWebAppBrowserTest;

  // Duration to wait before starting the opening animation.
  static constexpr base::TimeDelta kTitlebarAnimationDelay =
      base::TimeDelta::FromMilliseconds(750);

  // PageActionIconContainer:
  void AddPageActionIcon(views::View* icon) override;

  // PageActionIconView::Delegate:
  int GetPageActionIconSize() const override;

  gfx::Insets GetPageActionIconInsets(
      const PageActionIconView* icon_view) const override;

  // Methods for coordinate the titlebar animation (origin text slide, menu
  // highlight and icon fade in).
  bool GetAnimate() const;

  void StartTitlebarAnimation();

  void FadeInContentSettingIcons();

  void ChildPreferredSizeChanged(views::View* child) override;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  // ContentSettingImageView::Delegate:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;
  void OnContentSettingImageBubbleShown(
      ContentSettingImageModel::ImageType type) const override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;

  // PageActionIconView::Delegate:
  content::WebContents* GetWebContentsForPageActionIconView() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // Whether we're waiting for the widget to become visible.
  bool pending_widget_visibility_ = true;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observation_{this};

  // Timers for synchronising their respective parts of the titlebar animation.
  base::OneShotTimer animation_start_delay_;
  base::OneShotTimer icon_fade_in_delay_;

  // The containing browser view.
  BrowserView* const browser_view_;
  ToolbarButtonProvider* const toolbar_button_provider_;

  SkColor foreground_color_ = gfx::kPlaceholderColor;
  SkColor background_color_ = gfx::kPlaceholderColor;

  std::unique_ptr<PageActionIconController> page_action_icon_controller_;
  int page_action_insertion_point_ = 0;

  // All remaining members are owned by the views hierarchy.
  WebAppOriginText* web_app_origin_text_ = nullptr;
  WebAppContentSettingsContainer* content_settings_container_ = nullptr;
  ExtensionsToolbarContainer* extensions_container_ = nullptr;
  WebAppMenuButton* web_app_menu_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_TOOLBAR_BUTTON_CONTAINER_H_
