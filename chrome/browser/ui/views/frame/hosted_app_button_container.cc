// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"

#include "base/metrics/histogram_macros.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_menu_button.h"
#include "chrome/browser/ui/views/frame/hosted_app_origin_text.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/window/hit_test_utils.h"

namespace {

bool g_animation_disabled_for_testing = false;

constexpr base::TimeDelta kContentSettingsFadeInDuration =
    base::TimeDelta::FromMilliseconds(500);

class HostedAppToolbarActionsBar : public ToolbarActionsBar {
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
    return popped_out_action() ? 1 : 0;
  }

  int GetMinimumWidth() const override {
    // Allow the BrowserActionsContainer to collapse completely and be hidden
    return 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HostedAppToolbarActionsBar);
};

int HorizontalPaddingBetweenItems() {
  return views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
}

}  // namespace

const char HostedAppButtonContainer::kViewClassName[] =
    "HostedAppButtonContainer";

constexpr base::TimeDelta HostedAppButtonContainer::kTitlebarAnimationDelay;
constexpr base::TimeDelta HostedAppButtonContainer::kOriginFadeInDuration;
constexpr base::TimeDelta HostedAppButtonContainer::kOriginPauseDuration;
constexpr base::TimeDelta HostedAppButtonContainer::kOriginFadeOutDuration;

// static
base::TimeDelta HostedAppButtonContainer::OriginTotalDuration() {
  // TimeDelta.operator+ uses time_internal::SaturatedAdd() which isn't
  // constexpr, so this needs to be a function to not introduce a static
  // initializer.
  return kOriginFadeInDuration + kOriginPauseDuration + kOriginFadeOutDuration;
}

class HostedAppButtonContainer::ContentSettingsContainer : public views::View {
 public:
  explicit ContentSettingsContainer(
      ContentSettingImageView::Delegate* delegate);
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
    if (visible())
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

  const std::vector<ContentSettingImageView*>&
  GetContentSettingViewsForTesting() const {
    return content_setting_views_;
  }

 private:
  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // Owned by the views hierarchy.
  std::vector<ContentSettingImageView*> content_setting_views_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsContainer);
};

HostedAppButtonContainer::ContentSettingsContainer::ContentSettingsContainer(
    ContentSettingImageView::Delegate* delegate) {
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(),
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), delegate,
        views::NativeWidgetAura::GetWindowTitleFontList());
    // Padding around content setting icons.
    constexpr int kContentSettingIconInteriorPadding = 4;
    image_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kContentSettingIconInteriorPadding)));
    image_view->disable_animation();
    views::SetHitTestComponent(image_view.get(), static_cast<int>(HTCLIENT));
    content_setting_views_.push_back(image_view.get());
    AddChildView(image_view.release());
  }
}

HostedAppButtonContainer::HostedAppButtonContainer(
    views::Widget* widget,
    BrowserView* browser_view,
    SkColor active_color,
    SkColor inactive_color,
    base::Optional<int> right_margin)
    : scoped_widget_observer_(this),
      browser_view_(browser_view),
      active_color_(active_color),
      inactive_color_(inactive_color),
      hosted_app_origin_text_(new HostedAppOriginText(browser_view->browser())),
      content_settings_container_(new ContentSettingsContainer(this)),
      page_action_icon_container_view_(new PageActionIconContainerView(
          {PageActionIconType::kFind, PageActionIconType::kZoom},
          GetLayoutConstant(HOSTED_APP_PAGE_ACTION_ICON_SIZE),
          HorizontalPaddingBetweenItems(),
          browser_view->browser(),
          this,
          nullptr)),
      browser_actions_container_(
          new BrowserActionsContainer(browser_view->browser(),
                                      nullptr,
                                      this,
                                      false /* interactive */)),
      app_menu_button_(new HostedAppMenuButton(browser_view)) {
  DCHECK(browser_view_);
  DCHECK(browser_view_->IsBrowserTypeHostedApp());
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal,
          gfx::Insets(0,
                      right_margin.value_or(HorizontalPaddingBetweenItems())),
          HorizontalPaddingBetweenItems()));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  layout.set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);

  AddChildView(hosted_app_origin_text_);

  views::SetHitTestComponent(content_settings_container_,
                             static_cast<int>(HTCLIENT));
  AddChildView(content_settings_container_);

  views::SetHitTestComponent(page_action_icon_container_view_,
                             static_cast<int>(HTCLIENT));
  AddChildView(page_action_icon_container_view_);

  views::SetHitTestComponent(browser_actions_container_,
                             static_cast<int>(HTCLIENT));
  AddChildView(browser_actions_container_);
  AddChildView(app_menu_button_);

  UpdateChildrenColor();

  browser_view_->SetToolbarButtonProvider(this);
  browser_view_->immersive_mode_controller()->AddObserver(this);
  scoped_widget_observer_.Add(widget);
}

