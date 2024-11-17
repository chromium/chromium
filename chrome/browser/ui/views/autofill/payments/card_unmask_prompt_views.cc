// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/card_unmask_prompt_views.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace autofill {

CardUnmaskPromptViews::CardUnmaskPromptViews(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents->GetWeakPtr()) {
  UpdateButtons();

  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

CardUnmaskPromptViews::~CardUnmaskPromptViews() {
  if (controller_) {
    controller_->OnUnmaskDialogClosed();
  }
}

void CardUnmaskPromptViews::Show() {
  // Don't show the bubble if the web contents are or will soon be destroyed.
  // (e.g. when closing the platform authentication tab that usually triggers
  // the unmask flow as a fallback).
  if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
    delete this;
    return;
  }
  constrained_window::ShowWebModalDialogViews(this, web_contents_.get());
}

void CardUnmaskPromptViews::ControllerGone() {
  controller_ = nullptr;
  ClosePrompt();
}

void CardUnmaskPromptViews::DisableAndWaitForVerification() {
  SetInputsEnabled(false);
  controls_container_->SetVisible(false);
  overlay_->SetVisible(true);
  progress_throbber_->Start();
  UpdateButtons();
  DialogModelChanged();
  DeprecatedLayoutImmediately();
}

void CardUnmaskPromptViews::GotVerificationResult(
    const std::u16string& error_message,
    bool allow_retry) {
  progress_throbber_->Stop();
  if (error_message.empty()) {
    overlay_label_->SetText(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_SUCCESS));
    progress_throbber_->SetChecked(true);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CardUnmaskPromptViews::ClosePrompt,
                       weak_ptr_factory_.GetWeakPtr()),
        controller_->GetSuccessMessageDuration());
  } else {
    if (allow_retry) {
      controls_container_->SetVisible(true);
      overlay_->SetVisible(false);
      SetInputsEnabled(true);

      if (!controller_->ShouldRequestExpirationDate()) {
        // If there is more than one input showing, don't mark anything as
        // invalid since we don't know the location of the problem.
        cvc_input_->SetInvalid(true);

        // For non-virtual cards, show a "Update card" link that triggers the UI
        // to update the expiration date. This isn't relevant for virtual cards
        // since they never expire.
        if (!controller_->IsVirtualCard()) {
          ShowNewCardLink();
        }
      }

      // TODO(estade): When do we hide |error_label_|?
      SetRetriableErrorMessage(error_message);
    } else {
      SetRetriableErrorMessage(std::u16string());

      // Remove all child views. Since this is a permanent error we do not
      // intend to return to a previous state.
      // `RemoveAllChildViews()` destroys the views owned by the `overlay_`.
      // Prevent dangling pointers by setting pointers to the overlay's children
      // to null.
      overlay_label_ = nullptr;
      progress_throbber_ = nullptr;
      overlay_->RemoveAllChildViews();

      // Create and add the error icon.
      overlay_->AddChildView(
          std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
              kBrowserToolsErrorIcon, ui::kColorAlertHighSeverity)));

      // Create and add the label of the overlay, and show the error in gray.
      auto* error_label = overlay_->AddChildView(std::make_unique<views::Label>(
          error_message, views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY));
      error_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
      error_label->SetMultiLine(true);

      // Re-layout to correctly format the views on the overlay.
      overlay_->DeprecatedLayoutImmediately();

      // If it is a virtual card retrieval failure, we will need to update the
      // window title.
      GetWidget()->UpdateWindowTitle();
    }
    UpdateButtons();
    DialogModelChanged();
  }

  // Since we may have affected the layout of the button row, we retrigger a
  // layout of the whole dialog (contents and button row).
  InvalidateLayout();
  parent()->DeprecatedLayoutImmediately();
}

void CardUnmaskPromptViews::SetRetriableErrorMessage(
    const std::u16string& message) {
  error_label_->SetMultiLine(!message.empty());
  error_label_->SetText(message);
  temporary_error_->SetVisible(!message.empty());

  // Update the dialog's size.
  if (GetWidget() && web_contents_) {
    constrained_window::UpdateWebContentsModalDialogPosition(
        GetWidget(), web_modal::WebContentsModalDialogManager::FromWebContents(
                         web_contents_.get())
                         ->delegate()
                         ->GetWebContentsModalDialogHost());
  }

  DeprecatedLayoutImmediately();
}

void CardUnmaskPromptViews::SetInputsEnabled(bool enabled) {
  cvc_input_->SetEnabled(enabled);
  month_input_->SetEnabled(enabled);
  year_input_->SetEnabled(enabled);
}

