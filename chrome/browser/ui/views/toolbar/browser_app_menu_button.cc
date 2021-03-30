// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"

#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
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
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
bool BrowserAppMenuButton::g_open_app_immediately_for_testing = false;

BrowserAppMenuButton::BrowserAppMenuButton(PressedCallback callback,
                                           ToolbarView* toolbar_view)
    : AppMenuButton(std::move(callback)), toolbar_view_(toolbar_view) {
  SetInkDropMode(InkDropMode::ON);
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  SetInkDropVisibleOpacity(kToolbarInkDropVisibleOpacity);
}

BrowserAppMenuButton::~BrowserAppMenuButton() {}

void BrowserAppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  type_and_severity_ = type_and_severity;

  UpdateIcon();
  UpdateTextAndHighlightColor();
}

void BrowserAppMenuButton::ShowMenu(int run_types) {
  if (IsMenuShowing())
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_visible())
    keyboard_client->HideKeyboard(ash::HideReason::kSystem);
#endif

  Browser* browser = toolbar_view_->browser();

  FeaturePromoControllerViews* const feature_promo_controller =
      BrowserView::GetBrowserViewForBrowser(toolbar_view_->browser())
          ->feature_promo_controller();

  // If the menu was opened while reopen tab in-product help was
  // showing, we continue the IPH into the menu. Notify the promo
  // controller we are taking control of the promo.
  DCHECK(!reopen_tab_promo_handle_);
  if (feature_promo_controller->BubbleIsShowing(
          feature_engagement::kIPHReopenTabFeature)) {
    reopen_tab_promo_handle_ =
        feature_promo_controller->CloseBubbleAndContinuePromo(
            feature_engagement::kIPHReopenTabFeature);
  }

  bool alert_reopen_tab_items = reopen_tab_promo_handle_.has_value();

  RunMenu(
      std::make_unique<AppMenuModel>(toolbar_view_, browser,
                                     toolbar_view_->app_menu_icon_controller()),
      browser, run_types, alert_reopen_tab_items);
}

void BrowserAppMenuButton::OnThemeChanged() {
  UpdateTextAndHighlightColor();
  AppMenuButton::OnThemeChanged();
}

void BrowserAppMenuButton::UpdateIcon() {
  const gfx::VectorIcon& icon = ui::TouchUiController::Get()->touch_ui()
                                    ? kBrowserToolsTouchIcon
                                    : kBrowserToolsIcon;
  for (auto state : kButtonStates) {
    SkColor icon_color =
        toolbar_view_->app_menu_icon_controller()->GetIconColor(
            GetForegroundColor(state));
    SetImageModel(state, ui::ImageModel::FromVectorIcon(icon, icon_color));
  }
}

void BrowserAppMenuButton::HandleMenuClosed() {
  // If we were showing a promo in the menu, drop the handle to notify
  // FeaturePromoController we're done. This is a no-op if we weren't
  // showing the promo.
  reopen_tab_promo_handle_.reset();
}

void BrowserAppMenuButton::UpdateTextAndHighlightColor() {
  int tooltip_message_id;
  std::u16string text;
  if (type_and_severity_.severity == AppMenuIconController::Severity::NONE) {
    tooltip_message_id = IDS_APPMENU_TOOLTIP;
  } else if (type_and_severity_.type ==
             AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    tooltip_message_id = IDS_APPMENU_TOOLTIP_UPDATE_AVAILABLE;
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_UPDATE);
  } else {
    tooltip_message_id = IDS_APPMENU_TOOLTIP_ALERT;
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_ERROR);
  }

  base::Optional<SkColor> color;
  switch (type_and_severity_.severity) {
    case AppMenuIconController::Severity::NONE:
      break;
    case AppMenuIconController::Severity::LOW:
      color = AdjustHighlightColorForContrast(
          GetThemeProvider(), gfx::kGoogleGreen300, gfx::kGoogleGreen600,
          gfx::kGoogleGreen050, gfx::kGoogleGreen900);

      break;
    case AppMenuIconController::Severity::MEDIUM:
      color = AdjustHighlightColorForContrast(
          GetThemeProvider(), gfx::kGoogleYellow300, gfx::kGoogleYellow600,
          gfx::kGoogleYellow050, gfx::kGoogleYellow900);

      break;
    case AppMenuIconController::Severity::HIGH:
      color = AdjustHighlightColorForContrast(
          GetThemeProvider(), gfx::kGoogleRed300, gfx::kGoogleRed600,
          gfx::kGoogleRed050, gfx::kGoogleRed900);

      break;
  }

  SetTooltipText(l10n_util::GetStringUTF16(tooltip_message_id));
  SetHighlight(text, color);
}

std::unique_ptr<views::InkDropHighlight>
BrowserAppMenuButton::CreateInkDropHighlight() const {
  return CreateToolbarInkDropHighlight(this);
}

void BrowserAppMenuButton::OnTouchUiChanged() {
  UpdateColorsAndInsets();
  PreferredSizeChanged();
}

BEGIN_METADATA(BrowserAppMenuButton, AppMenuButton)
END_METADATA
