// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/scoped_observer.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/web_apps/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/web_app_origin_text.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/hit_test.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/views/window/hit_test_utils.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

bool g_animation_disabled_for_testing = false;

constexpr base::TimeDelta kContentSettingsFadeInDuration =
    base::TimeDelta::FromMilliseconds(500);

constexpr int kPaddingBetweenNavigationButtons = 9;

#if defined(OS_CHROMEOS)
constexpr int kWebAppFrameLeftMargin = 4;
#else
constexpr int kWebAppFrameLeftMargin = 9;
#endif

class WebAppToolbarActionsBar : public ToolbarActionsBar {
 public:
  using ToolbarActionsBar::ToolbarActionsBar;

  gfx::Insets GetIconAreaInsets() const override {
    // TODO(calamity): Unify these toolbar action insets with other clients once
    // all toolbar button sizings are consolidated. https://crbug.com/822967.
    return gfx::Insets(2);
  }

  size_t GetIconCount() const override {
    // Only show an icon when an extension action is popped out due to
    // activation, and none otherwise.
    return GetPoppedOutAction() ? 1 : 0;
  }

  int GetMinimumWidth() const override {
    // Allow the BrowserActionsContainer to collapse completely and be hidden
    return 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppToolbarActionsBar);
};

template <class BaseClass>
class WebAppToolbarButton : public BaseClass {
 public:
  using BaseClass::BaseClass;
  WebAppToolbarButton(const WebAppToolbarButton&) = delete;
  WebAppToolbarButton& operator=(const WebAppToolbarButton&) = delete;
  ~WebAppToolbarButton() override = default;

#if defined(OS_WIN)
  bool ShouldUseWindowsIconsForMinimalUI() const {
    return base::win::GetVersion() >= base::win::Version::WIN10;
  }
#endif

  void SetIconColor(SkColor icon_color) {
    if (icon_color_ == icon_color)
      return;

    icon_color_ = icon_color;
    UpdateIcon();
  }

  virtual const gfx::VectorIcon* GetAlternativeIcon() const { return nullptr; }

  // ToolbarButton:
  void UpdateIcon() override {
    if (const auto* icon = GetAlternativeIcon()) {
      BaseClass::UpdateIconsWithStandardColors(*icon);
      return;
    }

    BaseClass::UpdateIcon();
  }

 protected:
  // ToolbarButton:
  SkColor GetForegroundColor(views::Button::ButtonState state) const override {
    if (state == views::Button::STATE_DISABLED)
      return SkColorSetA(icon_color_, gfx::kDisabledControlAlpha);

    return icon_color_;
  }

 private:
  SkColor icon_color_ = gfx::kPlaceholderColor;
};

class WebAppToolbarBackButton : public WebAppToolbarButton<BackForwardButton> {
 public:
  WebAppToolbarBackButton(PressedCallback callback, Browser* browser);
  WebAppToolbarBackButton(const WebAppToolbarBackButton&) = delete;
  WebAppToolbarBackButton& operator=(const WebAppToolbarBackButton&) = delete;
  ~WebAppToolbarBackButton() override = default;

  // WebAppToolbarButton:
  const gfx::VectorIcon* GetAlternativeIcon() const override;
};

WebAppToolbarBackButton::WebAppToolbarBackButton(PressedCallback callback,
                                                 Browser* browser)
    : WebAppToolbarButton<BackForwardButton>(
          BackForwardButton::Direction::kBack,
          std::move(callback),
          browser) {}

const gfx::VectorIcon* WebAppToolbarBackButton::GetAlternativeIcon() const {
#if defined(OS_WIN)
  if (ShouldUseWindowsIconsForMinimalUI()) {
    return ui::TouchUiController::Get()->touch_ui()
               ? &kBackArrowWindowsTouchIcon
               : &kBackArrowWindowsIcon;
  }
#endif
  return nullptr;
}