void CardUnmaskPromptViews::ShowNewCardLink() {
  if (new_card_link_) {
    return;
  }

  auto new_card_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_NEW_CARD_LINK));
  new_card_link->SetCallback(base::BindRepeating(
      &CardUnmaskPromptViews::LinkClicked, base::Unretained(this)));
  new_card_link_ = input_row_->AddChildView(std::move(new_card_link));
}

views::View* CardUnmaskPromptViews::GetContentsView() {
  InitIfNecessary();
  return this;
}

void CardUnmaskPromptViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

std::u16string CardUnmaskPromptViews::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

bool CardUnmaskPromptViews::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  if (button == ui::mojom::DialogButton::kCancel) {
    return true;
  }

  DCHECK_EQ(ui::mojom::DialogButton::kOk, button);

  return cvc_input_->GetEnabled() &&
         controller_->InputCvcIsValid(cvc_input_->GetText()) &&
         ExpirationDateIsValid();
}

views::View* CardUnmaskPromptViews::GetInitiallyFocusedView() {
  return cvc_input_;
}

bool CardUnmaskPromptViews::ShouldShowCloseButton() const {
  return false;
}

bool CardUnmaskPromptViews::Cancel() {
  return true;
}

bool CardUnmaskPromptViews::Accept() {
  if (!controller_) {
    return true;
  }

  controller_->OnUnmaskPromptAccepted(
      cvc_input_->GetText(),
      month_input_->GetVisible() ? month_input_->GetTextForRow(
                                       month_input_->GetSelectedIndex().value())
                                 : std::u16string(),
      year_input_->GetVisible()
          ? year_input_->GetTextForRow(year_input_->GetSelectedIndex().value())
          : std::u16string(),
      /*enable_fido_auth=*/false,
      /*was_checkbox_visible=*/false);
  return false;
}

void CardUnmaskPromptViews::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  if (controller_->InputCvcIsValid(new_contents)) {
    cvc_input_->SetInvalid(false);
  }

  UpdateButtons();
  DialogModelChanged();
}

void CardUnmaskPromptViews::DateChanged() {
  if (ExpirationDateIsValid()) {
    if (month_input_->GetInvalid()) {
      month_input_->SetInvalid(false);
      year_input_->SetInvalid(false);
      SetRetriableErrorMessage(std::u16string());
    }
  } else if (month_input_->GetSelectedIndex() !=
                 month_combobox_model_.GetDefaultIndex().value() &&
             year_input_->GetSelectedIndex() !=
                 year_combobox_model_.GetDefaultIndex().value()) {
    month_input_->SetInvalid(true);
    year_input_->SetInvalid(true);
    SetRetriableErrorMessage(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_UNMASK_INVALID_EXPIRATION_DATE));
  }

  UpdateButtons();
  DialogModelChanged();
}

