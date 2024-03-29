// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_TOOLBAR_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_TOOLBAR_BUTTON_CONTAINER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"

class WebAppContentSettingsContainer;
class BrowserView;
class ToolbarButtonProvider;
class ExtensionsToolbarContainer;
class WebAppMenuButton;
class WebAppOriginText;
class WindowControlsOverlayToggleButton;
class SystemAppAccessibleName;
class ExtensionsToolbarCoordinator;

class WebAppToolbarButtonContainer : public views::View,
                                     public IconLabelBubbleView::Delegate,
                                     public ContentSettingImageView::Delegate,
                                     public ImmersiveModeController::Observer,
                                     public PageActionIconView::Delegate,
                                     public PageActionIconContainer {
  METADATA_HEADER(WebAppToolbarButtonContainer, views::View)

 public:
  // Timing parameters for the origin fade animation.
  // These control how long it takes for the origin text and menu button
  // highlight to fade in, pause then fade out.
  static constexpr base::TimeDelta kOriginFadeInDuration =
      base::Milliseconds(800);
  static constexpr base::TimeDelta kOriginPauseDuration =
      base::Milliseconds(2500);
  static constexpr base::TimeDelta kOriginFadeOutDuration =
      base::Milliseconds(800);

  // The total duration of the origin fade animation.
  static base::TimeDelta OriginTotalDuration();

  WebAppToolbarButtonContainer(BrowserView* browser_view,
                               ToolbarButtonProvider* toolbar_button_provider);
  ~WebAppToolbarButtonContainer() override;

  void UpdateStatusIconsVisibility();

  void SetColors(SkColor foreground_color,
                 SkColor background_color,
                 bool color_changed);

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

  ExtensionsToolbarCoordinator* extensions_toolbar_coordinator() {
    return extensions_toolbar_coordinator_.get();
  }

  DownloadToolbarButtonView* download_button() {
    return download_button_.get();
  }

  WebAppMenuButton* web_app_menu_button() { return web_app_menu_button_; }

  WindowControlsOverlayToggleButton* window_controls_overlay_toggle_button() {
    return window_controls_overlay_toggle_button_;
  }

  AvatarToolbarButton* avatar_button() { return avatar_button_; }

  static void DisableAnimationForTesting(bool disable);

 private:
  friend class ImmersiveModeControllerChromeosWebAppBrowserTest;

  // Duration to wait before starting the opening animation.
  static constexpr base::TimeDelta kTitlebarAnimationDelay =
      base::Milliseconds(750);

  // PageActionIconContainer:
  void AddPageActionIcon(std::unique_ptr<views::View> icon) override;

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

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;

  // PageActionIconView::Delegate:
  content::WebContents* GetWebContentsForPageActionIconView() override;

  // views::View:
  void AddedToWidget() override;

#if BUILDFLAG(IS_MAC)
  void AppShimChanged(const webapps::AppId& changed_app_id);
#endif

  // Timers for synchronising their respective parts of the titlebar animation.
  base::OneShotTimer animation_start_delay_;
  base::OneShotTimer icon_fade_in_delay_;

  // The containing browser view.
  const raw_ptr<BrowserView> browser_view_;
  const raw_ptr<ToolbarButtonProvider> toolbar_button_provider_;

  SkColor foreground_color_ = gfx::kPlaceholderColor;
  SkColor background_color_ = gfx::kPlaceholderColor;

  std::unique_ptr<PageActionIconController> page_action_icon_controller_;
  int page_action_insertion_point_ = 0;

  std::unique_ptr<ExtensionsToolbarCoordinator> extensions_toolbar_coordinator_;

#if BUILDFLAG(IS_MAC)
  base::CallbackListSubscription app_shim_registry_observation_;
#endif

  // All remaining members are owned by the views hierarchy.
  raw_ptr<WebAppOriginText> web_app_origin_text_ = nullptr;
  raw_ptr<WindowControlsOverlayToggleButton>
      window_controls_overlay_toggle_button_ = nullptr;
  raw_ptr<WebAppContentSettingsContainer> content_settings_container_ = nullptr;
  raw_ptr<ExtensionsToolbarContainer> extensions_container_ = nullptr;
  raw_ptr<WebAppMenuButton> web_app_menu_button_ = nullptr;
  raw_ptr<SystemAppAccessibleName> system_app_accessible_name_ = nullptr;
  raw_ptr<DownloadToolbarButtonView> download_button_ = nullptr;
  raw_ptr<AvatarToolbarButton> avatar_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_TOOLBAR_BUTTON_CONTAINER_H_
