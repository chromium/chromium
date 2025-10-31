// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_and_fill_dialog.h"

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {
// Add a top inset for the CVC icon so that when the icon is center aligned
// vertically, it is aligned with other elements in the same row.
constexpr int kCvcIconTopInsetDp = 4;
}  // namespace

SaveAndFillDialog::SaveAndFillDialog(
    base::WeakPtr<SaveAndFillDialogController> controller,
    base::RepeatingCallback<void(const GURL&)> on_legal_message_link_clicked)
    : controller_(controller),
      on_legal_message_link_clicked_(on_legal_message_link_clicked) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following line once this is the
  // default state for widgets.
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetModalType(ui::mojom::ModalType::kChild);
  SetAcceptCallbackWithClose(base::BindRepeating(&SaveAndFillDialog::OnAccepted,
                                                 base::Unretained(this)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_->GetAcceptButtonText());
  SetShowCloseButton(false);
  InitViews();
}

SaveAndFillDialog::~SaveAndFillDialog() = default;

void SaveAndFillDialog::AddedToWidget() {
  GetWidget()->MakeCloseSynchronous(base::BindOnce(
      &SaveAndFillDialog::OnDialogClosed, base::Unretained(this)));

  focus_manager_ = GetFocusManager();
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  }

  if (controller_->GetDialogState() == SaveAndFillDialogState::kUploadDialog) {
    GetBubbleFrameView()->SetTitleView(
        std::make_unique<TitleWithIconAfterLabelView>(
            GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
  } else {
    auto title_view = std::make_unique<views::Label>(
        GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
    title_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    title_view->SetMultiLine(true);
    GetBubbleFrameView()->SetTitleView(std::move(title_view));
  }
  SetAccessibleTitle(GetWindowTitle());
}

void SaveAndFillDialog::RemovedFromWidget() {
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
  }
}

void SaveAndFillDialog::OnWidgetInitialized() {
  views::DialogDelegateView::OnWidgetInitialized();
  card_number_data_.GetInputTextField().RequestFocus();
}

std::u16string SaveAndFillDialog::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void SaveAndFillDialog::ContentsChanged(views::Textfield* sender,
                                        const std::u16string& new_contents) {
  if (sender == &card_number_data_.GetInputTextField()) {
    card_number_data_.SetErrorState(
        /*is_valid=*/controller_->IsValidCreditCardNumber(new_contents));
  } else if (sender == &cvc_data_.GetInputTextField()) {
    cvc_data_.SetErrorState(
        /*is_valid=*/controller_->IsValidCvc(new_contents));
  } else if (sender == &name_on_card_data_.GetInputTextField()) {
    name_on_card_data_.SetErrorState(
        /*is_valid=*/controller_->IsValidNameOnCard(new_contents));
  } else if (sender == &expiration_date_data_.GetInputTextField()) {
    size_t new_cursor_position;

    std::u16string formatted_input = controller_->FormatExpirationDateInput(
        /*input=*/new_contents,
        /*old_cursor_position=*/sender->GetCursorPosition(),
        /*new_cursor_position=*/new_cursor_position);

    // Only update the textfield and cursor if the formatting resulted in a
    // change.
    if (new_contents != formatted_input) {
      sender->SetText(formatted_input);
      sender->SelectSelectionModel(
          gfx::SelectionModel(new_cursor_position, gfx::CURSOR_FORWARD));
    }
    expiration_date_data_.SetErrorState(
        /*is_valid=*/controller_->IsValidExpirationDate(formatted_input));
  }
  // Enable the save button iff all textfields are valid.
  SetButtonEnabled(ui::mojom::DialogButton::kOk,
                   card_number_data_.is_valid_input &&
                       cvc_data_.is_valid_input &&
                       expiration_date_data_.is_valid_input &&
                       name_on_card_data_.is_valid_input);
}

void SaveAndFillDialog::OnDidChangeFocus(views::View* before,
                                         views::View* now) {
  if (before == &card_number_data_.GetInputTextField()) {
    card_number_data_.GetInputTextField().SetText(
        GetFormattedCardNumberForDisplay(
            card_number_data_.GetInputTextField().GetText()));
    card_number_data_.MaybeAnnounceError();
  } else if (before == &cvc_data_.GetInputTextField()) {
    cvc_data_.MaybeAnnounceError();
  } else if (before == &name_on_card_data_.GetInputTextField()) {
    name_on_card_data_.MaybeAnnounceError();
  } else if (before == &expiration_date_data_.GetInputTextField()) {
    expiration_date_data_.MaybeAnnounceError();
  }
}

void SaveAndFillDialog::InitViews() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  container_view_ = AddChildView(std::make_unique<views::View>());
  container_view_->SetUseDefaultFillLayout(true);

  CreateMainContentView();
  CreatePendingView();

  if (controller_->GetDialogState() == SaveAndFillDialogState::kPendingDialog) {
    ToggleThrobberVisibility(/*visible=*/true);
  } else if (controller_->GetDialogState() ==
             SaveAndFillDialogState::kUploadDialog) {
    ToggleThrobberVisibility(/*visible=*/false);
  }
}

