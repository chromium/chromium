// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"

#include <set>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/action_app_menu.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/browser_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr int kHideTextForFlexPadding = 4;
}  // namespace

// static
bool BrowserAppMenuButton::g_open_app_immediately_for_testing = false;

BrowserAppMenuButton::BrowserAppMenuButton(ToolbarView* toolbar_view)
    : AppMenuButton(base::BindRepeating(&BrowserAppMenuButton::ButtonPressed,
                                        base::Unretained(this))),
      toolbar_view_(toolbar_view) {
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  SetImageLabelSpacing(
      GetLayoutConstant(LayoutConstant::kAppMenuButtonImageLabelPadding));
  label()->SetPaintToLayer();
  label()->SetSkipSubpixelRenderingOpacityCheck(true);
  label()->layer()->SetFillsBoundsOpaquely(false);
  label()->SetSubpixelRenderingEnabled(false);
}

BrowserAppMenuButton::~BrowserAppMenuButton() = default;

void BrowserAppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  type_and_severity_ = type_and_severity;
  GetViewAccessibility().SetName(
      AppMenuIconController::GetIconAccessibleName(type_and_severity_.type));
  UpdateThemeBasedState();
}

void BrowserAppMenuButton::ShowMenu(int run_types) {
  if (IsMenuShowing()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (auto* input_method = GetInputMethod()) {
    if (auto* controller = input_method->GetVirtualKeyboardController();
        controller && controller->IsKeyboardVisible()) {
      input_method->SetVirtualKeyboardVisibilityIfEnabled(false);
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Browser* browser = toolbar_view_->browser();

  if (base::FeatureList::IsEnabled(features::kAppMenuGlowUp)) {
    RunActionMenu(browser, run_types);
    return;
  }

  // Allow highlighting menu items when the menu was opened while
  // certain tutorials are running.
  AlertMenuItem alert_item =
      AppMenuModel::GetAlertItemForRunningTutorial(browser);

  RunMenu(std::make_unique<AppMenuModel>(
              toolbar_view_, browser, toolbar_view_->app_menu_icon_controller(),
              alert_item),
          browser, run_types);
}

void BrowserAppMenuButton::OnMenuClosed() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (features::IsToolbarGlowUpEnabled()) {
    views::SingleAnimatedImageContainer::AnimationConfig config{
        .boundary =
            views::SingleAnimatedImageContainer::AnimationBoundary{
                .start_offset = 0.5f, .end_offset = 0.75f},
        .tween = gfx::Tween::FAST_OUT_SLOW_IN_3,
        .duration = base::Milliseconds(250)};

    animated_image_container().PlayAnimation(
        {IDR_APP_MENU_LOTTIE, GetForegroundColor(GetState()),
         views::SingleAnimatedImageContainer::AnimationDirection::kForward,
         views::SingleAnimatedImageContainer::AnimationEndBehavior::kReset},
        config);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AppMenuButton::OnMenuClosed();
}

void BrowserAppMenuButton::OnThemeChanged() {
  UpdateThemeBasedState();
  AppMenuButton::OnThemeChanged();
}

void BrowserAppMenuButton::UpdateThemeBasedState() {
  UpdateLayoutInsets();
  UpdateTextAndHighlightColor();
  // Call `UpdateIcon()` after `UpdateTextAndHighlightColor()` as the icon color
  // depends on if the container is in an expanded state.
  UpdateIcon();
  UpdateInkdrop();
  // Outset focus ring should be present for the chip but not when only
  // the icon is visible.
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(
      !IsLabelPresentAndVisible());
}

void BrowserAppMenuButton::UpdateIcon() {
  const gfx::VectorIcon& icon =
      ui::TouchUiController::Get()->touch_ui()
          ? features::IsRoundedIconsEnabled() ? kMoreVertIcon
                                              : kBrowserToolsTouchOldIcon
      : features::IsRoundedIconsEnabled() ? kMoreVertIcon
                                          : kBrowserToolsChromeRefreshOldIcon;
  const int icon_size = GetIconSize();

  for (auto state : kButtonStates) {
    SkColor icon_color = GetForegroundColor(state);
    ui::ImageModel model =
        ui::ImageModel::FromVectorIcon(icon, icon_color, icon_size);
    SetImageModel(state, model);
  }
}

void BrowserAppMenuButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ToolbarButton::OnBoundsChanged(previous_bounds);
  UpdateLayoutInsets();
}

void BrowserAppMenuButton::UpdateInkdrop() {
  if (IsLabelPresentAndVisible()) {
    ConfigureToolbarInkdropForRefresh2023(this, kColorAppMenuChipInkDropHover,
                                          kColorAppMenuChipInkDropRipple);
  } else {
    ConfigureToolbarInkdropForRefresh2023(this, kColorToolbarInkDropHover,
                                          kColorToolbarInkDropRipple);
  }
}

bool BrowserAppMenuButton::IsLabelPresentAndVisible() const {
  if (!label() || !label()->GetVisible() || label()->GetText().empty()) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(features::kToolbarAppMenuLabelResizing)) {
    return true;
  }
  // If the chip is narrow enough that text doesn't fit, return false. The min
  // width is the height of the button but add padding because at slightly
  // larger widths, text isn't visible due to eliding and this simplifies
  // ToolbarView layout.
  const int icon_width = GetTargetSize().height() + kHideTextForFlexPadding;
  return GetLocalBounds().width() > icon_width;
}

SkColor BrowserAppMenuButton::GetForegroundColor(ButtonState state) const {
  if (IsLabelPresentAndVisible()) {
    const auto* const color_provider = GetColorProvider();
    if (type_and_severity_.use_primary_colors) {
      return color_provider->GetColor(kColorAppMenuExpandedForegroundPrimary);
    }
    return color_provider->GetColor(kColorAppMenuExpandedForegroundDefault);
  }

  return ToolbarButton::GetForegroundColor(state);
}

void BrowserAppMenuButton::UpdateTextAndHighlightColor() {
  const std::u16string text = AppMenuIconController::GetIconLabel(
      type_and_severity_.type, type_and_severity_.severity);
  SetTooltipText(AppMenuIconController::GetIconTooltip(
      type_and_severity_.type, type_and_severity_.severity));
  SetHighlight(text, GetHighlightColor());
}

bool BrowserAppMenuButton::ShouldPaintBorder() const {
  return false;
}

void BrowserAppMenuButton::UpdateLayoutInsets() {
  if (IsLabelPresentAndVisible()) {
    SetLayoutInsets(::GetLayoutInsets(BROWSER_APP_MENU_CHIP_PADDING));
    SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  } else {
    SetLayoutInsets(::GetLayoutInsets(TOOLBAR_BUTTON));
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }
}

std::optional<SkColor> BrowserAppMenuButton::GetHighlightTextColor() const {
  if (IsLabelPresentAndVisible()) {
    const auto* const color_provider = GetColorProvider();
    if (type_and_severity_.use_primary_colors) {
      return color_provider->GetColor(kColorAppMenuExpandedForegroundPrimary);
    }
    return color_provider->GetColor(kColorAppMenuExpandedForegroundDefault);
  }
  return std::nullopt;
}

std::optional<SkColor> BrowserAppMenuButton::GetHighlightColor() const {
  const auto* const color_provider = GetColorProvider();
  if (type_and_severity_.severity == AppMenuIconController::Severity::kNone) {
    return std::nullopt;
  } else {
    return color_provider->GetColor(type_and_severity_.use_primary_colors
                                        ? kColorAppMenuHighlightPrimary
                                        : kColorAppMenuHighlightDefault);
  }
}

void BrowserAppMenuButton::OnTouchUiChanged() {
  UpdateColorsAndInsets();
  PreferredSizeChanged();
}

void BrowserAppMenuButton::ButtonPressed(const ui::Event& event) {
#if BUILDFLAG(IS_CHROMEOS)
  auto* const user_education =
      BrowserUserEducationInterface::From(toolbar_view_->browser());
  if (user_education->IsFeaturePromoActive(
          feature_engagement::kIPHPasswordsSavePrimingPromoFeature)) {
    user_education->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHPasswordsSavePrimingPromoFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (features::IsToolbarGlowUpEnabled() && !IsMenuShowing()) {
    views::SingleAnimatedImageContainer::AnimationConfig config{
        .boundary =
            views::SingleAnimatedImageContainer::AnimationBoundary{
                .start_offset = 0.0f, .end_offset = 0.25f},
        .tween = gfx::Tween::FAST_OUT_SLOW_IN_3,
        .duration = base::Milliseconds(250)};

    animated_image_container().PlayAnimation(
        {IDR_APP_MENU_LOTTIE, GetForegroundColor(GetState()),
         views::SingleAnimatedImageContainer::AnimationDirection::kForward,
         views::SingleAnimatedImageContainer::AnimationEndBehavior::kPause},
        config);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  ShowMenu(event.IsKeyEvent() ? (views::MenuRunner::SHOULD_SHOW_MNEMONICS |
                                 views::MenuRunner::INVOKED_FROM_KEYBOARD)
                              : views::MenuRunner::NO_FLAGS);
}

bool BrowserAppMenuButton::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kExpand) {
    ShowMenu(views::MenuRunner::NO_FLAGS);
    return true;
  }
  if (action_data.action == ax::mojom::Action::kCollapse) {
    if (AppMenuButton::IsMenuShowing()) {
      CloseMenu();
    }
    return true;
  }
  return AppMenuButton::HandleAccessibleAction(action_data);
}

gfx::Size BrowserAppMenuButton::GetMinimumSize() const {
  const int size = GetTargetSize().height();
  return gfx::Size(size, size);
}

BEGIN_METADATA(BrowserAppMenuButton)
END_METADATA
