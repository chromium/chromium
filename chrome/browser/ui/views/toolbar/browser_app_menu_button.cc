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
#include "cc/paint/paint_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/browser_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/throb_animation.h"
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
constexpr int kChromeRefreshImageLabelPadding = 2;
constexpr int kGlowUpImageLabelPadding = 4;
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(300);

class LottieIconSource : public gfx::CanvasImageSource {
 public:
  LottieIconSource(lottie::Animation* animation,
                   float progress,
                   int size,
                   SkColor color)
      : gfx::CanvasImageSource(gfx::Size(size, size)),
        animation_(animation),
        progress_(progress),
        color_(color) {}
  LottieIconSource(const LottieIconSource&) = delete;
  LottieIconSource& operator=(const LottieIconSource&) = delete;
  ~LottieIconSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColorFilter(cc::ColorFilter::MakeBlend(
        SkColor4f::FromColor(color_), SkBlendMode::kSrcIn));
    canvas->SaveLayerWithFlags(flags);
    animation_->PaintFrame(canvas, progress_, size());
    canvas->Restore();
  }

 private:
  const raw_ptr<lottie::Animation> animation_;
  const float progress_;
  const SkColor color_;
};

}  // namespace

// static
bool BrowserAppMenuButton::g_open_app_immediately_for_testing = false;

BrowserAppMenuButton::BrowserAppMenuButton(ToolbarView* toolbar_view)
    : AppMenuButton(base::BindRepeating(&BrowserAppMenuButton::ButtonPressed,
                                        base::Unretained(this))),
      toolbar_view_(toolbar_view) {
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  SetImageLabelSpacing(base::FeatureList::IsEnabled(features::kToolbarGlowUp)
                           ? kGlowUpImageLabelPadding
                           : kChromeRefreshImageLabelPadding);
  label()->SetPaintToLayer();
  label()->SetSkipSubpixelRenderingOpacityCheck(true);
  label()->layer()->SetFillsBoundsOpaquely(false);
  label()->SetSubpixelRenderingEnabled(false);
  if (base::FeatureList::IsEnabled(features::kToolbarGlowUp)) {
    SetAnimateOnStateChange(true);
    SetAnimationDuration(kAnimationDuration);
    click_animation_ = std::make_unique<gfx::ThrobAnimation>(this);
    click_animation_->SetSlideDuration(kAnimationDuration);
  }
}

BrowserAppMenuButton::~BrowserAppMenuButton() = default;

void BrowserAppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  type_and_severity_ = type_and_severity;
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

  // Allow highlighting menu items when the menu was opened while
  // certain tutorials are running.
  AlertMenuItem alert_item = GetAlertItemForRunningTutorial();

  RunMenu(std::make_unique<AppMenuModel>(
              toolbar_view_, browser, toolbar_view_->app_menu_icon_controller(),
              alert_item),
          browser, run_types);
}

AlertMenuItem BrowserAppMenuButton::GetAlertItemForRunningTutorial() {
  Browser* browser = toolbar_view_->browser();
  BrowserWindow* browser_window = browser->window();

  if (browser_window == nullptr) {
    return AlertMenuItem::kNone;
  }

  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(browser->profile());
  if (service && service->tutorial_service().IsRunningTutorial(
                     kPasswordManagerTutorialId)) {
    return AlertMenuItem::kPasswordManager;
  }

  return AlertMenuItem::kNone;
}

void BrowserAppMenuButton::OnMenuClosed() {
  if (base::FeatureList::IsEnabled(features::kToolbarGlowUp)) {
    click_animation_->Hide();
  }
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
  const gfx::VectorIcon& icon = ui::TouchUiController::Get()->touch_ui()
                                    ? kBrowserToolsTouchIcon
                                    : kBrowserToolsChromeRefreshIcon;

  const double click_animation_value =
      base::FeatureList::IsEnabled(features::kToolbarGlowUp)
          ? click_animation_->GetCurrentValue()
          : 0;
  const int icon_size = GetIconSize();

  for (auto state : kButtonStates) {
    SkColor icon_color = GetForegroundColor(state);
    ui::ImageModel model =
        ui::ImageModel::FromVectorIcon(icon, icon_color, icon_size);

    if (base::FeatureList::IsEnabled(features::kToolbarGlowUp) &&
        click_animation_value > 0 && GetColorProvider()) {
      if (!lottie_animation_) {
        std::optional<std::vector<uint8_t>> lottie_bytes =
            ui::ResourceBundle::GetSharedInstance().GetLottieData(
                IDR_APP_MENU_BUTTON_HOVER_LOTTIE);
        CHECK(lottie_bytes);
        scoped_refptr<cc::SkottieWrapper> skottie =
            cc::SkottieWrapper::UnsafeCreateSerializable(
                std::move(*lottie_bytes));
        lottie_animation_ = std::make_unique<lottie::Animation>(skottie);
      }

      model = ui::ImageModel::FromImageSkia(
          gfx::CanvasImageSource::MakeImageSkia<LottieIconSource>(
              lottie_animation_.get(), click_animation_value, icon_size,
              icon_color));
    }
    SetImageModel(state, model);
  }
}

void BrowserAppMenuButton::AnimationProgressed(
    const gfx::Animation* animation) {
  if (base::FeatureList::IsEnabled(features::kToolbarGlowUp) &&
      animation == click_animation_.get()) {
    UpdateIcon();
  }
  AppMenuButton::AnimationProgressed(animation);
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
  if (!label()) {
    return false;
  }
  return label()->GetVisible() && !label()->GetText().empty();
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
  } else {
    SetLayoutInsets(::GetLayoutInsets(TOOLBAR_BUTTON));
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

  if (base::FeatureList::IsEnabled(features::kToolbarGlowUp)) {
    click_animation_->Show();
  }

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

BEGIN_METADATA(BrowserAppMenuButton)
END_METADATA