void SaveAndFillDialog::CreateMainContentView() {
  container_view_->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&main_view_)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_CONTROL_VERTICAL))
          .Build());
  main_view_->AddChildView(
      views::Builder<views::Label>()
          .SetText(controller_->GetExplanatoryMessage())
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets().set_bottom(
                           views::LayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_UNRELATED_CONTROL_VERTICAL)))
          .Build());

  card_number_data_ = CreateLabelAndTextfieldView(
      /*label_text=*/controller_->GetCardNumberLabel(),
      /*error_message=*/controller_->GetInvalidCardNumberErrorMessage());
  card_number_data_.GetInputTextField().SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  card_number_data_.GetInputTextField().SetController(this);
  main_view_->AddChildView(std::move(card_number_data_.container));

  expiration_date_data_ = CreateLabelAndTextfieldView(
      /*label_text=*/controller_->GetExpirationDateLabel(),
      /*error_message=*/controller_->GetInvalidExpirationDateErrorMessage());
  expiration_date_data_.GetInputTextField().SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_DATE);
  expiration_date_data_.GetInputTextField().SetController(this);
  expiration_date_data_.GetInputTextField().SetPlaceholderText(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_EXPIRATION_DATE_PLACEHOLDER));
  expiration_date_data_.GetInputTextField().SetDefaultWidthInChars(18);

  cvc_data_ = CreateLabelAndTextfieldView(
      /*label_text=*/controller_->GetCvcLabel(),
      /*error_message=*/controller_->GetInvalidCvcErrorMessage());
  cvc_data_.GetInputTextField().SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  cvc_data_.GetInputTextField().SetController(this);
  cvc_data_.GetInputTextField().SetPlaceholderText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_AND_FILL_DIALOG_CVC_PLACEHOLDER));
  cvc_data_.GetInputTextField().SetDefaultWidthInChars(18);
  // CVC is an optional field, so it is considered valid by default when the
  // dialog first appears.
  cvc_data_.SetErrorState(/*is_valid=*/true);

  // Create the horizontal row for expiration date, cvc, and icon.
  main_view_->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
          .AddChild(views::Builder<views::View>(
              std::move(expiration_date_data_.container)))
          .AddChild(views::Builder<views::View>(std::move(cvc_data_.container)))
          .AddChild(
              views::Builder<views::ImageView>()
                  .SetImage(ui::ImageModel::FromImage(
                      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                          IDR_CREDIT_CARD_CVC_HINT_BACK)))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets().set_top(kCvcIconTopInsetDp)))
          .Build());

  name_on_card_data_ = CreateLabelAndTextfieldView(
      /*label_text=*/controller_->GetNameOnCardLabel(),
      /*error_message=*/controller_->GetInvalidNameOnCardErrorMessage());
  name_on_card_data_.GetInputTextField().SetController(this);
  main_view_->AddChildView(std::move(name_on_card_data_.container));

  if (controller_->GetDialogState() == SaveAndFillDialogState::kUploadDialog) {
    main_view_->AddChildView(CreateLegalMessageView());
  }
}

void SaveAndFillDialog::CreatePendingView() {
  container_view_->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&pending_view_)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .AddChild(
              views::Builder<views::Throbber>(
                  std::make_unique<views::Throbber>(kDialogThrobberDiameter))
                  .CopyAddressTo(&throbber_))
          .SetVisible(false)
          .Build());
}

void SaveAndFillDialog::ToggleThrobberVisibility(bool visible) {
  if (visible) {
    throbber_->Start();
    throbber_->GetViewAccessibility().AnnouncePolitely(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_PENDING_DIALOG_LOADING_ACCESSIBILITY_DESCRIPTION));
    SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  } else {
    throbber_->Stop();
  }
  main_view_->SetVisible(!visible);
  pending_view_->SetVisible(visible);
}

payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails
SaveAndFillDialog::GetUserProvidedDataFromInput() const {
  payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails
      user_provided_card_details;

  user_provided_card_details.card_number =
      card_number_data_.GetInputTextField().GetText();
  user_provided_card_details.cardholder_name =
      name_on_card_data_.GetInputTextField().GetText();
  user_provided_card_details.security_code =
      cvc_data_.GetInputTextField().GetText();

  const std::u16string exp_text =
      std::u16string(expiration_date_data_.GetInputTextField().GetText());
  size_t slash_pos = exp_text.find(u'/');
  // This should never happen, as the expiration date field is validated and
  // formatted before the Save button is enabled.
  CHECK(slash_pos != std::u16string::npos && slash_pos > 0 &&
        slash_pos < exp_text.length() - 1);

  user_provided_card_details.expiration_date_month =
      exp_text.substr(0, slash_pos);
  user_provided_card_details.expiration_date_year =
      exp_text.substr(slash_pos + 1);

  return user_provided_card_details;
}

void SaveAndFillDialog::OnDialogClosed(views::Widget::ClosedReason reason) {
  CHECK_NE(reason, views::Widget::ClosedReason::kAcceptButtonClicked);
  if (reason == views::Widget::ClosedReason::kCancelButtonClicked) {
    controller_->OnUserCanceledDialog();
  } else {
    controller_->Dismiss();
  }
}

bool SaveAndFillDialog::OnAccepted() {
  ToggleThrobberVisibility(/*visible=*/true);
  controller_->OnUserAcceptedDialog(GetUserProvidedDataFromInput());
  // Return false to prevent the dialog from closing. The controller is now
  // responsible for closing it after the server call is complete.
  return false;
}

std::unique_ptr<views::View> SaveAndFillDialog::CreateLegalMessageView() {
  const LegalMessageLines& message_lines = controller_->GetLegalMessageLines();

  if (message_lines.empty()) {
    return nullptr;
  }

  return autofill::CreateLegalMessageView(
      message_lines, std::u16string(), ui::ImageModel(),
      base::BindRepeating(on_legal_message_link_clicked_));
}

}  // namespace autofill