class WebAppToolbarReloadButton : public WebAppToolbarButton<ReloadButton> {
 public:
  using WebAppToolbarButton<ReloadButton>::WebAppToolbarButton;
  WebAppToolbarReloadButton(const WebAppToolbarReloadButton&) = delete;
  WebAppToolbarReloadButton& operator=(const WebAppToolbarReloadButton&) =
      delete;
  ~WebAppToolbarReloadButton() override = default;

  // WebAppToolbarButton:
  const gfx::VectorIcon* GetAlternativeIcon() const override;
};

const gfx::VectorIcon* WebAppToolbarReloadButton::GetAlternativeIcon() const {
#if defined(OS_WIN)
  if (ShouldUseWindowsIconsForMinimalUI()) {
    const bool is_reload = visible_mode() == ReloadButton::Mode::kReload;
    if (ui::TouchUiController::Get()->touch_ui()) {
      return is_reload ? &kReloadWindowsTouchIcon
                       : &kNavigateStopWindowsTouchIcon;
    }
    return is_reload ? &kReloadWindowsIcon : &kNavigateStopWindowsIcon;
  }
#endif
  return nullptr;
}

int HorizontalPaddingBetweenPageActionsAndAppMenuButtons() {
  return views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
}

int WebAppFrameRightMargin() {
#if defined(OS_MAC)
  return kWebAppMenuMargin;
#else
  return HorizontalPaddingBetweenPageActionsAndAppMenuButtons();
#endif
}

// An ink drop with round corners in shown when the user hovers over the button.
// Insets are kept small to avoid increasing web app frame toolbar height.
void SetInsetsForWebAppToolbarButton(ToolbarButton* toolbar_button,
                                     bool is_browser_focus_mode) {
  if (!is_browser_focus_mode)
    toolbar_button->SetLayoutInsets(gfx::Insets(2));
}

}  // namespace

const char WebAppFrameToolbarView::kViewClassName[] = "WebAppFrameToolbarView";

constexpr base::TimeDelta WebAppFrameToolbarView::kTitlebarAnimationDelay;
constexpr base::TimeDelta WebAppFrameToolbarView::kOriginFadeInDuration;
constexpr base::TimeDelta WebAppFrameToolbarView::kOriginPauseDuration;
constexpr base::TimeDelta WebAppFrameToolbarView::kOriginFadeOutDuration;

// static
base::TimeDelta WebAppFrameToolbarView::OriginTotalDuration() {
  // TimeDelta.operator+ uses time_internal::SaturatedAdd() which isn't
  // constexpr, so this needs to be a function to not introduce a static
  // initializer.
  return kOriginFadeInDuration + kOriginPauseDuration + kOriginFadeOutDuration;
}

class WebAppFrameToolbarView::ContentSettingsContainer : public views::View {
 public:
  ContentSettingsContainer(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      ContentSettingImageView::Delegate* content_setting_image_delegate);
  ~ContentSettingsContainer() override = default;

  void UpdateContentSettingViewsVisibility() {
    for (auto* v : content_setting_views_)
      v->Update();
  }

  // Sets the color of the content setting icons.
  void SetIconColor(SkColor icon_color) {
    for (auto* v : content_setting_views_)
      v->SetIconColor(icon_color);
  }

  void SetUpForFadeIn() {
    SetVisible(false);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(0);
  }

  void FadeIn() {
    if (GetVisible())
      return;
    SetVisible(true);
    DCHECK_EQ(layer()->opacity(), 0);
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(kContentSettingsFadeInDuration);
    layer()->SetOpacity(1);
  }

  void EnsureVisible() {
    SetVisible(true);
    if (layer())
      layer()->SetOpacity(1);
  }

  const std::vector<ContentSettingImageView*>& get_content_setting_views()
      const {
    return content_setting_views_;
  }

 private:
  // views::View:
  const char* GetClassName() const override {
    return "WebAppFrameToolbarView::ContentSettingsContainer";
  }

  // Owned by the views hierarchy.
  std::vector<ContentSettingImageView*> content_setting_views_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsContainer);
};

