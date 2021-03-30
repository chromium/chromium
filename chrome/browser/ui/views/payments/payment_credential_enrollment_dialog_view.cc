// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_credential_enrollment_dialog_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_credential_enrollment_model.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace payments {

// static
base::WeakPtr<PaymentCredentialEnrollmentView>
PaymentCredentialEnrollmentView::Create() {
  // On desktop, the SecurePaymentConfirmationView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  return (new PaymentCredentialEnrollmentDialogView(
              /*observer_for_test=*/nullptr))
      ->GetWeakPtr();
}

PaymentCredentialEnrollmentView::PaymentCredentialEnrollmentView() = default;
PaymentCredentialEnrollmentView::~PaymentCredentialEnrollmentView() = default;

PaymentCredentialEnrollmentDialogView::PaymentCredentialEnrollmentDialogView(
    ObserverForTest* observer_for_test)
    : observer_for_test_(observer_for_test) {}
PaymentCredentialEnrollmentDialogView::
    ~PaymentCredentialEnrollmentDialogView() = default;

void PaymentCredentialEnrollmentDialogView::ShowDialog(
    content::WebContents* web_contents,
    base::WeakPtr<PaymentCredentialEnrollmentModel> model,
    AcceptCallback accept_callback,
    CancelCallback cancel_callback) {
  DCHECK(model);
  model_ = model;

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  InitChildViews();

  OnModelUpdated();

  accept_callback_ = std::move(accept_callback);
  cancel_callback_ = std::move(cancel_callback);

  SetAcceptCallback(
      base::BindOnce(&PaymentCredentialEnrollmentDialogView::OnDialogAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(
      base::BindOnce(&PaymentCredentialEnrollmentDialogView::OnDialogCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(
      base::BindOnce(&PaymentCredentialEnrollmentDialogView::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  SetModalType(ui::MODAL_TYPE_CHILD);

  constrained_window::ShowWebModalDialogViews(this, web_contents);

  // observer_for_test_ is used in views browsertests.
  if (observer_for_test_)
    observer_for_test_->OnDialogOpened();
}

void PaymentCredentialEnrollmentDialogView::OnDialogAccepted() {
  std::move(accept_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnAcceptButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void PaymentCredentialEnrollmentDialogView::OnDialogCancelled() {
  std::move(cancel_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnCancelButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void PaymentCredentialEnrollmentDialogView::OnDialogClosed() {
  std::move(cancel_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnDialogClosed();
  }
}

void PaymentCredentialEnrollmentDialogView::OnModelUpdated() {
  views::View* progress_bar =
      GetViewByID(static_cast<int>(DialogViewID::PROGRESS_BAR));
  if (progress_bar)
    progress_bar->SetVisible(model_->progress_bar_visible());

  SetButtonLabel(ui::DIALOG_BUTTON_OK, model_->accept_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, model_->accept_button_enabled());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_enabled());

  SetAccessibleTitle(model_->title());
  UpdateLabelView(DialogViewID::TITLE, model_->title());
  UpdateLabelView(DialogViewID::DESCRIPTION, model_->description());

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

  UpdateLabelView(DialogViewID::INSTRUMENT_NAME, model_->instrument_name());

  if (!model_->extra_description().empty()) {
    UpdateLabelView(DialogViewID::EXTRA_DESCRIPTION,
                    model_->extra_description());
  }
}

void PaymentCredentialEnrollmentDialogView::UpdateLabelView(
    DialogViewID id,
    const std::u16string& text) {
  static_cast<views::Label*>(GetViewByID(static_cast<int>(id)))->SetText(text);
}

void PaymentCredentialEnrollmentDialogView::HideDialog() {
  if (GetWidget())
    GetWidget()->Close();
}

bool PaymentCredentialEnrollmentDialogView::ShouldShowCloseButton() const {
  return false;
}

bool PaymentCredentialEnrollmentDialogView::Accept() {
  views::DialogDelegateView::Accept();
  // Returning "false" to keep the dialog open after "Confirm" button is
  // pressed, so the dialog can show a progress bar and wait for the user to use
  // their authenticator device.
  return false;
}

base::WeakPtr<PaymentCredentialEnrollmentDialogView>
PaymentCredentialEnrollmentDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentCredentialEnrollmentDialogView::InitChildViews() {
  RemoveAllChildViews(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  AddChildView(CreateSecurePaymentConfirmationHeaderView(
      GetNativeTheme()->ShouldUseDarkColors(),
      static_cast<int>(DialogViewID::PROGRESS_BAR),
      static_cast<int>(DialogViewID::HEADER_ICON)));

  AddChildView(CreateBodyView());

  InvalidateLayout();
}

// Creates the body.
// +------------------------------------------+
// | Title                                    |
// |                                          |
// | Description                              |
// | [icon] instrument                        |
// | Extra incognito description              |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
PaymentCredentialEnrollmentDialogView::CreateBodyView() {
  auto body = std::make_unique<views::View>();
  body->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kBodyInsets, kBodyExtraInset, kBodyInsets, kBodyExtraInset)));

  views::GridLayout* layout =
      body->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  std::unique_ptr<views::Label> title_text =
      CreateSecurePaymentConfirmationTitleLabel(model_->title());
  title_text->SetID(static_cast<int>(DialogViewID::TITLE));
  layout->AddView(std::move(title_text));

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(CreateDescription(model_->description(),
                                    DialogViewID::DESCRIPTION));

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(CreateInstrumentRow());

  if (!model_->extra_description().empty()) {
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(CreateDescription(model_->extra_description(),
                                      DialogViewID::EXTRA_DESCRIPTION));
  }

  return body;
}

// Creates the description view.
// +------------------------------------------+
// | Description                              |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
PaymentCredentialEnrollmentDialogView::CreateDescription(
    const std::u16string& text,
    DialogViewID view_id) {
  auto description = std::make_unique<views::View>();

  description->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  auto description_label = std::make_unique<views::Label>(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  description_label->SetMultiLine(true);
  description_label->SetLineHeight(kDescriptionLineHeight);
  description_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  description_label->SetAllowCharacterBreak(true);
  description_label->SetID(static_cast<int>(view_id));
  description->AddChildView(description_label.release());

  return description;
}

// Creates the instrument row view.
// +------------------------------------------+
// | [icon] instrument                        |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
PaymentCredentialEnrollmentDialogView::CreateInstrumentRow() {
  std::unique_ptr<views::View> row = std::make_unique<views::View>();

  views::GridLayout* layout =
      row->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);

  // Icon column
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kFixed,
                     kInstrumentIconWidth, kInstrumentIconWidth);

  columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                            ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_LABEL_HORIZONTAL));

  // Instrument column
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, 0, kPaymentInfoRowHeight);

  instrument_icon_ = model_->instrument_icon();
  instrument_icon_generation_id_ = model_->instrument_icon()->getGenerationID();

  std::unique_ptr<views::ImageView> icon_view =
      CreateSecurePaymentConfirmationInstrumentIconView(
          *model_->instrument_icon());
  icon_view->SetID(static_cast<int>(DialogViewID::INSTRUMENT_ICON));
  layout->AddView(std::move(icon_view));

  std::unique_ptr<views::Label> instrument_name_view =
      std::make_unique<views::Label>(model_->instrument_name(),
                                     views::style::CONTEXT_DIALOG_BODY_TEXT,
                                     views::style::STYLE_PRIMARY);
  instrument_name_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  instrument_name_view->SetLineHeight(kDescriptionLineHeight);
  instrument_name_view->SetID(static_cast<int>(DialogViewID::INSTRUMENT_NAME));
  layout->AddView(std::move(instrument_name_view));

  return row;
}

BEGIN_METADATA(PaymentCredentialEnrollmentDialogView, views::DialogDelegateView)
END_METADATA

}  // namespace payments
