// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_ui_observer.h"
#include "components/payments/content/secure_payment_confirmation_model.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace payments {
namespace {

// Height of each row.
constexpr int kRowHeight = 48;

// Records UMA metric for the authentication dialog result.
void RecordAuthenticationDialogResult(
    const SecurePaymentConfirmationAuthenticationDialogResult result) {
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      result);
}

}  // namespace

// static
base::WeakPtr<SecurePaymentConfirmationView>
SecurePaymentConfirmationView::Create(
    const PaymentUIObserver* payment_ui_observer) {
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
    ObserverForTest* observer_for_test,
    const PaymentUIObserver* ui_observer_for_test)
    : observer_for_test_(observer_for_test),
      ui_observer_for_test_(ui_observer_for_test) {}
SecurePaymentConfirmationDialogView::~SecurePaymentConfirmationDialogView() =
    default;

void SecurePaymentConfirmationDialogView::ShowDialog(
    content::WebContents* web_contents,
    base::WeakPtr<SecurePaymentConfirmationModel> model,
    VerifyCallback verify_callback,
    CancelCallback cancel_callback) {
  DCHECK(model);
  model_ = model;

  InitChildViews();

  OnModelUpdated();

  verify_callback_ = std::move(verify_callback);
  cancel_callback_ = std::move(cancel_callback);

  SetAcceptCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  SetModalType(ui::MODAL_TYPE_CHILD);

  constrained_window::ShowWebModalDialogViews(this, web_contents);

  // observer_for_test_ is used in views browsertests.
  if (observer_for_test_)
    observer_for_test_->OnDialogOpened();

  // ui_observer_for_test_ is used in platform browsertests.
  if (ui_observer_for_test_)
    ui_observer_for_test_->OnUIDisplayed();
}

