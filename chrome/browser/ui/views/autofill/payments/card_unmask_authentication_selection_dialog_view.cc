// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/card_unmask_authentication_selection_dialog_view.h"

#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

ui::ImageModel GetAuthenticationModeIcon(
    const CardUnmaskChallengeOption& challenge_option) {
  switch (challenge_option.type) {
    case CardUnmaskChallengeOptionType::kSmsOtp:
      return ui::ImageModel::FromVectorIcon(vector_icons::kSmsIcon);
    case CardUnmaskChallengeOptionType::kEmailOtp:
      return ui::ImageModel::FromVectorIcon(vector_icons::kEmailOutlineIcon);
    case CardUnmaskChallengeOptionType::kCvc:
      // CVC auth has its own authentication dialog in the single challenge
      // option case.
    case CardUnmaskChallengeOptionType::kThreeDomainSecure:
      // 3DS auth has its own authentication dialog in the single challenge
      // option case.
    case CardUnmaskChallengeOptionType::kUnknownType:
      break;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

CardUnmaskAuthenticationSelectionDialogView::
    CardUnmaskAuthenticationSelectionDialogView(
        CardUnmaskAuthenticationSelectionDialogController* controller)
    : controller_(controller) {
  SetShowTitle(true);

  // Set the cancel button label now because it is constant throughout the
  // view's lifecycle. The ok button label will be set later once we have more
  // details on the challenge options that will be displayed in the dialog,
  // since the label can change based on the challenge options.
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 GetDialogButtonLabel(ui::mojom::DialogButton::kCancel));

  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  InitViews();
}

CardUnmaskAuthenticationSelectionDialogView::
    ~CardUnmaskAuthenticationSelectionDialogView() {
  // Inform |controller_| of the dialog's destruction. By the time this is
  // called, the |controller_| will not be nullptr only if the dialog is closed
  // by the user. For other cases, the |controller_| should already be reset.
  if (controller_) {
    controller_->OnDialogClosed(/*user_closed_dialog=*/true,
                                /*server_success=*/false);
    controller_ = nullptr;
  }
}

void CardUnmaskAuthenticationSelectionDialogView::Dismiss(
    bool user_closed_dialog,
    bool server_success) {
  if (controller_) {
    controller_->OnDialogClosed(user_closed_dialog, server_success);
    controller_ = nullptr;
  }
  GetWidget()->Close();
}

void CardUnmaskAuthenticationSelectionDialogView::UpdateContent() {
  ReplaceContentWithProgressThrobber();
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
}

bool CardUnmaskAuthenticationSelectionDialogView::Accept() {
  DCHECK(!controller_->GetChallengeOptions().empty());
  controller_->OnOkButtonClicked();
  return false;
}

std::u16string CardUnmaskAuthenticationSelectionDialogView::GetWindowTitle()
    const {
  return controller_->GetWindowTitle();
}

void CardUnmaskAuthenticationSelectionDialogView::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

void CardUnmaskAuthenticationSelectionDialogView::InitViews() {
  DCHECK(children().empty());
  // Sets the layout manager for the top level view.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  // Adds the header.
  AddHeaderText();
  // Adds the list of challenge options.
  AddChallengeOptionsViews();
  // Adds the footer.
  AddFooterText();
}

void CardUnmaskAuthenticationSelectionDialogView::AddHeaderText() {
  auto* content = AddChildView(std::make_unique<views::Label>(
      controller_->GetContentHeaderText(),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  content->SetMultiLine(true);
  content->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void CardUnmaskAuthenticationSelectionDialogView::AddChallengeOptionsViews() {
  auto* challenge_options_section =
      AddChildView(std::make_unique<views::View>());
  int horizontal_column_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  challenge_options_section
      ->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddPaddingColumn(views::TableLayout::kFixedSize,
                         horizontal_column_padding)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        horizontal_column_padding)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  const std::vector<CardUnmaskChallengeOption>& challenge_options =
      controller_->GetChallengeOptions();

  static_cast<views::TableLayout*>(
      challenge_options_section->GetLayoutManager())
      ->AddRows(1, views::TableLayout::kFixedSize);
  if (challenge_options.size() > 1) {
    for (auto it = challenge_options.begin(); it != challenge_options.end();
         it++) {
      // When there are multiple challenge options, provide the user with
      // radio buttons to select.
      auto* challenge_option_radio_button =
          challenge_options_section->AddChildView(
              CreateChallengeOptionRadioButton(*it));

      // Set the first challenge option radio button to be automatically
      // checked.
      if (it == challenge_options.begin()) {
        challenge_option_radio_button->SetChecked(true);
      }

      // Only add padding and another row if it isn't the last challenge option
      // to display.
      if (std::next(it) != challenge_options.end()) {
        static_cast<views::TableLayout*>(
            challenge_options_section->GetLayoutManager())
            ->AddPaddingRow(views::TableLayout::kFixedSize,
                            ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_UNRELATED_CONTROL_VERTICAL))
            .AddRows(1, views::TableLayout::kFixedSize);
      }

      AddChallengeOptionDetails(*it, challenge_options_section);
    }
  } else {
    // Instead of a radio button, create the left side image of the
    // challenge option.
    challenge_options_section->AddChildView(std::make_unique<views::ImageView>(
        GetAuthenticationModeIcon(challenge_options[0])));

    // Since there's only one challenge option, the selected challenge
    // option id will always be the first one.
    OnChallengeOptionSelected(challenge_options[0].id);

    AddChallengeOptionDetails(challenge_options[0], challenge_options_section);
  }
}

void CardUnmaskAuthenticationSelectionDialogView::AddChallengeOptionDetails(
    const CardUnmaskChallengeOption& challenge_option,
    views::View* challenge_options_section) {
  // Creates the right side of the challenge option (label and information
  // such as masked phone number, masked email, etc...) and adds it to the
  // current challenge option.
  auto* challenge_option_details = challenge_options_section->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  challenge_option_details->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  challenge_option_details->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  challenge_option_details->AddChildView(std::make_unique<views::Label>(
      controller_->GetAuthenticationModeLabel(challenge_option),
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_PRIMARY));
  challenge_option_details->AddChildView(std::make_unique<views::Label>(
      challenge_option.challenge_info,
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY));
}

void CardUnmaskAuthenticationSelectionDialogView::AddFooterText() {
  auto* content = AddChildView(std::make_unique<views::Label>(
      controller_->GetContentFooterText(),
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY));
  content->SetMultiLine(true);
  content->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void CardUnmaskAuthenticationSelectionDialogView::
    ReplaceContentWithProgressThrobber() {
  RemoveAllChildViews();
  AddChildView(std::make_unique<ProgressBarWithTextView>(
      controller_->GetProgressLabel()));
}

void CardUnmaskAuthenticationSelectionDialogView::OnChallengeOptionSelected(
    const CardUnmaskChallengeOption::ChallengeOptionId&
        selected_challenge_option_id) {
  controller_->SetSelectedChallengeOptionId(selected_challenge_option_id);
  SetButtonLabel(ui::mojom::DialogButton::kOk, controller_->GetOkButtonLabel());
}

std::unique_ptr<views::RadioButton>
CardUnmaskAuthenticationSelectionDialogView::CreateChallengeOptionRadioButton(
    CardUnmaskChallengeOption challenge_option) {
  auto radio_button = std::make_unique<views::RadioButton>();
  radio_button_checked_changed_subscriptions_.push_back(
      radio_button->AddCheckedChangedCallback(base::BindRepeating(
          &CardUnmaskAuthenticationSelectionDialogView::
              OnChallengeOptionSelected,
          weak_ptr_factory_.GetWeakPtr(), challenge_option.id)));
  radio_button->GetViewAccessibility().SetName(
      controller_->GetAuthenticationModeLabel(challenge_option) + u". " +
      challenge_option.challenge_info);
  return radio_button;
}

CardUnmaskAuthenticationSelectionDialog*
CreateAndShowCardUnmaskAuthenticationSelectionDialog(
    content::WebContents* web_contents,
    CardUnmaskAuthenticationSelectionDialogController* controller) {
  CardUnmaskAuthenticationSelectionDialogView* dialog_view =
      new CardUnmaskAuthenticationSelectionDialogView(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  return dialog_view;
}

}  // namespace autofill
