// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_content_settings_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_origin_text.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/hit_test.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/window/hit_test_utils.h"

namespace {

bool g_animation_disabled_for_testing = false;

}  // namespace

constexpr base::TimeDelta WebAppToolbarButtonContainer::kTitlebarAnimationDelay;
constexpr base::TimeDelta WebAppToolbarButtonContainer::kOriginFadeInDuration;
constexpr base::TimeDelta WebAppToolbarButtonContainer::kOriginPauseDuration;
constexpr base::TimeDelta WebAppToolbarButtonContainer::kOriginFadeOutDuration;

// static
base::TimeDelta WebAppToolbarButtonContainer::OriginTotalDuration() {
  // TimeDelta.operator+ uses time_internal::SaturatedAdd() which isn't
  // constexpr, so this needs to be a function to not introduce a static
  // initializer.
  return kOriginFadeInDuration + kOriginPauseDuration + kOriginFadeOutDuration;
}

WebAppToolbarButtonContainer::WebAppToolbarButtonContainer(
    views::Widget* widget,
    BrowserView* browser_view,
    ToolbarButtonProvider* toolbar_button_provider)
    : browser_view_(browser_view),
      toolbar_button_provider_(toolbar_button_provider),
      page_action_icon_controller_(
          std::make_unique<PageActionIconController>()) {
  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(gfx::Insets(0, WebAppFrameRightMargin()))
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(0,
                      HorizontalPaddingBetweenPageActionsAndAppMenuButtons()))
      .SetCollapseMargins(true)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::LayoutOrientation::kHorizontal,
                      views::MinimumFlexSizeRule::kPreferredSnapToZero)
                      .WithWeight(0))
      .SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse);

  const auto* app_controller = browser_view_->browser()->app_controller();

  if (app_controller->HasTitlebarAppOriginText()) {
    web_app_origin_text_ = AddChildView(
        std::make_unique<WebAppOriginText>(browser_view_->browser()));
  }

  if (app_controller->HasTitlebarContentSettings()) {
    content_settings_container_ = AddChildView(
        std::make_unique<WebAppContentSettingsContainer>(this, this));
    views::SetHitTestComponent(content_settings_container_,
                               static_cast<int>(HTCLIENT));
  }

  // This is the point where we will be inserting page action icons.
  page_action_insertion_point_ = int{children().size()};

  // Insert the default page action icons.
  PageActionIconParams params;
  params.types_enabled = app_controller->GetTitleBarPageActions();
  params.icon_color = gfx::kPlaceholderColor;
  params.between_icon_spacing =
      HorizontalPaddingBetweenPageActionsAndAppMenuButtons();
  params.browser = browser_view_->browser();
  params.command_updater = browser_view_->browser()->command_controller();
  params.icon_label_bubble_delegate = this;
  params.page_action_icon_delegate = this;
  page_action_icon_controller_->Init(params, this);

  // Do not create the extensions or browser actions container if it is a
  // System Web App.
  if (!web_app::IsSystemWebApp(browser_view_->browser())) {
    // Extensions toolbar area with pinned extensions is lower priority than,
    // for example, the menu button or other toolbar buttons, and pinned
    // extensions should hide before other toolbar buttons.
    constexpr int kLowPriorityFlexOrder = 2;

    auto display_mode =
        base::FeatureList::IsEnabled(features::kDesktopPWAsElidedExtensionsMenu)
            ? ExtensionsToolbarContainer::DisplayMode::kAutoHide
            : ExtensionsToolbarContainer::DisplayMode::kCompact;
    extensions_container_ =
        AddChildView(std::make_unique<ExtensionsToolbarContainer>(
            browser_view_->browser(), display_mode));
    extensions_container_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            extensions_container_->GetAnimatingLayoutManager()
                ->GetDefaultFlexRule())
            .WithOrder(kLowPriorityFlexOrder));
    views::SetHitTestComponent(extensions_container_,
                               static_cast<int>(HTCLIENT));
  }

  if (app_controller->HasTitlebarMenuButton()) {
    web_app_menu_button_ =
        AddChildView(std::make_unique<WebAppMenuButton>(browser_view_));
    web_app_menu_button_->SetID(VIEW_ID_APP_MENU);
    const bool is_browser_focus_mode =
        browser_view_->browser()->is_focus_mode();
    SetInsetsForWebAppToolbarButton(web_app_menu_button_,
                                    is_browser_focus_mode);
    web_app_menu_button_->SetMinSize(
        toolbar_button_provider_->GetToolbarButtonSize());
    web_app_menu_button_->SetProperty(views::kFlexBehaviorKey,
                                      views::FlexSpecification());
  }

  browser_view_->immersive_mode_controller()->AddObserver(this);
  scoped_widget_observation_.Observe(widget);
}

WebAppToolbarButtonContainer::~WebAppToolbarButtonContainer() {
  ImmersiveModeController* immersive_controller =
      browser_view_->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);
}

void WebAppToolbarButtonContainer::UpdateStatusIconsVisibility() {
  if (content_settings_container_)
    content_settings_container_->UpdateContentSettingViewsVisibility();
  page_action_icon_controller_->UpdateAll();
}