void SecurePaymentConfirmationDialogView::OnDialogAccepted() {
  std::move(verify_callback_).Run();
  RecordAuthenticationDialogResult(
      SecurePaymentConfirmationAuthenticationDialogResult::kAccepted);

  if (observer_for_test_) {
    observer_for_test_->OnConfirmButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogCancelled() {
  std::move(cancel_callback_).Run();
  RecordAuthenticationDialogResult(
      SecurePaymentConfirmationAuthenticationDialogResult::kCanceled);

  if (observer_for_test_) {
    observer_for_test_->OnCancelButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogClosed() {
  std::move(cancel_callback_).Run();
  RecordAuthenticationDialogResult(
      SecurePaymentConfirmationAuthenticationDialogResult::kClosed);

  if (observer_for_test_) {
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnModelUpdated() {
  // Changing the progress bar visibility does not invalidate layout as it is
  // absolutely positioned.
  if (progress_bar_)
    progress_bar_->SetVisible(model_->progress_bar_visible());

  SetButtonLabel(ui::DIALOG_BUTTON_OK, model_->verify_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, model_->verify_button_enabled());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_enabled());

  UpdateLabelView(DialogViewID::TITLE, model_->title());
  UpdateLabelView(DialogViewID::MERCHANT_LABEL, model_->merchant_label());
  UpdateLabelView(DialogViewID::MERCHANT_VALUE, model_->merchant_value());
  UpdateLabelView(DialogViewID::INSTRUMENT_LABEL, model_->instrument_label());
  UpdateLabelView(DialogViewID::INSTRUMENT_VALUE, model_->instrument_value());

  // Update the instrument icon only if it's changed
  if (model_->instrument_icon() &&
      (model_->instrument_icon() != instrument_icon_ ||
       model_->instrument_icon()->getGenerationID() !=
           instrument_icon_generation_id_)) {
    instrument_icon_generation_id_ =
        model_->instrument_icon()->getGenerationID();
    gfx::ImageSkia image =
        gfx::ImageSkia::CreateFrom1xBitmap(*model_->instrument_icon())
            .DeepCopy();

    static_cast<views::ImageView*>(
        GetViewByID(static_cast<int>(DialogViewID::INSTRUMENT_ICON)))
        ->SetImage(image);
  }
  instrument_icon_ = model_->instrument_icon();

  UpdateLabelView(DialogViewID::TOTAL_LABEL, model_->total_label());
  UpdateLabelView(DialogViewID::TOTAL_VALUE, model_->total_value());
}

void SecurePaymentConfirmationDialogView::UpdateLabelView(
    DialogViewID id,
    const base::string16& text) {
  static_cast<views::Label*>(GetViewByID(static_cast<int>(id)))->SetText(text);
}

void SecurePaymentConfirmationDialogView::HideDialog() {
  if (GetWidget())
    GetWidget()->Close();
}

bool SecurePaymentConfirmationDialogView::ShouldShowCloseButton() const {
  return false;
}

bool SecurePaymentConfirmationDialogView::Accept() {
  views::DialogDelegateView::Accept();
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
  RemoveAllChildViews(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  AddChildView(CreateHeaderView());

  AddChildView(CreateBodyView());

  InvalidateLayout();
}

// Creates the header view, which is the fingerprint icon and a progress bar.
// The fingerprint icon covers the whole header view and the progress bar is
// overlaid on the top of the header.
// +------------------------------------------+
// |===============progress bar===============|
// |                                          |
// |             fingerprint icon             |
// +------------------------------------------+
std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateHeaderView() {
  const int header_width = GetSecurePaymentConfirmationHeaderWidth();
  const gfx::Size header_size(header_width, kHeaderIconHeight);

  // The container view has no layout, so its preferred size is hardcoded to
  // match the size of the image, and the progress bar overlay is absolutely
  // positioned.
  auto header_view = std::make_unique<views::View>();
  header_view->SetPreferredSize(header_size);

  // Fingerprint header icon
  auto image_view = CreateSecurePaymentConfirmationHeaderView(
      GetNativeTheme()->ShouldUseDarkColors());
  image_view->SetID(static_cast<int>(DialogViewID::HEADER_ICON));
  header_view->AddChildView(image_view.release());

  // Progress bar
  auto progress_bar = CreateSecurePaymentConfirmationProgressBarView();
  progress_bar->SetID(static_cast<int>(DialogViewID::PROGRESS_BAR));
  progress_bar->SetVisible(model_->progress_bar_visible());
  progress_bar_ = progress_bar.get();
  header_view->AddChildView(progress_bar.release());

  return header_view;
}

// Creates the body.
// +------------------------------------------+
// | Title                                    |
// |                                          |
// | merchant label      value                |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | instrument label    value           icon |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | total label         value                |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateBodyView() {
  auto body = std::make_unique<views::View>();
  body->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kBodyInsets, kBodyInsets, kBodyExtraInset, kBodyInsets)));

  views::GridLayout* layout =
      body->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  std::unique_ptr<views::Label> title_text =
      CreateSecurePaymentConfirmationTitleLabel(model_->title());
  title_text->SetID(static_cast<int>(DialogViewID::TITLE));
  layout->AddView(std::move(title_text));

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(CreateRows());

  return body;
}

// Creates the set of merchant, instrument, and total rows.
// +------------------------------------------+
// | merchant label     value                 |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | instrument label   value            icon |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
// | total label        value                 |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View> SecurePaymentConfirmationDialogView::CreateRows() {
  auto rows = std::make_unique<views::View>();

  views::GridLayout* layout =
      rows->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(
      CreateRowView(model_->merchant_label(), DialogViewID::MERCHANT_LABEL,
                    model_->merchant_value(), DialogViewID::MERCHANT_VALUE));

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(
      CreateRowView(model_->instrument_label(), DialogViewID::INSTRUMENT_LABEL,
                    model_->instrument_value(), DialogViewID::INSTRUMENT_VALUE,
                    model_->instrument_icon(), DialogViewID::INSTRUMENT_ICON));

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(
      CreateRowView(model_->total_label(), DialogViewID::TOTAL_LABEL,
                    model_->total_value(), DialogViewID::TOTAL_VALUE));

  return rows;
}

// Creates a row of data with |label|, |value|, and optionally |icon|.
// +------------------------------------------+
// | label      value                    icon |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- border
std::unique_ptr<views::View> SecurePaymentConfirmationDialogView::CreateRowView(
    const base::string16& label,
    DialogViewID label_id,
    const base::string16& value,
    DialogViewID value_id,
    const SkBitmap* icon,
    DialogViewID icon_id) {
  std::unique_ptr<views::View> row = std::make_unique<views::View>();

  row->SetBorder(views::CreateSolidSidedBorder(
      0, 0, 1, 0,
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_SeparatorColor)));

  views::GridLayout* layout =
      row->SetLayoutManager(std::make_unique<views::GridLayout>());

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // Label column
  constexpr int kLabelColumnWidth = 80;
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kFixed, kLabelColumnWidth,
                     0);

  constexpr int kPaddingAfterLabel = 24;
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, kPaddingAfterLabel);

  // Value column
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  // Icon column
  if (icon) {
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       views::GridLayout::kFixedSize,
                       views::GridLayout::ColumnSize::kFixed,
                       kInstrumentIconWidth, kInstrumentIconWidth);
  }

  layout->StartRow(views::GridLayout::kFixedSize, 0, kRowHeight);

  std::unique_ptr<views::Label> label_text = std::make_unique<views::Label>(
      label, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  label_text->SetLineHeight(kDescriptionLineHeight);
  label_text->SetID(static_cast<int>(label_id));
  layout->AddView(std::move(label_text));

  std::unique_ptr<views::Label> value_text = std::make_unique<views::Label>(
      value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  value_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  value_text->SetLineHeight(kDescriptionLineHeight);
  value_text->SetID(static_cast<int>(value_id));
  layout->AddView(std::move(value_text));

  if (icon) {
    instrument_icon_ = model_->instrument_icon();
    instrument_icon_generation_id_ =
        model_->instrument_icon()->getGenerationID();

    std::unique_ptr<views::ImageView> icon_view =
        CreateSecurePaymentConfirmationInstrumentIconView(
            *model_->instrument_icon());
    icon_view->SetID(static_cast<int>(icon_id));
    layout->AddView(std::move(icon_view));
  }

  return row;
}

BEGIN_METADATA(SecurePaymentConfirmationDialogView, views::DialogDelegateView)
END_METADATA

}  // namespace payments
