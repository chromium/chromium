// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"

#include <set>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
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
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
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
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr int kChromeRefreshImageLabelPadding = 2;
}

// static
bool BrowserAppMenuButton::g_open_app_immediately_for_testing = false;

BrowserAppMenuButton::BrowserAppMenuButton(ToolbarView* toolbar_view)
    : AppMenuButton(base::BindRepeating(&BrowserAppMenuButton::ButtonPressed,
                                        base::Unretained(this))),
      toolbar_view_(toolbar_view) {
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  SetImageLabelSpacing(kChromeRefreshImageLabelPadding);
  label()->SetPaintToLayer();
  label()->SetSkipSubpixelRenderingOpacityCheck(true);
  label()->layer()->SetFillsBoundsOpaquely(false);
  label()->SetSubpixelRenderingEnabled(false);
}

BrowserAppMenuButton::~BrowserAppMenuButton() {}

void BrowserAppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  type_and_severity_ = type_and_severity;
  UpdateThemeBasedState();
}

void BrowserAppMenuButton::ShowMenu(int run_types) {
  if (IsMenuShowing())
    return;

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

  if (browser_window == nullptr)
    return AlertMenuItem::kNone;

  auto* const service =
      UserEducationServiceFactory::GetForBrowserContext(browser->profile());
  if (service && service->tutorial_service().IsRunningTutorial(
                     kPasswordManagerTutorialId)) {
    return AlertMenuItem::kPasswordManager;
  }

  return AlertMenuItem::kNone;
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
  for (auto state : kButtonStates) {
    SkColor icon_color = GetForegroundColor(state);
    SetImageModel(state, ui::ImageModel::FromVectorIcon(icon, icon_color));
  }
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
  int tooltip_message_id;
  std::u16string text;
  if (type_and_severity_.severity == AppMenuIconController::Severity::NONE) {
    tooltip_message_id = IDS_APPMENU_TOOLTIP;
  } else if (type_and_severity_.type ==
             AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    tooltip_message_id = IDS_APPMENU_TOOLTIP_UPDATE_AVAILABLE;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
    int message_id = IDS_APP_MENU_BUTTON_UPDATE;
    // Select an update text option randomly. Show this text in all browser
    // windows.
    static const int update_text_option = base::RandInt(1, 3);
    if (update_text_option == 1) {
      message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT1;
    } else if (update_text_option == 2) {
      message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT2;
    } else {
      message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT3;
    }
    text = l10n_util::GetStringUTF16(message_id);
#else
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_UPDATE);
#endif
  } else if (type_and_severity_.type ==
             AppMenuIconController::IconType::DEFAULT_BROWSER_PROMPT) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
    tooltip_message_id = IDS_APP_MENU_TOOLTIP_DEFAULT_PROMPT;
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_DEFAULT_PROMPT);
#else
    tooltip_message_id = IDS_APPMENU_TOOLTIP;
#endif
  } else {
    tooltip_message_id = IDS_APPMENU_TOOLTIP_ALERT;
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_ERROR);
  }

  SetTooltipText(l10n_util::GetStringUTF16(tooltip_message_id));
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
  if (type_and_severity_.severity == AppMenuIconController::Severity::NONE) {
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
  ShowMenu(event.IsKeyEvent() ? views::MenuRunner::SHOULD_SHOW_MNEMONICS
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