HostedAppButtonContainer::~HostedAppButtonContainer() {
  ImmersiveModeController* immersive_controller =
      browser_view_->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);
}

void HostedAppButtonContainer::UpdateContentSettingViewsVisibility() {
  content_settings_container_->UpdateContentSettingViewsVisibility();
}

void HostedAppButtonContainer::SetPaintAsActive(bool active) {
  if (paint_as_active_ == active)
    return;
  paint_as_active_ = active;
  UpdateChildrenColor();
}

int HostedAppButtonContainer::LayoutInContainer(int leading_x,
                                                int trailing_x,
                                                int y,
                                                int available_height) {
  if (available_height == 0) {
    SetSize(gfx::Size());
    return trailing_x;
  }

  gfx::Size preferred_size = GetPreferredSize();
  const int width =
      std::min(preferred_size.width(), std::max(0, trailing_x - leading_x));
  const int height = preferred_size.height();
  DCHECK_LE(height, available_height);
  SetBounds(trailing_x - width, y + (available_height - height) / 2, width,
            height);
  Layout();
  return bounds().x();
}

const char* HostedAppButtonContainer::GetClassName() const {
  return kViewClassName;
}

views::MenuButton* HostedAppButtonContainer::GetOverflowReferenceView() {
  return app_menu_button_;
}

base::Optional<int> HostedAppButtonContainer::GetMaxBrowserActionsWidth()
    const {
  // Our maximum size is 1 icon so don't specify a pixel-width max here.
  return base::Optional<int>();
}

std::unique_ptr<ToolbarActionsBar>
HostedAppButtonContainer::CreateToolbarActionsBar(
    ToolbarActionsBarDelegate* delegate,
    Browser* browser,
    ToolbarActionsBar* main_bar) const {
  DCHECK_EQ(browser_view_->browser(), browser);
  return std::make_unique<HostedAppToolbarActionsBar>(delegate, browser,
                                                      main_bar);
}

SkColor HostedAppButtonContainer::GetContentSettingInkDropColor() const {
  return GetIconColor();
}

content::WebContents* HostedAppButtonContainer::GetContentSettingWebContents() {
  return browser_view_->GetActiveWebContents();
}

ContentSettingBubbleModelDelegate*
HostedAppButtonContainer::GetContentSettingBubbleModelDelegate() {
  return browser_view_->browser()->content_setting_bubble_model_delegate();
}

void HostedAppButtonContainer::OnContentSettingImageBubbleShown(
    ContentSettingImageModel::ImageType type) const {
  UMA_HISTOGRAM_ENUMERATION(
      "HostedAppFrame.ContentSettings.ImagePressed", type,
      ContentSettingImageModel::ImageType::NUM_IMAGE_TYPES);
}

void HostedAppButtonContainer::OnImmersiveRevealStarted() {
  // Don't wait for the fade in animation to make content setting icons visible
  // once in immersive mode.
  content_settings_container_->EnsureVisible();
}

SkColor HostedAppButtonContainer::GetPageActionInkDropColor() const {
  return GetIconColor();
}

content::WebContents*
HostedAppButtonContainer::GetWebContentsForPageActionIconView() {
  return browser_view_->GetActiveWebContents();
}

BrowserActionsContainer*
HostedAppButtonContainer::GetBrowserActionsContainer() {
  return browser_actions_container_;
}

