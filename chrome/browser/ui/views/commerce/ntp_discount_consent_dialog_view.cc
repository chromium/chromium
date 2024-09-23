// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/ntp_discount_consent_dialog_view.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commerce/commerce_prompt.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {
// TODO(crbug.com/40226507): These are approved one-off dialog style, we will
// migrate when this bug is resolved.
// Spacing between child of the Discount Consent Dialog View
constexpr int kChildSpacing = 24;
constexpr int kBannerImageTopMargin = 44;
constexpr int kBodyContainerHorizontalMargin = 22;
constexpr int kBodyContainerBottomMargin = 54;
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
  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowCloseButton(false);
  SetOwnedByWidget(true);
  // TODO(meiliang@): Set text for the button.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel) |
             static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_DISCOUNT_CONTEXTUAL_CONSENT_NO_THANKS));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
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
  auto* layout_provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG), kChildSpacing));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  auto banner_image_container = std::make_unique<views::View>();
  banner_image_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  banner_image_container->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(kBannerImageTopMargin, 0, 0, 0));
  // It is safe to use base::Unretained() here because the GetBackgroundColor
  // will not be called after this class is destroyed.
  banner_image_container->AddChildView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          *bundle.GetImageSkiaNamed(IDR_NTP_CART_DISCOUNT_CONSENT_LIGHT),
          *bundle.GetImageSkiaNamed(IDR_NTP_CART_DISCOUNT_CONSENT_DARK),
          base::BindRepeating(&NtpDiscountConsentDialogView::GetBackgroundColor,
                              base::Unretained(this))));
  AddChildView(std::move(banner_image_container));

  // TODO(crbug.com/40227597): Remove the view wrappers when the bug is
  // resolved.
  auto title_container = std::make_unique<views::View>();
  title_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* title_label =
      title_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_NATIVE_NTP_CART_DISCOUNT_CONSENT_TITLE),
          CONTEXT_HEADLINE, views::style::STYLE_PRIMARY));
  title_label->SetMultiLine(true);
  AddChildView(std::move(title_container));

  // TODO(crbug.com/40227597): Remove the view wrappers when the bug is
  // resolved.
  auto body_container = std::make_unique<views::View>();
  body_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  body_container->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, kBodyContainerHorizontalMargin,
                                            kBodyContainerBottomMargin,
                                            kBodyContainerHorizontalMargin));
  auto* consent_label =
      body_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_NATIVE_NTP_CART_DISCOUNT_CONSENT_BODY),
          views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  consent_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  consent_label->SetAllowCharacterBreak(true);
  consent_label->SetMultiLine(true);
  AddChildView(std::move(body_container));
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
