// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/ntp_discount_consent_dialog_view.h"

#include "base/callback_helpers.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#include "chrome/browser/ui/commerce/commerce_prompt.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {
// Spacing between child of the Discount Consent Dialog View
constexpr int kChildSpacing = 24;
}  // namespace

namespace commerce {
void ShowDiscountConsentPrompt(
    Browser* browser,
    base::OnceCallback<void(chrome_cart::mojom::ConsentStatus)> callback) {
  constrained_window::CreateBrowserModalDialogViews(
      std::make_unique<NtpDiscountConsentDialogView>(std::move(callback)),
      browser->window()->GetNativeWindow())
      ->Show();
}
}  // namespace commerce

NtpDiscountConsentDialogView::NtpDiscountConsentDialogView(
    ActionCallback callback)
    : callback_(std::move(callback)) {
  // Set up dialog properties.
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetShowCloseButton(false);
  SetOwnedByWidget(true);
  // TODO(meiliang@): Set text for the button.
  SetButtons(ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK);
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_DISCOUNT_CONTEXTUAL_CONSENT_NO_THANKS));
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_NATIVE_NTP_CART_DISCOUNT_CONSENT_ACCEPT_BUTTON));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetAcceptCallback(base::BindOnce(&NtpDiscountConsentDialogView::OnAccept,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&NtpDiscountConsentDialogView::OnReject,
                                   base::Unretained(this)));
  SetCloseCallback(base::BindOnce(&NtpDiscountConsentDialogView::OnDismiss,
                                  base::Unretained(this)));

  // Set up dialog content view.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG),
      kChildSpacing));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  AddChildView(std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_NTP_CART_DISCOUNT_CONSENT_LIGHT),
      *bundle.GetImageSkiaNamed(IDR_NTP_CART_DISCOUNT_CONSENT_DARK),
      base::BindRepeating(&NtpDiscountConsentDialogView::GetBackgroundColor,
                          base::Unretained(this))));

  auto title_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_NATIVE_NTP_CART_DISCOUNT_CONSENT_TITLE),
      CONTEXT_HEADLINE, views::style::STYLE_PRIMARY);
  auto* title = AddChildView(std::move(title_label));
  title->SetMultiLine(true);

  auto consent_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_NATIVE_NTP_CART_DISCOUNT_CONSENT_BODY),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  auto* consent = AddChildView(std::move(consent_label));
  consent->SetMultiLine(true);
}

NtpDiscountConsentDialogView::~NtpDiscountConsentDialogView() = default;

SkColor NtpDiscountConsentDialogView::GetBackgroundColor() {
  return GetWidget()->GetColorProvider()->GetColor(ui::kColorDialogBackground);
}

void NtpDiscountConsentDialogView::OnAccept() {
  assert(callback_);
  std::move(callback_).Run(chrome_cart::mojom::ConsentStatus::ACCEPTED);
}

void NtpDiscountConsentDialogView::OnReject() {
  assert(callback_);
  std::move(callback_).Run(chrome_cart::mojom::ConsentStatus::REJECTED);
}

void NtpDiscountConsentDialogView::OnDismiss() {
  assert(callback_);
  std::move(callback_).Run(chrome_cart::mojom::ConsentStatus::DISMISSED);
}