PageActionIconContainerView*
HostedAppButtonContainer::GetPageActionIconContainerView() {
  return page_action_icon_container_view_;
}

AppMenuButton* HostedAppButtonContainer::GetAppMenuButton() {
  return app_menu_button_;
}

gfx::Rect HostedAppButtonContainer::GetFindBarBoundingBox(
    int contents_height) const {
  if (!IsDrawn())
    return gfx::Rect();

  gfx::Rect anchor_bounds =
      app_menu_button_->ConvertRectToWidget(app_menu_button_->GetLocalBounds());
  if (base::i18n::IsRTL()) {
    // Find bar will be left aligned so align to left edge of app menu button.
    int widget_width = GetWidget()->GetRootView()->width();
    return gfx::Rect(anchor_bounds.x(), anchor_bounds.bottom(),
                     widget_width - anchor_bounds.x(), contents_height);
  }
  // Find bar will be right aligned so align to right edge of app menu button.
  return gfx::Rect(0, anchor_bounds.bottom(),
                   anchor_bounds.x() + anchor_bounds.width(), contents_height);
}

void HostedAppButtonContainer::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* HostedAppButtonContainer::GetAsAccessiblePaneView() {
  return this;
}

void HostedAppButtonContainer::OnWidgetVisibilityChanged(views::Widget* widget,
                                                         bool visibility) {
  if (!visibility || !pending_widget_visibility_)
    return;
  pending_widget_visibility_ = false;
  if (ShouldAnimate()) {
    content_settings_container_->SetUpForFadeIn();
    animation_start_delay_.Start(
        FROM_HERE, kTitlebarAnimationDelay, this,
        &HostedAppButtonContainer::StartTitlebarAnimation);
  }
}

gfx::Size HostedAppButtonContainer::CalculatePreferredSize() const {
  // Prefer height consistency over accommodating edge case icons that may bump
  // up the container height (e.g. extension action icons with badges).
  // TODO(https://crbug.com/889745): Fix the inconsistent icon sizes found in
  // this container and turn this into a DCHECK that the conatiner height is the
  // same as the app menu button height.
  return gfx::Size(views::View::CalculatePreferredSize().width(),
                   app_menu_button_->GetPreferredSize().height());
}

void HostedAppButtonContainer::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void HostedAppButtonContainer::ChildVisibilityChanged(views::View* child) {
  // Changes to layout need to be taken into account by the frame view.
  PreferredSizeChanged();
}

bool HostedAppButtonContainer::ShouldAnimate() const {
  return !g_animation_disabled_for_testing &&
         !browser_view_->immersive_mode_controller()->IsEnabled();
}

void HostedAppButtonContainer::StartTitlebarAnimation() {
  if (!ShouldAnimate())
    return;

  hosted_app_origin_text_->StartFadeAnimation();
  app_menu_button_->StartHighlightAnimation();
  icon_fade_in_delay_.Start(
      FROM_HERE, OriginTotalDuration(), this,
      &HostedAppButtonContainer::FadeInContentSettingIcons);
}

void HostedAppButtonContainer::FadeInContentSettingIcons() {
  content_settings_container_->FadeIn();
}

void HostedAppButtonContainer::DisableAnimationForTesting() {
  g_animation_disabled_for_testing = true;
}

views::View* HostedAppButtonContainer::GetContentSettingContainerForTesting() {
  return content_settings_container_;
}

const std::vector<ContentSettingImageView*>&
HostedAppButtonContainer::GetContentSettingViewsForTesting() const {
  return content_settings_container_->GetContentSettingViewsForTesting();
}

SkColor HostedAppButtonContainer::GetIconColor() const {
  return paint_as_active_ ? active_color_ : inactive_color_;
}

void HostedAppButtonContainer::UpdateChildrenColor() {
  SkColor icon_color = GetIconColor();
  hosted_app_origin_text_->SetTextColor(icon_color);
  content_settings_container_->SetIconColor(icon_color);
  page_action_icon_container_view_->SetIconColor(icon_color);
  app_menu_button_->SetColor(icon_color);
}
