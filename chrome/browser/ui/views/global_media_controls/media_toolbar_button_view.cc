// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help.h"
#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help_factory.h"
#include "chrome/browser/ui/views/feature_promos/global_media_controls_promo_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller.h"

MediaToolbarButtonView::MediaToolbarButtonView(const Browser* browser)
    : ToolbarButton(this),
      service_(
          MediaNotificationServiceFactory::GetForProfile(browser->profile())),
      browser_(browser) {
  GlobalMediaControlsInProductHelp* in_product_help =
      GlobalMediaControlsInProductHelpFactory::GetForProfile(
          browser_->profile());
  if (in_product_help)
    AddObserver(in_product_help);

  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  EnableCanvasFlippingForRTLUI(false);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_ICON_TOOLTIP_TEXT));
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kDialog);

  ToolbarButton::Init();

  // We start hidden and only show once |controller_| tells us to.
  SetVisible(false);

  // Wait until we're done with everything else before creating |controller_|
  // since it can call |Show()| synchronously.
  controller_ = std::make_unique<MediaToolbarButtonController>(this, service_);
}

MediaToolbarButtonView::~MediaToolbarButtonView() {
  // When |controller_| is destroyed, it may call
  // |MediaToolbarButtonView::Hide()|, so we want to be sure it's destroyed
  // before any of our other members.
  controller_.reset();
}

void MediaToolbarButtonView::AddObserver(MediaToolbarButtonObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaToolbarButtonView::RemoveObserver(
    MediaToolbarButtonObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaToolbarButtonView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (MediaDialogView::IsShowing()) {
    MediaDialogView::HideDialog();
  } else {
    MediaDialogView::ShowDialog(this, service_);

    // Inform observers. Since the promo controller cares about the dialog
    // showing, we need to ensure that it's created.
    EnsurePromoController();
    for (auto& observer : observers_)
      observer.OnMediaDialogOpened();
  }
}

void MediaToolbarButtonView::Show() {
  SetVisible(true);
  PreferredSizeChanged();

  for (auto& observer : observers_)
    observer.OnMediaButtonShown();
}

void MediaToolbarButtonView::Hide() {
  SetVisible(false);
  PreferredSizeChanged();

  // Inform observers. Since the promo controller cares about hiding, we need to
  // ensure that it's created.
  EnsurePromoController();
  for (auto& observer : observers_)
    observer.OnMediaButtonHidden();
}

void MediaToolbarButtonView::Enable() {
  SetEnabled(true);

#if defined(OS_MACOSX)
  UpdateIcon();
#endif  // defined(OS_MACOSX)

  for (auto& observer : observers_)
    observer.OnMediaButtonEnabled();
}

void MediaToolbarButtonView::Disable() {
  SetEnabled(false);

#if defined(OS_MACOSX)
  UpdateIcon();
#endif  // defined(OS_MACOSX)

  // Inform observers. Since the promo controller cares about disabling, we need
  // to ensure that it's created.
  EnsurePromoController();
  for (auto& observer : observers_)
    observer.OnMediaButtonDisabled();
}

SkColor MediaToolbarButtonView::GetInkDropBaseColor() const {
  return is_promo_showing_ ? GetNativeTheme()->GetSystemColor(
                                 ui::NativeTheme::kColorId_ProminentButtonColor)
                           : ToolbarButton::GetInkDropBaseColor();
}

void MediaToolbarButtonView::UpdateIcon() {
  if (!GetWidget())
    return;

  const gfx::VectorIcon& icon = ui::MaterialDesignController::touch_ui()
                                    ? kMediaToolbarButtonTouchIcon
                                    : kMediaToolbarButtonIcon;

  const SkColor normal_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  const SkColor disabled_color = GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE);

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, normal_color));

#if defined(OS_MACOSX)
  // On Mac OS X, the toolbar is set to disabled any time the current window is
  // not in focus. This causes the icon to look disabled in weird cases, such as
  // when the dialog is open. Therefore on Mac we only set the disabled image
  // when necessary.
  if (GetEnabled()) {
    SetImage(views::Button::STATE_DISABLED, gfx::ImageSkia());
  } else {
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(icon, disabled_color));
  }
#else
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(icon, disabled_color));
#endif  // defined(OS_MACOSX)
}

void MediaToolbarButtonView::ShowPromo() {
  EnsurePromoController();
  promo_controller_->ShowPromo();
  is_promo_showing_ = true;
  GetInkDrop()->AnimateToState(views::InkDropState::ACTIVATED);
}

void MediaToolbarButtonView::OnPromoEnded() {
  is_promo_showing_ = false;
  GetInkDrop()->AnimateToState(views::InkDropState::HIDDEN);
}

void MediaToolbarButtonView::EnsurePromoController() {
  if (promo_controller_)
    return;

  promo_controller_ = std::make_unique<GlobalMediaControlsPromoController>(
      this, browser_->profile());
  AddObserver(promo_controller_.get());
}