WebAppFrameToolbarView::ContentSettingsContainer::ContentSettingsContainer(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    ContentSettingImageView::Delegate* content_setting_image_delegate) {
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), icon_label_bubble_delegate,
        content_setting_image_delegate,
        views::CustomFrameView::GetWindowTitleFontList());
    // Padding around content setting icons.
    constexpr auto kContentSettingIconInteriorPadding = gfx::Insets(4);
    image_view->SetBorder(
        views::CreateEmptyBorder(kContentSettingIconInteriorPadding));
    image_view->disable_animation();
    views::SetHitTestComponent(image_view.get(), static_cast<int>(HTCLIENT));
    content_setting_views_.push_back(image_view.get());
    AddChildView(image_view.release());
  }
}

// Holds controls in the far left of the toolbar.
class WebAppFrameToolbarView::NavigationButtonContainer
    : public views::View,
      public CommandObserver {
 public:
  explicit NavigationButtonContainer(BrowserView* browser_view);
  ~NavigationButtonContainer() override;

  WebAppToolbarBackButton* back_button() { return back_button_; }
  WebAppToolbarReloadButton* reload_button() { return reload_button_; }

  void SetIconColor(SkColor icon_color) {
    back_button_->SetIconColor(icon_color);
    reload_button_->SetIconColor(icon_color);
  }

 protected:
  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override {
    switch (id) {
      case IDC_BACK:
        back_button_->SetEnabled(enabled);
        break;
      case IDC_RELOAD:
        reload_button_->SetEnabled(enabled);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  // views::View:
  const char* GetClassName() const override {
    return "WebAppFrameToolbarView::NavigationButtonContainer";
  }

  // The containing browser.
  Browser* const browser_;

  // These members are owned by the views hierarchy.
  WebAppToolbarBackButton* back_button_ = nullptr;
  WebAppToolbarReloadButton* reload_button_ = nullptr;
};

WebAppFrameToolbarView::NavigationButtonContainer::NavigationButtonContainer(
    BrowserView* browser_view)
    : browser_(browser_view->browser()) {
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kWebAppFrameLeftMargin),
          kPaddingBetweenNavigationButtons));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout.set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  back_button_ = AddChildView(std::make_unique<WebAppToolbarBackButton>(
      base::BindRepeating(
          [](Browser* browser, const ui::Event& event) {
            chrome::ExecuteCommandWithDisposition(
                browser, IDC_BACK,
                ui::DispositionFromEventFlags(event.flags()));
          },
          browser_),
      browser_));
  back_button_->set_tag(IDC_BACK);
  reload_button_ = AddChildView(std::make_unique<WebAppToolbarReloadButton>(
      browser_->command_controller()));
  reload_button_->set_tag(IDC_RELOAD);

  const bool is_browser_focus_mode = browser_->is_focus_mode();
  SetInsetsForWebAppToolbarButton(back_button_, is_browser_focus_mode);
  SetInsetsForWebAppToolbarButton(reload_button_, is_browser_focus_mode);

  views::SetHitTestComponent(back_button_, static_cast<int>(HTCLIENT));
  views::SetHitTestComponent(reload_button_, static_cast<int>(HTCLIENT));

  chrome::AddCommandObserver(browser_, IDC_BACK, this);
  chrome::AddCommandObserver(browser_, IDC_RELOAD, this);
}

WebAppFrameToolbarView::NavigationButtonContainer::
    ~NavigationButtonContainer() {
  chrome::RemoveCommandObserver(browser_, IDC_BACK, this);
  chrome::RemoveCommandObserver(browser_, IDC_RELOAD, this);
}

