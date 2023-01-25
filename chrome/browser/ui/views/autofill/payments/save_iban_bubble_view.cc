// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"

#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill {

SaveIbanBubbleView::SaveIbanBubbleView(views::View* anchor_view,
                                       content::WebContents* web_contents,
                                       SaveIbanBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller->GetAcceptButtonText());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller->GetDeclineButtonText());
  SetCancelCallback(base::BindOnce(&SaveIbanBubbleView::OnDialogCancelled,
                                   base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(&SaveIbanBubbleView::OnDialogAccepted,
                                   base::Unretained(this)));

  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void SaveIbanBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
  AssignIdsToDialogButtonsForTesting();  // IN-TEST
}

std::u16string SaveIbanBubbleView::GetIBANIdentifierString() {
  return controller_->GetIBAN().GetIdentifierStringForAutofillDisplay();
}

void SaveIbanBubbleView::Hide() {
  CloseBubble();

  // If `controller_` is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out `controller_`'s reference to `this`. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveIbanBubbleView::AddedToWidget() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  GetBubbleFrameView()->SetHeaderView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          *bundle.GetImageSkiaNamed(IDR_SAVE_CARD_SECURELY),
          *bundle.GetImageSkiaNamed(IDR_SAVE_CARD_SECURELY_DARK),
          base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                              base::Unretained(this))));

  auto title_label = std::make_unique<views::Label>(
      GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);

  // TODO(crbug.com/1352606): Add Chrome icon and separator.
  GetBubbleFrameView()->SetTitleView(std::move(title_label));
}

std::u16string SaveIbanBubbleView::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void SaveIbanBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

SaveIbanBubbleView::~SaveIbanBubbleView() = default;

void SaveIbanBubbleView::CreateMainContentView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();

  auto* iban_view = AddChildView(std::make_unique<views::BoxLayoutView>());
  iban_view->SetID(DialogViewId::MAIN_CONTENT_VIEW_LOCAL);
  views::TableLayout* layout =
      iban_view->SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(
          views::TableLayout::kFixedSize,
          provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, 1.0,
                 views::TableLayout::ColumnSize::kFixed, 0, 0)
      // Add a row for IBAN label and the value of IBAN.
      .AddRows(1, views::TableLayout::kFixedSize)
      .AddPaddingRow(views::TableLayout::kFixedSize,
                     ChromeLayoutProvider::Get()->GetDistanceMetric(
                         DISTANCE_CONTROL_LIST_VERTICAL))
      // Add a row for nickname label and the input text field.
      .AddRows(1, views::TableLayout::kFixedSize);

  iban_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  iban_view
      ->AddChildView(std::make_unique<views::Label>(
          GetIBANIdentifierString(), views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY))
      ->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  iban_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_NICKNAME),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  nickname_textfield_ =
      iban_view->AddChildView(std::make_unique<views::Textfield>());
  nickname_textfield_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PROMPT_NICKNAME));
  nickname_textfield_->SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_TEXT);
  nickname_textfield_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_IBAN_PLACEHOLDER));
}

void SaveIbanBubbleView::AssignIdsToDialogButtonsForTesting() {
  auto* ok_button = GetOkButton();
  if (ok_button) {
    ok_button->SetID(DialogViewId::OK_BUTTON);
  }
  auto* cancel_button = GetCancelButton();
  if (cancel_button) {
    cancel_button->SetID(DialogViewId::CANCEL_BUTTON);
  }
}

void SaveIbanBubbleView::OnDialogAccepted() {
  if (controller_) {
    DCHECK(nickname_textfield_);
    controller_->OnSaveButton(nickname_textfield_->GetText());
  }
}

void SaveIbanBubbleView::OnDialogCancelled() {
  if (controller_) {
    controller_->OnCancelButton();
  }
}

void SaveIbanBubbleView::Init() {
  CreateMainContentView();
}

}  // namespace autofill
