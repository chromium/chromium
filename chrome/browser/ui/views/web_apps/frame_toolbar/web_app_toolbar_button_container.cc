// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_coordinator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/system_app_accessible_name.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_content_settings_container.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_origin_text.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/window_controls_overlay_toggle_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/window/hit_test_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

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
    BrowserView* browser_view,
    ToolbarButtonProvider* toolbar_button_provider)
    : browser_view_(browser_view),
      toolbar_button_provider_(toolbar_button_provider),
      page_action_icon_controller_(
          std::make_unique<PageActionIconController>()) {
#if BUILDFLAG(IS_MAC)
  app_shim_registry_observation_ =
      AppShimRegistry::Get()->RegisterAppChangedCallback(
          base::BindRepeating(&WebAppToolbarButtonContainer::AppShimChanged,
                              base::Unretained(this)));
#endif

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(gfx::Insets::VH(0, WebAppFrameRightMargin()))
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets::VH(
              0, HorizontalPaddingBetweenPageActionsAndAppMenuButtons()))
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

  // App's origin will not be shown in the borderless mode, it will only be
  // visible in App Settings UI.
  if (app_controller->HasTitlebarAppOriginText() &&
      !browser_view_->IsBorderlessModeEnabled()) {
    web_app_origin_text_ = AddChildView(
        std::make_unique<WebAppOriginText>(browser_view_->browser()));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (app_controller->system_app()) {
    system_app_accessible_name_ =
        AddChildView(std::make_unique<SystemAppAccessibleName>(
            app_controller->GetAppShortName()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (app_controller->AppUsesWindowControlsOverlay()) {
    window_controls_overlay_toggle_button_ = AddChildView(
        std::make_unique<WindowControlsOverlayToggleButton>(browser_view_));
    views::SetHitTestComponent(window_controls_overlay_toggle_button_,
                               static_cast<int>(HTCLIENT));
    ConfigureWebAppToolbarButton(window_controls_overlay_toggle_button_,
                                 toolbar_button_provider_);
    window_controls_overlay_toggle_button_->SetVisible(
        browser_view_->should_show_window_controls_overlay_toggle());
  }

  if (app_controller->HasTitlebarContentSettings()) {
    content_settings_container_ = AddChildView(
        std::make_unique<WebAppContentSettingsContainer>(this, this));
    views::SetHitTestComponent(content_settings_container_,
                               static_cast<int>(HTCLIENT));
  }

  // This is the point where we will be inserting page action icons.
  page_action_insertion_point_ = static_cast<int>(children().size());

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

  bool create_extensions_container = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Do not create the extensions or browser actions container if it is a
  // System Web App.
  create_extensions_container = !ash::IsSystemWebApp(browser_view_->browser());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (create_extensions_container) {
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
    extensions_toolbar_coordinator_ =
        std::make_unique<ExtensionsToolbarCoordinator>(browser_view_->browser(),
                                                       extensions_container_);

    extensions_container_->GetExtensionsButton()
        ->SetAppearDisabledInInactiveWidget(true);
    extensions_container_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            extensions_container_->GetAnimatingLayoutManager()
                ->GetDefaultFlexRule())
            .WithOrder(kLowPriorityFlexOrder));
    views::SetHitTestComponent(extensions_container_,
                               static_cast<int>(HTCLIENT));
  }

  if (download::IsDownloadBubbleEnabled()) {
    download_button_ = AddChildView(
        std::make_unique<DownloadToolbarButtonView>(browser_view_));
    views::SetHitTestComponent(download_button_, static_cast<int>(HTCLIENT));
    ConfigureWebAppToolbarButton(download_button_, toolbar_button_provider_);
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (app_controller->HasProfileMenuButton()) {
    avatar_button_ =
        AddChildView(std::make_unique<AvatarToolbarButton>(browser_view_));
    avatar_button_->SetID(VIEW_ID_AVATAR_BUTTON);
    ConfigureWebAppToolbarButton(avatar_button_, toolbar_button_provider_);
    views::SetHitTestComponent(avatar_button_, static_cast<int>(HTCLIENT));
    avatar_button_->SetVisible(app_controller->IsProfileMenuButtonVisible());
  }
#endif

  if (app_controller->HasTitlebarMenuButton()) {
    web_app_menu_button_ =
        AddChildView(std::make_unique<WebAppMenuButton>(browser_view_));
    web_app_menu_button_->SetID(VIEW_ID_APP_MENU);
    ConfigureWebAppToolbarButton(web_app_menu_button_,
                                 toolbar_button_provider_);
    web_app_menu_button_->SetProperty(views::kFlexBehaviorKey,
                                      views::FlexSpecification());
  }

  browser_view_->immersive_mode_controller()->AddObserver(this);
}

WebAppToolbarButtonContainer::~WebAppToolbarButtonContainer() {
  ImmersiveModeController* immersive_controller =
      browser_view_->immersive_mode_controller();
  if (immersive_controller) {
    immersive_controller->RemoveObserver(this);
  }
}

void WebAppToolbarButtonContainer::UpdateStatusIconsVisibility() {
  if (content_settings_container_) {
    content_settings_container_->UpdateContentSettingViewsVisibility();
  }
  page_action_icon_controller_->UpdateAll();
}

void WebAppToolbarButtonContainer::SetColors(SkColor foreground_color,
                                             SkColor background_color,
                                             bool color_changed) {
  foreground_color_ = foreground_color;
  background_color_ = background_color;
  if (web_app_origin_text_) {
    web_app_origin_text_->SetTextColor(foreground_color_,
                                       /*show_text=*/color_changed);
  }

  if (content_settings_container_) {
    content_settings_container_->SetIconColor(foreground_color_);
  }
  page_action_icon_controller_->SetIconColor(foreground_color_);
}

views::FlexRule WebAppToolbarButtonContainer::GetFlexRule() const {
  // Prefer height consistency over accommodating edge case icons that may
  // bump up the container height (e.g. extension action icons with badges).
  // TODO(crbug.com/41417506): Fix the inconsistent icon sizes found in
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

void WebAppToolbarButtonContainer::DisableAnimationForTesting(bool disable) {
  g_animation_disabled_for_testing = disable;
}

void WebAppToolbarButtonContainer::AddPageActionIcon(
    std::unique_ptr<views::View> icon) {
  auto* icon_ptr =
      AddChildViewAt(std::move(icon), page_action_insertion_point_++);
  views::SetHitTestComponent(icon_ptr, static_cast<int>(HTCLIENT));
}

int WebAppToolbarButtonContainer::GetPageActionIconSize() const {
  return GetLayoutConstant(WEB_APP_PAGE_ACTION_ICON_SIZE);
}

gfx::Insets WebAppToolbarButtonContainer::GetPageActionIconInsets(
    const PageActionIconView* icon_view) const {
  const int icon_size =
      icon_view->GetImageContainerView()->GetPreferredSize().height();
  if (icon_size == 0) {
    return gfx::Insets();
  }

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
  if (!GetAnimate()) {
    return;
  }

  if (web_app_origin_text_) {
    web_app_origin_text_->SetAllowedToAnimate(true);
    web_app_origin_text_->StartFadeAnimation();
  }
  if (web_app_menu_button_) {
    web_app_menu_button_->StartHighlightAnimation();
  }
  icon_fade_in_delay_.Start(
      FROM_HERE, OriginTotalDuration(), this,
      &WebAppToolbarButtonContainer::FadeInContentSettingIcons);
}

void WebAppToolbarButtonContainer::FadeInContentSettingIcons() {
  if (!GetAnimate()) {
    return;
  }

  if (content_settings_container_) {
    content_settings_container_->FadeIn();
  }
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

// ImmersiveModeController::Observer:
void WebAppToolbarButtonContainer::OnImmersiveRevealStarted() {
  // Don't wait for the fade in animation to make content setting icons
  // visible once in immersive mode.
  if (content_settings_container_) {
    content_settings_container_->EnsureVisible();
  }
}

// PageActionIconView::Delegate:
content::WebContents*
WebAppToolbarButtonContainer::GetWebContentsForPageActionIconView() {
  return browser_view_->GetActiveWebContents();
}

void WebAppToolbarButtonContainer::AddedToWidget() {
  if (GetAnimate()) {
    if (content_settings_container_) {
      content_settings_container_->SetUpForFadeIn();
    }
    animation_start_delay_.Start(
        FROM_HERE, kTitlebarAnimationDelay, this,
        &WebAppToolbarButtonContainer::StartTitlebarAnimation);
  }
}

#if BUILDFLAG(IS_MAC)
void WebAppToolbarButtonContainer::AppShimChanged(
    const webapps::AppId& changed_app_id) {
  const auto* app_controller = browser_view_->browser()->app_controller();
  if (changed_app_id != app_controller->app_id()) {
    return;
  }
  if (avatar_button_) {
    avatar_button_->SetVisible(app_controller->IsProfileMenuButtonVisible());
  }
}
#endif

BEGIN_METADATA(WebAppToolbarButtonContainer)
ADD_READONLY_PROPERTY_METADATA(bool, Animate)
END_METADATA
