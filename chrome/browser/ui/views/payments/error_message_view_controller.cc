// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/error_message_view_controller.h"

#include <memory>

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/features.h"
#include "components/payments/core/sizes.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class PaymentsErrorLabel : public views::Label {
  METADATA_HEADER(PaymentsErrorLabel, views::Label)

 public:
  explicit PaymentsErrorLabel(const std::u16string& text) : Label(text) {
    SetMultiLine(true);
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (base::FeatureList::IsEnabled(
            payments::features::kPaymentRequestMandatoryPaymentAppUi)) {
      SetTextStyle(views::style::STYLE_BODY_3);
      SetAllowCharacterBreak(true);
    }
  }
  ~PaymentsErrorLabel() override = default;

  // views::Label:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    SetEnabledColor(
        base::FeatureList::IsEnabled(
            payments::features::kPaymentRequestMandatoryPaymentAppUi)
            ? GetColorProvider()->GetColor(ui::kColorSysOnSurfaceSubtle)
            : GetColorProvider()->GetColor(ui::kColorAlertHighSeverity));
  }
};

BEGIN_METADATA(PaymentsErrorLabel)
END_METADATA

}  // namespace

namespace payments {

ErrorMessageViewController::ErrorMessageViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog) {}

ErrorMessageViewController::~ErrorMessageViewController() = default;

std::u16string ErrorMessageViewController::GetPrimaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

PaymentRequestSheetController::ButtonCallback
ErrorMessageViewController::GetPrimaryButtonCallback() {
  return base::BindRepeating(&ErrorMessageViewController::CloseButtonPressed,
                             base::Unretained(this));
}

int ErrorMessageViewController::GetPrimaryButtonId() {
  return static_cast<int>(DialogViewID::CANCEL_BUTTON);
}

bool ErrorMessageViewController::GetPrimaryButtonEnabled() {
  return true;
}

bool ErrorMessageViewController::ShouldShowHeaderBackArrow() {
  return false;
}

bool ErrorMessageViewController::ShouldShowSecondaryButton() {
  return false;
}

std::u16string ErrorMessageViewController::GetSheetTitle() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_ERROR_MESSAGE_DIALOG_TITLE);
}

void ErrorMessageViewController::PopulateSheetHeaderView(views::View* view) {
  PaymentRequestSheetController::PopulateSheetHeaderView(view);
  if (base::FeatureList::IsEnabled(
          features::kPaymentRequestMandatoryPaymentAppUi)) {
    views::Label* title_label = static_cast<views::Label*>(
        view->GetViewByID(static_cast<int>(DialogViewID::SHEET_TITLE)));
    if (title_label) {
      title_label->SetTextStyle(views::style::STYLE_HEADLINE_4);
    }
  }
}

void ErrorMessageViewController::FillContentView(views::View* content_view) {
  if (base::FeatureList::IsEnabled(
          features::kPaymentRequestMandatoryPaymentAppUi)) {
    views::LayoutProvider* provider = views::LayoutProvider::Get();
    views::BoxLayout* layout = content_view->SetLayoutManager(std::make_unique<
                                                              views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::TLBR(
            /*top=*/kPaymentHandlerErrorRowTopInset,
            /*left=*/provider->GetInsetsMetric(views::INSETS_DIALOG).left(),
            /*bottom=*/0,
            /*right=*/provider->GetInsetsMetric(views::INSETS_DIALOG).right()),
        provider->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto error_icon =
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            ::features::IsRoundedIconsEnabled()
                ? vector_icons::kErrorIcon
                : vector_icons::kErrorOutlineOldIcon,
            ui::kColorSysOnSurfaceSubtle, kPaymentHandlerErrorIconSize));
    content_view->AddChildView(std::move(error_icon));

    std::u16string merchant_origin =
        url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(state()->GetTopOrigin()),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    std::u16string app_origin = url_formatter::FormatOriginForSecurityDisplay(
        url::Origin::Create(GURL(state()->selected_app()->GetId())),
        url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
    std::u16string message = l10n_util::GetStringFUTF16(
        IDS_PAYMENTS_MANDATORY_PAYMENT_APP_UI_ERROR_MESSAGE, merchant_origin,
        app_origin);
    views::Label* label = content_view->AddChildView(
        std::make_unique<PaymentsErrorLabel>(message));
    // The label should expand to take up all remaining horizontal space.
    layout->SetFlexForView(label, 1);
  } else {
    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::VH(0, kPaymentRequestRowHorizontalInsets), 0);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    content_view->SetLayoutManager(std::move(layout));
    std::u16string message =
        l10n_util::GetStringUTF16(IDS_PAYMENTS_ERROR_MESSAGE);
    content_view->AddChildView(std::make_unique<PaymentsErrorLabel>(message));
  }
}

bool ErrorMessageViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::ERROR_SHEET;
  return true;
}

bool ErrorMessageViewController::ShouldAccelerateEnterKey() {
  return true;
}

bool ErrorMessageViewController::CanContentViewBeScrollable() {
  // The error message is a single line of text that doesn't need a scroll view.
  return false;
}

base::WeakPtr<PaymentRequestSheetController>
ErrorMessageViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