void CardUnmaskPromptViews::InitIfNecessary() {
  if (!children().empty()) {
    return;
  }
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  // The layout is a FillLayout that will contain the progress or error overlay
  // on top of the actual contents in |controls_container| (instructions, input
  // fields).
  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Inset the whole main section.
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));

  auto controls_container = std::make_unique<views::View>();
  controls_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  controls_container_ = AddChildView(std::move(controls_container));

  // Instruction text of the dialog.
  auto instructions = std::make_unique<views::Label>(
      controller_->GetInstructionsMessage(),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  instructions->SetMultiLine(true);
  instructions->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  instructions_ = controls_container_->AddChildView(std::move(instructions));

  // The input container is a vertical box layout containing the input row and
  // the temporary error label. They are separated by a related distance.
  auto input_container = std::make_unique<views::View>();
  input_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  // Input row, containing month/year dropdowns if needed and the CVC field.
  auto input_row = std::make_unique<views::View>();
  input_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL)));

  // Add the month and year comboboxes if the expiration date is needed.
  auto month_input = std::make_unique<views::Combobox>(&month_combobox_model_);
  month_input->SetCallback(base::BindRepeating(
      &CardUnmaskPromptViews::DateChanged, base::Unretained(this)));
  month_input->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_EXPIRATION_MONTH));
  month_input_ = input_row->AddChildView(std::move(month_input));
  auto year_input = std::make_unique<views::Combobox>(&year_combobox_model_);
  year_input->SetCallback(base::BindRepeating(
      &CardUnmaskPromptViews::DateChanged, base::Unretained(this)));
  year_input->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_EXPIRATION_YEAR));
  year_input_ = input_row->AddChildView(std::move(year_input));
  if (!controller_->ShouldRequestExpirationDate()) {
    month_input_->SetVisible(false);
    year_input_->SetVisible(false);
  }

  std::unique_ptr<views::Textfield> cvc_input =
      std::make_unique<views::Textfield>();
  // Only put a placeholder text if there is no challenge option present. A
  // challenge option being present indicates we are unmasking a virtual card
  // CVC.
  if (!controller_->IsChallengeOptionPresent()) {
    cvc_input->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC));
  }
  cvc_input->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DIALOG_ACCESSIBLE_NAME_SECURITY_CODE));
  cvc_input->SetDefaultWidthInChars(8);
  cvc_input->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  cvc_input->set_controller(this);
  cvc_input_ = input_row->AddChildView(std::move(cvc_input));

  auto cvc_image = std::make_unique<views::ImageView>();
  cvc_image->SetImage(
      ui::ImageModel::FromResourceId(controller_->GetCvcImageRid()));
  cvc_image->SetTooltipText(
      l10n_util::GetStringUTF16(controller_->GetCvcTooltipResourceId()));
  input_row->AddChildView(std::move(cvc_image));
  input_row_ = input_container->AddChildView(std::move(input_row));

  // Temporary error view, just below the input field(s).
  auto temporary_error = std::make_unique<views::View>();
  auto* temporary_error_layout =
      temporary_error->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
  temporary_error_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  temporary_error->SetVisible(false);
  temporary_error->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorIcon, ui::kColorAlertHighSeverity,
          gfx::GetDefaultSizeOfVectorIcon(vector_icons::kErrorIcon))));

  auto error_label = std::make_unique<views::Label>(
      std::u16string(), ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      STYLE_RED);
  error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  error_label_ = temporary_error->AddChildView(std::move(error_label));
  temporary_error_layout->SetFlexForView(error_label_, 1);
  temporary_error_ = input_container->AddChildView(std::move(temporary_error));

  controls_container_->AddChildView(std::move(input_container));

  // On top of the main contents, we add the progress/error overlay and hide it.
  // A child view will be added to it when about to be shown.
  overlay_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_LABEL_HORIZONTAL))
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetVisible(false)
          .SetBackground(
              views::CreateThemedSolidBackground(ui::kColorDialogBackground))
          .AddChildren(
              views::Builder<views::Throbber>().CopyAddressTo(
                  &progress_throbber_),
              views::Builder<views::Label>()
                  .CopyAddressTo(&overlay_label_)
                  .SetBackgroundColorId(ui::kColorDialogBackground)
                  .SetEnabledColorId(ui::kColorThrobber)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_IN_PROGRESS)))
          .Build());
}

bool CardUnmaskPromptViews::ExpirationDateIsValid() const {
  if (!controller_->ShouldRequestExpirationDate()) {
    return true;
  }

  return controller_->InputExpirationIsValid(
      month_input_->GetTextForRow(month_input_->GetSelectedIndex().value()),
      year_input_->GetTextForRow(year_input_->GetSelectedIndex().value()));
}

void CardUnmaskPromptViews::ClosePrompt() {
  GetWidget()->Close();
}

void CardUnmaskPromptViews::UpdateButtons() {
  // In permanent error state, only the "close" button is shown.
  using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
  PaymentsRpcResult result = controller_->GetVerificationResult();
  bool has_ok = result != PaymentsRpcResult::kPermanentFailure &&
                result != PaymentsRpcResult::kNetworkError &&
                result != PaymentsRpcResult::kVcnRetrievalPermanentFailure &&
                result != PaymentsRpcResult::kVcnRetrievalTryAgainFailure;

  SetButtons(has_ok ? static_cast<int>(ui::mojom::DialogButton::kOk) |
                          static_cast<int>(ui::mojom::DialogButton::kCancel)
                    : static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk, controller_->GetOkButtonLabel());
}

void CardUnmaskPromptViews::LinkClicked() {
  controller_->NewCardLinkClicked();
  for (views::View* child : input_row_->children()) {
    child->SetVisible(true);
  }

  new_card_link_->SetVisible(false);
  input_row_->InvalidateLayout();
  cvc_input_->SetInvalid(false);
  cvc_input_->SetText(std::u16string());
  UpdateButtons();
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();
  instructions_->SetText(controller_->GetInstructionsMessage());
  SetRetriableErrorMessage(std::u16string());
}

CardUnmaskPromptView* CreateCardUnmaskPromptView(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents) {
  return new CardUnmaskPromptViews(controller, web_contents);
}

BEGIN_METADATA(CardUnmaskPromptViews)
END_METADATA

}  // namespace autofill
