// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_ui_observer.h"
#include "components/payments/content/secure_payment_confirmation_model.h"
#include "components/payments/core/features.h"
#include "components/payments/core/sizes.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"

namespace payments {

using features::SecurePaymentConfirmationNetworkAndIssuerIconsTreatment;

namespace {

class BorderedRowView : public views::View {
  METADATA_HEADER(BorderedRowView, views::View)

 public:
  void OnThemeChanged() override {
    View::OnThemeChanged();
    SetBorder(views::CreateSolidSidedBorder(
        gfx::Insets::TLBR(0, 0, 1, 0),
        GetColorProvider()->GetColor(ui::kColorSeparator)));
  }
};

BEGIN_METADATA(BorderedRowView)
END_METADATA

std::unique_ptr<views::View> CreateSpacer(
    views::DistanceMetric vertical_distance) {
  return views::Builder<views::View>()
      .SetPreferredSize(gfx::Size(
          /*width=*/1,
          views::LayoutProvider::Get()->GetDistanceMetric(vertical_distance)))
      .Build();
}

std::u16string GetTitleText(std::u16string title_text,
                            std::u16string relying_party_id) {
  if (features::GetNetworkAndIssuerIconsTreatment() !=
      SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kInline) {
    return title_text;
  }
  return base::ReplaceStringPlaceholders(title_text, relying_party_id, nullptr);
}

void UpdateProgressBarVisiblity(views::BubbleFrameView* bubble_frame_view,
                                bool visible) {
  if (bubble_frame_view) {
    // -1 indicates an infinitely animating progress
    bubble_frame_view->SetProgress(visible ? std::optional<double>(-1)
                                           : std::nullopt);
  }
}

}  // namespace

// static
base::WeakPtr<SecurePaymentConfirmationView>
SecurePaymentConfirmationView::Create(
    const base::WeakPtr<PaymentUIObserver> payment_ui_observer) {
  // On desktop, the SecurePaymentConfirmationView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  return (new SecurePaymentConfirmationDialogView(
              /*observer_for_test=*/nullptr, payment_ui_observer))
      ->GetWeakPtr();
}

SecurePaymentConfirmationView::SecurePaymentConfirmationView() = default;
SecurePaymentConfirmationView::~SecurePaymentConfirmationView() = default;

SecurePaymentConfirmationDialogView::SecurePaymentConfirmationDialogView(
    base::WeakPtr<ObserverForTest> observer_for_test,
    const base::WeakPtr<PaymentUIObserver> ui_observer_for_test)
    : observer_for_test_(observer_for_test),
      ui_observer_for_test_(ui_observer_for_test) {}
SecurePaymentConfirmationDialogView::~SecurePaymentConfirmationDialogView() =
    default;

void SecurePaymentConfirmationDialogView::ShowDialog(
    content::WebContents* web_contents,
    base::WeakPtr<SecurePaymentConfirmationModel> model,
    VerifyCallback verify_callback,
    CancelCallback cancel_callback,
    OptOutCallback opt_out_callback) {
  DCHECK(model);
  model_ = model;

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  InitChildViews();

  OnModelUpdated();

  verify_callback_ = std::move(verify_callback);
  cancel_callback_ = std::move(cancel_callback);
  opt_out_callback_ = std::move(opt_out_callback);

  SetAcceptCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  SetModalType(ui::mojom::ModalType::kChild);

  views::Widget* widget =
      constrained_window::ShowWebModalDialogViews(this, web_contents);
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(widget);

  // The progress bar doesn't exist until after ShowWebModalDialogViews, so we
  // have to update it here in case it starts visible.
  UpdateProgressBarVisiblity(GetBubbleFrameView(),
                             model_->progress_bar_visible());

  // ui_observer_for_test_ is used in platform browsertests.
  if (ui_observer_for_test_)
    ui_observer_for_test_->OnUIDisplayed();
}