void WebAppToolbarButtonContainer::SetColors(SkColor foreground_color,
                                             SkColor background_color) {
  foreground_color_ = foreground_color;
  background_color_ = background_color;
  if (web_app_origin_text_)
    web_app_origin_text_->SetTextColor(foreground_color_);
  if (content_settings_container_)
    content_settings_container_->SetIconColor(foreground_color_);
  if (extensions_container_)
    extensions_container_->SetIconColor(foreground_color_);
  page_action_icon_controller_->SetIconColor(foreground_color_);
  if (web_app_menu_button_)
    web_app_menu_button_->SetColor(foreground_color_);
}

views::FlexRule WebAppToolbarButtonContainer::GetFlexRule() const {
  // Prefer height consistency over accommodating edge case icons that may
  // bump up the container height (e.g. extension action icons with badges).
  // TODO(https://crbug.com/889745): Fix the inconsistent icon sizes found in
  // the right-hand container and turn this into a DCHECK that the container
  // height is the same as the app menu button height.
  const auto* const layout =
      static_cast<views::FlexLayout*>(GetLayoutManager());
  return base::BindRepeating(
      [](ToolbarButtonProvider* toolbar_button_provider,
         views::FlexRule input_flex_rule, const views::View* view,
         const views::SizeBounds& available_size) {
        const gfx::Size preferred = input_flex_rule.Run(view, available_size);
        return gfx::Size(
            preferred.width(),
            toolbar_button_provider->GetToolbarButtonSize().height());
      },
      base::Unretained(toolbar_button_provider_), layout->GetDefaultFlexRule());
}

void WebAppToolbarButtonContainer::DisableAnimationForTesting() {
  g_animation_disabled_for_testing = true;
}

void WebAppToolbarButtonContainer::AddPageActionIcon(views::View* icon) {
  AddChildViewAt(icon, page_action_insertion_point_++);
  views::SetHitTestComponent(icon, static_cast<int>(HTCLIENT));
}

int WebAppToolbarButtonContainer::GetPageActionIconSize() const {
  return GetLayoutConstant(WEB_APP_PAGE_ACTION_ICON_SIZE);
}

gfx::Insets WebAppToolbarButtonContainer::GetPageActionIconInsets(
    const PageActionIconView* icon_view) const {
  const int icon_size = icon_view->GetImageView()->GetPreferredSize().height();
  if (icon_size == 0)
    return gfx::Insets();

  const int height = toolbar_button_provider_->GetToolbarButtonSize().height();
  const int inset_size = std::max(0, (height - icon_size) / 2);
  return gfx::Insets(inset_size);
}

// Methods for coordinate the titlebar animation (origin text slide, menu
// highlight and icon fade in).
bool WebAppToolbarButtonContainer::GetAnimate() const {
  return !g_animation_disabled_for_testing &&
         !browser_view_->immersive_mode_controller()->IsEnabled();
}

void WebAppToolbarButtonContainer::StartTitlebarAnimation() {
  if (!GetAnimate())
    return;

  if (web_app_origin_text_)
    web_app_origin_text_->StartFadeAnimation();
  if (web_app_menu_button_)
    web_app_menu_button_->StartHighlightAnimation();
  icon_fade_in_delay_.Start(
      FROM_HERE, OriginTotalDuration(), this,
      &WebAppToolbarButtonContainer::FadeInContentSettingIcons);
}

void WebAppToolbarButtonContainer::FadeInContentSettingIcons() {
  if (content_settings_container_)
    content_settings_container_->FadeIn();
}

void WebAppToolbarButtonContainer::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

SkColor
WebAppToolbarButtonContainer::GetIconLabelBubbleSurroundingForegroundColor()
    const {
  return foreground_color_;
}

SkColor WebAppToolbarButtonContainer::GetIconLabelBubbleBackgroundColor()
    const {
  return background_color_;
}

bool WebAppToolbarButtonContainer::ShouldHideContentSettingImage() {
  return false;
}

content::WebContents*
WebAppToolbarButtonContainer::GetContentSettingWebContents() {
  return browser_view_->GetActiveWebContents();
}

ContentSettingBubbleModelDelegate*
WebAppToolbarButtonContainer::GetContentSettingBubbleModelDelegate() {
  return browser_view_->browser()->content_setting_bubble_model_delegate();
}

void WebAppToolbarButtonContainer::OnContentSettingImageBubbleShown(
    ContentSettingImageModel::ImageType type) const {
  UMA_HISTOGRAM_ENUMERATION(
      "HostedAppFrame.ContentSettings.ImagePressed", type,
      ContentSettingImageModel::ImageType::NUM_IMAGE_TYPES);
}

// ImmersiveModeController::Observer:
void WebAppToolbarButtonContainer::OnImmersiveRevealStarted() {
  // Don't wait for the fade in animation to make content setting icons
  // visible once in immersive mode.
  if (content_settings_container_)
    content_settings_container_->EnsureVisible();
}

// PageActionIconView::Delegate:
content::WebContents*
WebAppToolbarButtonContainer::GetWebContentsForPageActionIconView() {
  return browser_view_->GetActiveWebContents();
}

// views::WidgetObserver:
void WebAppToolbarButtonContainer::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  if (!visible || !pending_widget_visibility_)
    return;
  pending_widget_visibility_ = false;
  if (GetAnimate()) {
    if (content_settings_container_)
      content_settings_container_->SetUpForFadeIn();
    animation_start_delay_.Start(
        FROM_HERE, kTitlebarAnimationDelay, this,
        &WebAppToolbarButtonContainer::StartTitlebarAnimation);
  }
}

BEGIN_METADATA(WebAppToolbarButtonContainer, views::View)
ADD_READONLY_PROPERTY_METADATA(bool, Animate)
END_METADATA