// Holds controls in the far right of the toolbar.
// Forces a layout of the toolbar (and hence the window text) whenever a control
// changes visibility.
class WebAppFrameToolbarView::ToolbarButtonContainer
    : public views::View,
      public BrowserActionsContainer::Delegate,
      public IconLabelBubbleView::Delegate,
      public ContentSettingImageView::Delegate,
      public ImmersiveModeController::Observer,
      public PageActionIconView::Delegate,
      public PageActionIconContainer,
      public views::WidgetObserver {
 public:
  ToolbarButtonContainer(views::Widget* widget,
                         BrowserView* browser_view,
                         ToolbarButtonProvider* toolbar_button_provider);
  ~ToolbarButtonContainer() override;

  void UpdateStatusIconsVisibility() {
    if (content_settings_container_)
      content_settings_container_->UpdateContentSettingViewsVisibility();
    page_action_icon_controller_->UpdateAll();
  }

  void SetColors(SkColor foreground_color, SkColor background_color) {
    foreground_color_ = foreground_color;
    background_color_ = background_color;
    if (web_app_origin_text_)
      web_app_origin_text_->SetTextColor(foreground_color_);
    if (content_settings_container_)
      content_settings_container_->SetIconColor(foreground_color_);
    if (extensions_container_)
      extensions_container_->OverrideIconColor(foreground_color_);
    page_action_icon_controller_->SetIconColor(foreground_color_);
    if (web_app_menu_button_)
      web_app_menu_button_->SetColor(foreground_color_);
  }

  views::FlexRule GetFlexRule() const {
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
        base::Unretained(toolbar_button_provider_),
        layout->GetDefaultFlexRule());
  }

  ContentSettingsContainer* content_settings_container() {
    return content_settings_container_;
  }

  PageActionIconController* page_action_icon_controller() {
    return page_action_icon_controller_.get();
  }

  BrowserActionsContainer* browser_actions_container() {
    return browser_actions_container_;
  }

  ExtensionsToolbarContainer* extensions_container() {
    return extensions_container_;
  }

  WebAppMenuButton* web_app_menu_button() { return web_app_menu_button_; }

 private:
  // views::View:
  const char* GetClassName() const override {
    return "WebAppFrameToolbarView::ToolbarButtonContainer";
  }

  // PageActionIconContainer:
  void AddPageActionIcon(views::View* icon) override {
    AddChildViewAt(icon, page_action_insertion_point_++);
    views::SetHitTestComponent(icon, static_cast<int>(HTCLIENT));
  }

  // PageActionIconView::Delegate:
  int GetPageActionIconSize() const override {
    return GetLayoutConstant(WEB_APP_PAGE_ACTION_ICON_SIZE);
  }

  gfx::Insets GetPageActionIconInsets(
      const PageActionIconView* icon_view) const override {
    const int icon_size =
        icon_view->GetImageView()->GetPreferredSize().height();
    if (icon_size == 0)
      return gfx::Insets();

    const int height =
        toolbar_button_provider_->GetToolbarButtonSize().height();
    const int inset_size = std::max(0, (height - icon_size) / 2);
    return gfx::Insets(inset_size);
  }

  // Methods for coordinate the titlebar animation (origin text slide, menu
  // highlight and icon fade in).
  bool ShouldAnimate() const {
    return !g_animation_disabled_for_testing &&
           !browser_view_->immersive_mode_controller()->IsEnabled();
  }

  void StartTitlebarAnimation() {
    if (!ShouldAnimate())
      return;

    if (web_app_origin_text_)
      web_app_origin_text_->StartFadeAnimation();
    if (web_app_menu_button_)
      web_app_menu_button_->StartHighlightAnimation();
    icon_fade_in_delay_.Start(FROM_HERE, OriginTotalDuration(), this,
                              &WebAppFrameToolbarView::ToolbarButtonContainer::
                                  FadeInContentSettingIcons);
  }

  void FadeInContentSettingIcons() {
    if (content_settings_container_)
      content_settings_container_->FadeIn();
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // BrowserActionsContainer::Delegate:
  views::LabelButton* GetOverflowReferenceView() override {
    return web_app_menu_button_;
  }
  base::Optional<int> GetMaxBrowserActionsWidth() const override {
    // Our maximum size is 1 icon so don't specify a pixel-width max here.
    return base::Optional<int>();
  }
  bool CanShowIconInToolbar() const override { return false; }
  std::unique_ptr<ToolbarActionsBar> CreateToolbarActionsBar(
      ToolbarActionsBarDelegate* delegate,
      Browser* browser,
      ToolbarActionsBar* main_bar) const override {
    DCHECK_EQ(browser_view_->browser(), browser);
    return std::make_unique<WebAppToolbarActionsBar>(delegate, browser,
                                                     main_bar);
  }

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override {
    return foreground_color_;
  }
  SkColor GetIconLabelBubbleBackgroundColor() const override {
    return background_color_;
  }

  // ContentSettingImageView::Delegate:
  bool ShouldHideContentSettingImage() override { return false; }
  content::WebContents* GetContentSettingWebContents() override {
    return browser_view_->GetActiveWebContents();
  }
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override {
    return browser_view_->browser()->content_setting_bubble_model_delegate();
  }
  void OnContentSettingImageBubbleShown(
      ContentSettingImageModel::ImageType type) const override {
    UMA_HISTOGRAM_ENUMERATION(
        "HostedAppFrame.ContentSettings.ImagePressed", type,
        ContentSettingImageModel::ImageType::NUM_IMAGE_TYPES);
  }

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override {
    // Don't wait for the fade in animation to make content setting icons
    // visible once in immersive mode.
    if (content_settings_container_)
      content_settings_container_->EnsureVisible();
  }

  // PageActionIconView::Delegate:
  content::WebContents* GetWebContentsForPageActionIconView() override {
    return browser_view_->GetActiveWebContents();
  }

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // Whether we're waiting for the widget to become visible.
  bool pending_widget_visibility_ = true;

  ScopedObserver<views::Widget, views::WidgetObserver> scoped_widget_observer_{
      this};

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
  ContentSettingsContainer* content_settings_container_ = nullptr;
  BrowserActionsContainer* browser_actions_container_ = nullptr;
  ExtensionsToolbarContainer* extensions_container_ = nullptr;
  WebAppMenuButton* web_app_menu_button_ = nullptr;
};

