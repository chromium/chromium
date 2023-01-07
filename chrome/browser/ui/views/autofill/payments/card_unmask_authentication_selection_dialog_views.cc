// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/card_unmask_authentication_selection_dialog_views.h"

#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"

namespace autofill {

CardUnmaskAuthenticationSelectionDialogViews::
    CardUnmaskAuthenticationSelectionDialogViews(
        CardUnmaskAuthenticationSelectionDialogController* controller)
    : controller_(controller) {
  SetShowTitle(true);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller_->GetOkButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  InitViews();
}

CardUnmaskAuthenticationSelectionDialogViews::
    ~CardUnmaskAuthenticationSelectionDialogViews() {
  // Inform |controller_| of the dialog's destruction. By the time this is
  // called, the |controller_| will not be nullptr only if the dialog is closed
  // by the user. For other cases, the |controller_| should already be reset.
  if (controller_) {
    controller_->OnDialogClosed(/*user_closed_dialog=*/true,
                                /*server_success=*/false);
    controller_ = nullptr;
  }
}

// static
CardUnmaskAuthenticationSelectionDialogView*
CardUnmaskAuthenticationSelectionDialogView::CreateAndShow(
    CardUnmaskAuthenticationSelectionDialogController* controller,
    content::WebContents* web_contents) {
  CardUnmaskAuthenticationSelectionDialogViews* dialog_view =
      new CardUnmaskAuthenticationSelectionDialogViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  return dialog_view;
}

void CardUnmaskAuthenticationSelectionDialogViews::Dismiss(
    bool user_closed_dialog,
    bool server_success) {
  if (controller_) {
    controller_->OnDialogClosed(user_closed_dialog, server_success);
    controller_ = nullptr;
  }
  GetWidget()->Close();
}

bool CardUnmaskAuthenticationSelectionDialogViews::Accept() {
  ReplaceContentWithProgressThrobber();
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, false);
  DCHECK(!controller_->GetChallengeOptions().empty());
  controller_->OnOkButtonClicked(controller_->GetChallengeOptions()[0].id);
  return false;
}

std::u16string CardUnmaskAuthenticationSelectionDialogViews::GetWindowTitle()
    const {
  return controller_->GetWindowTitle();
}

void CardUnmaskAuthenticationSelectionDialogViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(
          GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

void CardUnmaskAuthenticationSelectionDialogViews::InitViews() {
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

void CardUnmaskAuthenticationSelectionDialogViews::AddHeaderText() {
  auto* content = AddChildView(std::make_unique<views::Label>(
      controller_->GetContentHeaderText(),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  content->SetMultiLine(true);
  content->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void CardUnmaskAuthenticationSelectionDialogViews::AddChallengeOptionsViews() {
  for (const CardUnmaskChallengeOption& challenge_option :
       controller_->GetChallengeOptions()) {
    // Initializes the current challenge option.
    auto* challenge_option_container =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    challenge_option_container->SetOrientation(
        views::BoxLayout::Orientation::kHorizontal);
    challenge_option_container->SetBetweenChildSpacing(
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL));

    // Creates the left side image of the challenge option and adds it to the
    // current challenge option.
    challenge_option_container->AddChildView(std::make_unique<views::ImageView>(
        controller_->GetAuthenticationModeIcon(challenge_option)));

    // Creates the right side of the challenge option (label and information
    // such as masked phone number, masked email, etc...) and adds it to the
    // current challenge option.
    auto* challenge_option_details = challenge_option_container->AddChildView(
        std::make_unique<views::BoxLayoutView>());
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
}

void CardUnmaskAuthenticationSelectionDialogViews::AddFooterText() {
  auto* content = AddChildView(std::make_unique<views::Label>(
      controller_->GetContentFooterText(),
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY));
  content->SetMultiLine(true);
  content->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void CardUnmaskAuthenticationSelectionDialogViews::
    ReplaceContentWithProgressThrobber() {
  RemoveAllChildViews();
  AddChildView(std::make_unique<ProgressBarWithTextView>(
      controller_->GetProgressLabel()));
}

}  // namespace autofill
