// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/secure_payment_confirmation_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {
namespace {

// Height of the header icon.
constexpr int kHeaderIconHeight = 148;

// Height of the progress bar at the top of the dialog.
constexpr int kProgressBarHeight = 4;

// Size of the instrument icon shown in the payment method row.
constexpr int kInstrumentIconWidth = 32;
constexpr int kInstrumentIconHeight = 20;

// Line height of the title text.
constexpr int kTitleLineHeight = 24;

// Line height of the row text.
constexpr int kRowViewLineHeight = 20;

// Insets of the body content.
constexpr int kBodyInsets = 16;

// Extra inset between the body content and the dialog buttons.
constexpr int kBodyExtraInset = 24;

// Height of each row.
constexpr int kRowHeight = 48;

}  // namespace

SecurePaymentConfirmationDialogView::SecurePaymentConfirmationDialogView(
    ObserverForTest* observer_for_test)
    : observer_for_test_(observer_for_test) {}
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

  constrained_window::ShowWebModalDialogViews(this, web_contents);

  if (observer_for_test_)
    observer_for_test_->OnDialogOpened();
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
  std::move(cancel_callback_).Run();

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

ui::ModalType SecurePaymentConfirmationDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
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

const gfx::VectorIcon&
SecurePaymentConfirmationDialogView::GetFingerprintIcon() {
  return GetNativeTheme()->ShouldUseDarkColors() ? kWebauthnFingerprintDarkIcon
                                                 : kWebauthnFingerprintIcon;
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
// overlayed on the top of the header.
// +------------------------------------------+
// |===============progress bar===============|
// |                                          |
// |             fingerprint icon             |
// +------------------------------------------+
std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateHeaderView() {
  const int header_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  const gfx::Size header_size(header_width, kHeaderIconHeight);

  // The container view has no layout, so its preferred size is hardcoded to
  // match the size of the image, and the progress bar overlay is absolutely
  // positioned.
  auto header_view = std::make_unique<views::View>();
  header_view->SetPreferredSize(header_size);

  // Fingerprint header icon
  auto image_view = std::make_unique<NonAccessibleImageView>();
  gfx::IconDescription icon_description(GetFingerprintIcon());
  image_view->SetImage(gfx::CreateVectorIcon(icon_description));
  image_view->SetSize(header_size);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  image_view->SetID(static_cast<int>(DialogViewID::HEADER_ICON));
  header_view->AddChildView(image_view.release());

  // Progress bar
  auto progress_bar = std::make_unique<views::ProgressBar>(
      kProgressBarHeight, /*allow_round_corner=*/false);
  progress_bar->SetValue(-1);  // infinite animation.
  progress_bar->SetBackgroundColor(SK_ColorTRANSPARENT);
  progress_bar->SetPreferredSize(gfx::Size(header_width, kProgressBarHeight));
  progress_bar->SizeToPreferredSize();
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
  std::unique_ptr<views::Label> title_text = std::make_unique<views::Label>(
      model_->title(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_PRIMARY);
  title_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_text->SetLineHeight(kTitleLineHeight);
  title_text->SetBorder(views::CreateEmptyBorder(0, 0, kBodyInsets, 0));
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
  label_text->SetLineHeight(kRowViewLineHeight);
  label_text->SetID(static_cast<int>(label_id));
  layout->AddView(std::move(label_text));

  std::unique_ptr<views::Label> value_text = std::make_unique<views::Label>(
      value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  value_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  value_text->SetLineHeight(kRowViewLineHeight);
  value_text->SetID(static_cast<int>(value_id));
  layout->AddView(std::move(value_text));

  if (icon) {
    std::unique_ptr<views::ImageView> icon_view =
        std::make_unique<views::ImageView>();

    instrument_icon_ = model_->instrument_icon();
    instrument_icon_generation_id_ =
        model_->instrument_icon()->getGenerationID();
    gfx::ImageSkia image =
        gfx::ImageSkia::CreateFrom1xBitmap(*model_->instrument_icon())
            .DeepCopy();

    icon_view->SetImage(image);
    icon_view->SetImageSize(
        gfx::Size(kInstrumentIconWidth, kInstrumentIconHeight));
    icon_view->SetPaintToLayer();
    icon_view->layer()->SetFillsBoundsOpaquely(false);
    icon_view->SetID(static_cast<int>(icon_id));
    layout->AddView(std::move(icon_view));
  }

  return row;
}

}  // namespace payments