WebAppFrameToolbarView::ToolbarButtonContainer::ToolbarButtonContainer(
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
    content_settings_container_ =
        AddChildView(std::make_unique<ContentSettingsContainer>(this, this));
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
    if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
      extensions_container_ =
          AddChildView(std::make_unique<ExtensionsToolbarContainer>(
              browser_view_->browser(),
              ExtensionsToolbarContainer::DisplayMode::kCompact));
      extensions_container_->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(
              extensions_container_->animating_layout_manager()
                  ->GetDefaultFlexRule())
              .WithOrder(kLowPriorityFlexOrder));
      views::SetHitTestComponent(extensions_container_,
                                 static_cast<int>(HTCLIENT));
    } else {
      browser_actions_container_ =
          AddChildView(std::make_unique<BrowserActionsContainer>(
              browser_view_->browser(), nullptr, this,
              false /* interactive */));
      browser_actions_container_->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(browser_actions_container_->GetFlexRule())
              .WithOrder(kLowPriorityFlexOrder));
      views::SetHitTestComponent(browser_actions_container_,
                                 static_cast<int>(HTCLIENT));
    }
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
  scoped_widget_observer_.Add(widget);
}

WebAppFrameToolbarView::ToolbarButtonContainer::~ToolbarButtonContainer() {
  ImmersiveModeController* immersive_controller =
      browser_view_->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);
}

void WebAppFrameToolbarView::ToolbarButtonContainer::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  if (!visible || !pending_widget_visibility_)
    return;
  pending_widget_visibility_ = false;
  if (ShouldAnimate()) {
    if (content_settings_container_)
      content_settings_container_->SetUpForFadeIn();
    animation_start_delay_.Start(
        FROM_HERE, kTitlebarAnimationDelay, this,
        &WebAppFrameToolbarView::ToolbarButtonContainer::
            StartTitlebarAnimation);
  }
}

