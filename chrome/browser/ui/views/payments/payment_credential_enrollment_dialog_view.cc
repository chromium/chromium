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
  // Changing the progress bar visibility does not invalidate layout as it is
  // absolutely positioned.
  if (progress_bar_)
    progress_bar_->SetVisible(model_->progress_bar_visible());

  SetButtonLabel(ui::DIALOG_BUTTON_OK, model_->accept_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, model_->accept_button_enabled());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_enabled());

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
}

void PaymentCredentialEnrollmentDialogView::UpdateLabelView(
    DialogViewID id,
    const base::string16& text) {
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
PaymentCredentialEnrollmentDialogView::CreateHeaderView() {
  const int header_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
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
// | Description                              |
// | icon                                     |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
PaymentCredentialEnrollmentDialogView::CreateBodyView() {
  auto body = std::make_unique<views::View>();
  body->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kBodyInsets, kBodyInsets, kBodyExtraInset, kBodyInsets)));

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
  layout->AddView(CreateDescription());

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  instrument_icon_ = model_->instrument_icon();
  instrument_icon_generation_id_ = model_->instrument_icon()->getGenerationID();

  std::unique_ptr<views::ImageView> icon_view =
      CreateSecurePaymentConfirmationInstrumentIconView(
          *model_->instrument_icon());
  icon_view->SetID(static_cast<int>(DialogViewID::INSTRUMENT_ICON));
  layout->AddView(std::move(icon_view));

  return body;
}

// Creates the description view.
// +------------------------------------------+
// | Description                              |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
PaymentCredentialEnrollmentDialogView::CreateDescription() {
  auto description = std::make_unique<views::View>();

  description->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  auto description_label = std::make_unique<views::Label>(
      model_->description(), views::style::CONTEXT_DIALOG_BODY_TEXT);
  description_label->SetMultiLine(true);
  description_label->SetLineHeight(kDescriptionLineHeight);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetAllowCharacterBreak(true);
  description_label->SetID(static_cast<int>(DialogViewID::DESCRIPTION));
  description->AddChildView(description_label.release());

  return description;
}

BEGIN_METADATA(PaymentCredentialEnrollmentDialogView, views::DialogDelegateView)
END_METADATA

}  // namespace payments