void SecurePaymentConfirmationDialogView::OnDialogAccepted() {
  std::move(verify_callback_).Run();
  if (observer_for_test_) {
    observer_for_test_->OnConfirmButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogCancelled() {
  std::move(cancel_callback_).Run();
  if (observer_for_test_) {
    observer_for_test_->OnCancelButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogClosed() {
  // We can reach OnDialogClosed either when the user cancels out of the
  // WebAuthn dialog after clicking 'Verify', or when the user chooses to
  // opt-out. We should only run the cancellation callback in the former case;
  // in the latter the opt-out callback will trigger from OnOptOutClicked.
  if (!model_->opt_out_clicked()) {
    std::move(cancel_callback_).Run();
  }

  if (observer_for_test_) {
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnOptOutClicked() {
  if (observer_for_test_) {
    observer_for_test_->OnOptOutClicked();
  }
  std::move(opt_out_callback_).Run();
}

void SecurePaymentConfirmationDialogView::OnModelUpdated() {
  UpdateProgressBarVisiblity(GetBubbleFrameView(),
                             model_->progress_bar_visible());

  SetButtonLabel(ui::mojom::DialogButton::kOk, model_->verify_button_label());
  SetButtonEnabled(ui::mojom::DialogButton::kOk,
                   model_->verify_button_enabled());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 model_->cancel_button_label());
  SetButtonEnabled(ui::mojom::DialogButton::kCancel,
                   model_->cancel_button_enabled());

  SetAccessibleTitle(model_->title());
  UpdateLabelView(DialogViewID::TITLE,
                  GetTitleText(model_->title(), model_->relying_party_id()));
  UpdateLabelView(DialogViewID::MERCHANT_LABEL, model_->merchant_label());
  UpdateLabelView(
      DialogViewID::MERCHANT_VALUE,
      FormatMerchantLabel(model_->merchant_name(), model_->merchant_origin()));
  UpdateLabelView(DialogViewID::INSTRUMENT_LABEL, model_->instrument_label());
  UpdateLabelView(DialogViewID::INSTRUMENT_VALUE, model_->instrument_value());

  // Update the instrument icon only if it's changed
  if (model_->instrument_icon()) {
    auto* image_view = static_cast<views::ImageView*>(
        GetViewByID(static_cast<int>(DialogViewID::INSTRUMENT_ICON)));
    if (model_->instrument_icon() != instrument_icon_ ||
        model_->instrument_icon()->getGenerationID() !=
            instrument_icon_generation_id_) {
      instrument_icon_generation_id_ =
          model_->instrument_icon()->getGenerationID();
      gfx::ImageSkia image =
          gfx::ImageSkia::CreateFrom1xBitmap(*model_->instrument_icon())
              .DeepCopy();
      image_view->SetImage(ui::ImageModel::FromImageSkia(image));
    }
    if (model_->instrument_icon()->drawsNothing()) {
      image_view->SetImage(ui::ImageModel::FromVectorIcon(
          kCreditCardIcon, ui::kColorDialogForeground,
          kSecurePaymentConfirmationIconDefaultWidthPx));
    }
  }

  instrument_icon_ = model_->instrument_icon();

  UpdateLabelView(DialogViewID::TOTAL_LABEL, model_->total_label());
  UpdateLabelView(DialogViewID::TOTAL_VALUE, model_->total_value());

  opt_out_view_->SetVisible(model_->opt_out_visible());
}

void SecurePaymentConfirmationDialogView::UpdateLabelView(
    DialogViewID id,
    const std::u16string& text) {
  static_cast<views::Label*>(GetViewByID(static_cast<int>(id)))->SetText(text);
}

void SecurePaymentConfirmationDialogView::HideDialog() {
  if (GetWidget())
    GetWidget()->Close();
}

bool SecurePaymentConfirmationDialogView::ClickOptOutForTesting() {
  if (!model_->opt_out_visible())
    return false;
  OnOptOutClicked();
  return true;
}

bool SecurePaymentConfirmationDialogView::ShouldShowCloseButton() const {
  return false;
}

bool SecurePaymentConfirmationDialogView::Accept() {
  views::DialogDelegateView::Accept();

  // Disable the opt-out link to avoid the user clicking on it whilst the
  // WebAuthn dialog is showing over the SPC one. If opt-out support wasn't
  // requested by the SPC caller, it won't be visible and doesn't need disabled.
  //
  // TODO(crbug.com/40225659): Even disabled this link still looks clickable
  // (underline disappears, but color doesn't change). Force style the color?
  if (opt_out_view_->GetVisible()) {
    opt_out_view_->SetEnabled(false);
  }

  // Returning "false" to keep the dialog open after "Confirm" button is
  // pressed, so the dialog can show a progress bar and wait for the user to use
  // their authenticator device.
  return false;
}

base::WeakPtr<SecurePaymentConfirmationDialogView>
SecurePaymentConfirmationDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SecurePaymentConfirmationDialogView::InitChildViews() {
  RemoveAllChildViews();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  // When the network/issuer icons are inline with the title for the transaction
  // UX, we don't draw an additional logo on top.
  if (features::GetNetworkAndIssuerIconsTreatment() !=
      SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kInline) {
    AddChildView(CreateSecurePaymentConfirmationHeaderIcon(
        static_cast<int>(DialogViewID::HEADER_ICON)));
  }

  AddChildView(CreateBodyView());

  // We always create the view for the Opt Out link, but show or hide it
  // depending on whether it was requested. The visibility status is set in
  // OnModelUpdated.
  opt_out_view_ = SetFootnoteView(CreateSecurePaymentConfirmationOptOutView(
      model_->relying_party_id(), model_->opt_out_label(),
      model_->opt_out_link_label(),
      base::BindRepeating(&SecurePaymentConfirmationDialogView::OnOptOutClicked,
                          weak_ptr_factory_.GetWeakPtr())));

  InvalidateLayout();
}

// Creates the body including the title, the set of merchant, instrument, and
// total rows.
// +------------------------------------------+
// | Title                                    |
// |                                          |
// | merchant label      value                |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | instrument label    [icon] value         |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | total label         value                |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | network label       [icon] value         |  <-- optional
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | issuer label        [icon] value         |  <-- optional
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateBodyView() {
  auto body_view = std::make_unique<views::BoxLayoutView>();
  body_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  body_view->SetInsideBorderInsets(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  body_view->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  body_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  std::unique_ptr<views::Label> title_text =
      CreateSecurePaymentConfirmationTitleLabel(
          GetTitleText(model_->title(), model_->relying_party_id()));
  title_text->SetID(static_cast<int>(DialogViewID::TITLE));
  if (features::GetNetworkAndIssuerIconsTreatment() ==
      SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kInline) {
    body_view->AddChildView(CreateSecurePaymentConfirmationInlineImageTitleView(
        std::move(title_text), *model_->network_icon(),
        static_cast<int>(DialogViewID::NETWORK_ICON), *model_->issuer_icon(),
        static_cast<int>(DialogViewID::ISSUER_ICON)));

    body_view->AddChildView(
        CreateSpacer(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

    auto description_text = std::make_unique<views::Label>(
        model_->description(), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_SECONDARY);
    description_text->SetID(static_cast<int>(DialogViewID::DESCRIPTION));
    description_text->SetLineHeight(kDescriptionLineHeight);
    description_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    body_view->AddChildView(std::move(description_text));
  } else {
    body_view->AddChildView(std::move(title_text));
  }

  body_view->AddChildView(
      CreateSpacer(views::DISTANCE_RELATED_CONTROL_VERTICAL));

  body_view->AddChildView(CreateRowView(
      model_->merchant_label(), DialogViewID::MERCHANT_LABEL,
      FormatMerchantLabel(model_->merchant_name(), model_->merchant_origin()),
      DialogViewID::MERCHANT_VALUE));

  // When including the Network and Issuer icons, the Total line comes before
  // the Payment Instrument, versus the current UI where it comes afterwards.
  std::unique_ptr<views::View> total_line_view =
      CreateRowView(model_->total_label(), DialogViewID::TOTAL_LABEL,
                    model_->total_value(), DialogViewID::TOTAL_VALUE);
  if (features::GetNetworkAndIssuerIconsTreatment() !=
      SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kNone) {
    body_view->AddChildView(std::move(total_line_view));
  }

  body_view->AddChildView(
      CreateRowView(model_->instrument_label(), DialogViewID::INSTRUMENT_LABEL,
                    model_->instrument_value(), DialogViewID::INSTRUMENT_VALUE,
                    model_->instrument_icon(), DialogViewID::INSTRUMENT_ICON));

  if (features::GetNetworkAndIssuerIconsTreatment() ==
      SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kNone) {
    body_view->AddChildView(std::move(total_line_view));
  }

  // Add the Network and Issuer icons, if the flag is enabled and an icon was
  // specified and successfully downloaded.
  if (features::GetNetworkAndIssuerIconsTreatment() ==
      SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kRows) {
    if (!model_->network_icon()->drawsNothing()) {
      body_view->AddChildView(
          CreateRowView(model_->network_label(), DialogViewID::NETWORK_LABEL,
                        model_->network_value(), DialogViewID::NETWORK_VALUE,
                        model_->network_icon(), DialogViewID::NETWORK_ICON));
    }

    if (!model_->issuer_icon()->drawsNothing()) {
      body_view->AddChildView(
          CreateRowView(model_->issuer_label(), DialogViewID::ISSUER_LABEL,
                        model_->issuer_value(), DialogViewID::ISSUER_VALUE,
                        model_->issuer_icon(), DialogViewID::ISSUER_ICON));
    }
  }

  return body_view;
}

// Creates a row of data with |label|, |value|, and optionally |icon|.
// +------------------------------------------+
// | label      [icon] value                  |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- border
std::unique_ptr<views::View> SecurePaymentConfirmationDialogView::CreateRowView(
    const std::u16string& label,
    DialogViewID label_id,
    const std::u16string& value,
    DialogViewID value_id,
    const SkBitmap* icon,
    DialogViewID icon_id) {
  auto row = std::make_unique<BorderedRowView>();

  views::TableLayout* layout =
      row->SetLayoutManager(std::make_unique<views::TableLayout>());

  // Label column
  constexpr int kLabelColumnWidth = 80;
  layout->AddColumn(
      views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
      kLabelColumnWidth, 0);

  constexpr int kPaddingAfterLabel = 24;
  layout->AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingAfterLabel);

  // Icon column
  if (icon) {
    layout->AddColumn(
        views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
        views::TableLayout::kFixedSize,
        views::TableLayout::ColumnSize::kUsePreferred,
        /*fixed_width=*/0,
        /*min_width=*/kSecurePaymentConfirmationIconDefaultWidthPx);
    layout->AddPaddingColumn(views::TableLayout::kFixedSize,
                             ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  }

  // Value column
  layout->AddColumn(views::LayoutAlignment::kStretch,
                    views::LayoutAlignment::kCenter, 1.0f,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->AddRows(1, views::TableLayout::kFixedSize, kPaymentInfoRowHeight);

  std::unique_ptr<views::Label> label_text = std::make_unique<views::Label>(
      label, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_EMPHASIZED_SECONDARY);
  label_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  label_text->SetLineHeight(kDescriptionLineHeight);
  label_text->SetID(static_cast<int>(label_id));
  row->AddChildView(std::move(label_text));

  if (icon) {
    gfx::ImageSkia skia_icon;

    // TODO(crbug.com/333945861): CreateRowView shouldn't need to know to do
    // clever things with the instrument icon. The empty icon should be resolved
    // before calling this method.
    if (icon_id == DialogViewID::INSTRUMENT_ICON) {
      instrument_icon_ = model_->instrument_icon();
      instrument_icon_generation_id_ =
          model_->instrument_icon()->getGenerationID();

      // The instrument icon may be empty, if it couldn't be downloaded/decoded
      // and iconMustBeShown was set to false. In that case, use a default icon.
      // The actual display color is set based on the theme in OnThemeChanged.
      if (instrument_icon_->drawsNothing()) {
        skia_icon = gfx::CreateVectorIcon(
            kCreditCardIcon, kSecurePaymentConfirmationIconDefaultWidthPx,
            gfx::kPlaceholderColor);
      } else {
        skia_icon =
            gfx::ImageSkia::CreateFrom1xBitmap(*model_->instrument_icon())
                .DeepCopy();
      }
    } else {
      skia_icon = gfx::ImageSkia::CreateFrom1xBitmap(*icon).DeepCopy();
    }

    std::unique_ptr<views::ImageView> icon_view =
        CreateSecurePaymentConfirmationIconView(std::move(skia_icon));
    icon_view->SetID(static_cast<int>(icon_id));
    row->AddChildView(std::move(icon_view));
  }

  std::unique_ptr<views::Label> value_text = std::make_unique<views::Label>(
      value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  value_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  value_text->SetLineHeight(kDescriptionLineHeight);
  value_text->SetID(static_cast<int>(value_id));
  row->AddChildView(std::move(value_text));

  return row;
}

BEGIN_METADATA(SecurePaymentConfirmationDialogView)
END_METADATA

}  // namespace payments