WebAppFrameToolbarView::WebAppFrameToolbarView(views::Widget* widget,
                                               BrowserView* browser_view)
    : browser_view_(browser_view) {
  DCHECK(browser_view_);
  DCHECK(web_app::AppBrowserController::IsForWebAppBrowser(
      browser_view_->browser()));
  SetID(VIEW_ID_WEB_APP_FRAME_TOOLBAR);

  {
    // TODO(tluk) fix the need for both LayoutInContainer() and a layout
    // manager for frame layout.
    views::FlexLayout* layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  }

  const auto* app_controller = browser_view_->browser()->app_controller();

  if (app_controller->HasMinimalUiButtons()) {
    left_container_ = AddChildView(
        std::make_unique<NavigationButtonContainer>(browser_view_));
    left_container_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            views::LayoutOrientation::kHorizontal,
            views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero)
            .WithOrder(2));
  }

  center_container_ = AddChildView(std::make_unique<views::View>());
  center_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(3));

  right_container_ = AddChildView(
      std::make_unique<ToolbarButtonContainer>(widget, browser_view, this));
  right_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(right_container_->GetFlexRule()).WithOrder(1));

  UpdateStatusIconsVisibility();

  DCHECK(!browser_view_->toolbar_button_provider() ||
         browser_view_->toolbar_button_provider()
                 ->GetAsAccessiblePaneView()
                 ->GetClassName() == GetClassName())
      << "This should be the first ToolbarButtorProvider or a replacement for "
         "an existing instance of this class during a window frame refresh.";
  browser_view_->SetToolbarButtonProvider(this);
}

WebAppFrameToolbarView::~WebAppFrameToolbarView() = default;

void WebAppFrameToolbarView::UpdateStatusIconsVisibility() {
  right_container_->UpdateStatusIconsVisibility();
}

void WebAppFrameToolbarView::UpdateCaptionColors() {
  const BrowserNonClientFrameView* frame_view =
      browser_view_->frame()->GetFrameView();
  DCHECK(frame_view);

  active_background_color_ =
      frame_view->GetFrameColor(BrowserFrameActiveState::kActive);
  active_foreground_color_ =
      frame_view->GetCaptionColor(BrowserFrameActiveState::kActive);
  inactive_background_color_ =
      frame_view->GetFrameColor(BrowserFrameActiveState::kInactive);
  inactive_foreground_color_ =
      frame_view->GetCaptionColor(BrowserFrameActiveState::kInactive);
  UpdateChildrenColor();
}

void WebAppFrameToolbarView::SetPaintAsActive(bool active) {
  if (paint_as_active_ == active)
    return;
  paint_as_active_ = active;
  UpdateChildrenColor();
}

std::pair<int, int> WebAppFrameToolbarView::LayoutInContainer(
    int leading_x,
    int trailing_x,
    int y,
    int available_height) {
  SetVisible(available_height > 0);

  if (available_height == 0) {
    SetSize(gfx::Size());
    return std::pair<int, int>(0, 0);
  }

  gfx::Size preferred_size = GetPreferredSize();
  const int width = std::max(trailing_x - leading_x, 0);
  const int height = preferred_size.height();
  DCHECK_LE(height, available_height);
  SetBounds(leading_x, y + (available_height - height) / 2, width, height);
  Layout();

  if (!center_container_->GetVisible())
    return std::pair<int, int>(0, 0);

  // Bounds for remaining inner space, in parent container coordinates.
  gfx::Rect center_bounds = center_container_->bounds();
  DCHECK(center_bounds.x() == 0 || left_container_);
  center_bounds.Offset(bounds().OffsetFromOrigin());

  return std::pair<int, int>(center_bounds.x(), center_bounds.right());
}

BrowserActionsContainer* WebAppFrameToolbarView::GetBrowserActionsContainer() {
  CHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  return right_container_->browser_actions_container();
}

ExtensionsToolbarContainer*
WebAppFrameToolbarView::GetExtensionsToolbarContainer() {
  return right_container_->extensions_container();
}

gfx::Size WebAppFrameToolbarView::GetToolbarButtonSize() const {
  constexpr int kFocusModeButtonSize = 34;
  int size = browser_view_->browser()->is_focus_mode()
                 ? kFocusModeButtonSize
                 : GetLayoutConstant(WEB_APP_MENU_BUTTON_SIZE);
  return gfx::Size(size, size);
}

views::View* WebAppFrameToolbarView::GetDefaultExtensionDialogAnchorView() {
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu))
    return right_container_->extensions_container()->extensions_button();
  return GetAppMenuButton();
}

PageActionIconView* WebAppFrameToolbarView::GetPageActionIconView(
    PageActionIconType type) {
  return right_container_->page_action_icon_controller()->GetIconView(type);
}

AppMenuButton* WebAppFrameToolbarView::GetAppMenuButton() {
  return right_container_->web_app_menu_button();
}

gfx::Rect WebAppFrameToolbarView::GetFindBarBoundingBox(int contents_bottom) {
  if (!IsDrawn())
    return gfx::Rect();

  // If LTR find bar will be right aligned so align to right edge of app menu
  // button. Otherwise it will be left aligned so align to the left edge of the
  // app menu button.
  views::View* anchor_view = GetAnchorView(PageActionIconType::kFind);
  gfx::Rect anchor_bounds =
      anchor_view->ConvertRectToWidget(anchor_view->GetLocalBounds());
  int x_pos = 0;
  int width = anchor_bounds.right();
  if (base::i18n::IsRTL()) {
    x_pos = anchor_bounds.x();
    width = GetWidget()->GetRootView()->width() - anchor_bounds.x();
  }
  return gfx::Rect(x_pos, anchor_bounds.bottom(), width,
                   contents_bottom - anchor_bounds.bottom());
}

void WebAppFrameToolbarView::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* WebAppFrameToolbarView::GetAsAccessiblePaneView() {
  return this;
}

views::View* WebAppFrameToolbarView::GetAnchorView(PageActionIconType type) {
  views::View* anchor = GetAppMenuButton();
  return anchor ? anchor : this;
}

void WebAppFrameToolbarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  right_container_->page_action_icon_controller()->ZoomChangedForActiveTab(
      can_show_bubble);
}

AvatarToolbarButton* WebAppFrameToolbarView::GetAvatarToolbarButton() {
  return nullptr;
}

ToolbarButton* WebAppFrameToolbarView::GetBackButton() {
  return left_container_ ? left_container_->back_button() : nullptr;
}

ReloadButton* WebAppFrameToolbarView::GetReloadButton() {
  return left_container_ ? left_container_->reload_button() : nullptr;
}

void WebAppFrameToolbarView::DisableAnimationForTesting() {
  g_animation_disabled_for_testing = true;
}

views::View* WebAppFrameToolbarView::GetLeftContainerForTesting() {
  return left_container_;
}

views::View* WebAppFrameToolbarView::GetRightContainerForTesting() {
  return right_container_;
}

PageActionIconController*
WebAppFrameToolbarView::GetPageActionIconControllerForTesting() {
  return right_container_->page_action_icon_controller();
}

const char* WebAppFrameToolbarView::GetClassName() const {
  return kViewClassName;
}

void WebAppFrameToolbarView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void WebAppFrameToolbarView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  UpdateCaptionColors();
}

views::View* WebAppFrameToolbarView::GetContentSettingContainerForTesting() {
  return right_container_->content_settings_container();
}

const std::vector<ContentSettingImageView*>&
WebAppFrameToolbarView::GetContentSettingViewsForTesting() const {
  return right_container_->content_settings_container()
      ->get_content_setting_views();
}

void WebAppFrameToolbarView::UpdateChildrenColor() {
  const SkColor foreground_color =
      paint_as_active_ ? active_foreground_color_ : inactive_foreground_color_;
  if (left_container_)
    left_container_->SetIconColor(foreground_color);
  right_container_->SetColors(
      foreground_color,
      paint_as_active_ ? active_background_color_ : inactive_background_color_);
}
