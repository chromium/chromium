// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/card_unmask_prompt_views.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

static views::GridLayout* ResetOverlayLayout(views::View* overlay) {
  views::GridLayout* overlay_layout =
      overlay->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = overlay_layout->AddColumnSet(0);
  // The throbber's checkmark is 18dp.
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0.5, views::GridLayout::FIXED, 18, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                            ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0.5,
                     views::GridLayout::USE_PREF, 0, 0);
  overlay_layout->StartRow(1.0, 0);
  return overlay_layout;
}

std::unique_ptr<views::Checkbox> CreateSaveCheckbox(bool start_state) {
  auto storage_checkbox =
      std::make_unique<views::Checkbox>(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_STORAGE_CHECKBOX));
  storage_checkbox->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  storage_checkbox->SetChecked(start_state);
  storage_checkbox->SetEnabledTextColors(views::style::GetColor(
      *storage_checkbox.get(), ChromeTextContext::CONTEXT_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY));

  return storage_checkbox;
}

}  // namespace

CardUnmaskPromptViews::CardUnmaskPromptViews(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::CARD_UNMASK);
  if (controller_->CanStoreLocally()) {
    storage_checkbox_ = DialogDelegate::SetFootnoteView(
        CreateSaveCheckbox(controller_->GetStoreLocallyStartState()));
  }

  UpdateButtonLabels();
}

CardUnmaskPromptViews::~CardUnmaskPromptViews() {
  if (controller_)
    controller_->OnUnmaskDialogClosed();
}

void CardUnmaskPromptViews::Show() {
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
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
  UpdateButtonLabels();
  DialogModelChanged();
  Layout();
}

void CardUnmaskPromptViews::GotVerificationResult(
    const base::string16& error_message,
    bool allow_retry) {
  progress_throbber_->Stop();
  if (error_message.empty()) {
    overlay_label_->SetText(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_SUCCESS));
    progress_throbber_->SetChecked(true);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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

        // Show a "New card?" link, which when clicked will cause us to ask
        // for expiration date.
        ShowNewCardLink();
      }

      // TODO(estade): When do we hide |error_label_|?
      SetRetriableErrorMessage(error_message);
    } else {
      SetRetriableErrorMessage(base::string16());

      // Rows cannot be replaced in GridLayout, so we reset it.
      overlay_->RemoveAllChildViews(/*delete_children=*/true);
      views::GridLayout* layout = ResetOverlayLayout(overlay_);

      // The label of the overlay will now show the error in red.
      auto error_label = std::make_unique<views::Label>(error_message);
      const SkColor warning_text_color = views::style::GetColor(
          *error_label, ChromeTextContext::CONTEXT_BODY_TEXT_SMALL, STYLE_RED);
      error_label->SetEnabledColor(warning_text_color);
      error_label->SetMultiLine(true);

      // Replace the throbber with a warning icon. Since this is a permanent
      // error we do not intend to return to a previous state.
      auto error_icon = std::make_unique<views::ImageView>();
      error_icon->SetImage(gfx::CreateVectorIcon(
          kBrowserToolsErrorIcon,
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_AlertSeverityHigh)));

      layout->StartRow(1.0, 0);
      layout->AddView(std::move(error_icon));
      layout->AddView(std::move(error_label));
    }
    UpdateButtonLabels();
    DialogModelChanged();
  }

  // Since we may have affected the layout of the button row, we retrigger a
  // layout of the whole dialog (contents and button row).
  InvalidateLayout();
  parent()->Layout();
}

void CardUnmaskPromptViews::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(source, new_card_link_);
  controller_->NewCardLinkClicked();
  for (views::View* child : input_row_->children())
    child->SetVisible(true);

  new_card_link_->SetVisible(false);
  input_row_->InvalidateLayout();
  cvc_input_->SetInvalid(false);
  cvc_input_->SetText(base::string16());
  UpdateButtonLabels();
  DialogModelChanged();
  GetWidget()->UpdateWindowTitle();
  instructions_->SetText(controller_->GetInstructionsMessage());
  SetRetriableErrorMessage(base::string16());
}

void CardUnmaskPromptViews::SetRetriableErrorMessage(
    const base::string16& message) {
  error_label_->SetMultiLine(!message.empty());
  error_label_->SetText(message);
  temporary_error_->SetVisible(!message.empty());

  // Update the dialog's size.
  if (GetWidget() && web_contents_) {
    constrained_window::UpdateWebContentsModalDialogPosition(
        GetWidget(),
        web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_)
            ->delegate()
            ->GetWebContentsModalDialogHost());
  }

  Layout();
}

void CardUnmaskPromptViews::SetInputsEnabled(bool enabled) {
  cvc_input_->SetEnabled(enabled);
  if (storage_checkbox_)
    storage_checkbox_->SetEnabled(enabled);
  month_input_->SetEnabled(enabled);
  year_input_->SetEnabled(enabled);
}

void CardUnmaskPromptViews::ShowNewCardLink() {
  if (new_card_link_)
    return;

  new_card_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_NEW_CARD_LINK));
  new_card_link_->SetUnderline(false);
  new_card_link_->set_listener(this);
  input_row_->AddChildView(new_card_link_);
}

views::View* CardUnmaskPromptViews::GetContentsView() {
  InitIfNecessary();
  return this;
}

gfx::Size CardUnmaskPromptViews::CalculatePreferredSize() const {
  // If the margins width is not discounted here, the bubble border will be
  // taken into consideration in the frame width size. Because of that, the
  // dialog width will be snapped to a larger size when Harmony is enabled.
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void CardUnmaskPromptViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(GetWindowTitle()));
}

void CardUnmaskPromptViews::OnThemeChanged() {
  SkColor bg_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground);
  overlay_->SetBackground(views::CreateSolidBackground(bg_color));
  if (overlay_label_)
    overlay_label_->SetBackgroundColor(bg_color);
}

ui::ModalType CardUnmaskPromptViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 CardUnmaskPromptViews::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void CardUnmaskPromptViews::DeleteDelegate() {
  delete this;
}

int CardUnmaskPromptViews::GetDialogButtons() const {
  // In permanent error state, only the "close" button is shown.
  AutofillClient::PaymentsRpcResult result =
      controller_->GetVerificationResult();
  if (result == AutofillClient::PERMANENT_FAILURE ||
      result == AutofillClient::NETWORK_ERROR) {
    return ui::DIALOG_BUTTON_CANCEL;
  }

  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

bool CardUnmaskPromptViews::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return true;

  DCHECK_EQ(ui::DIALOG_BUTTON_OK, button);

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
  if (!controller_)
    return true;

  controller_->OnUnmaskPromptAccepted(
      cvc_input_->GetText(),
      month_input_->GetVisible()
          ? month_input_->GetTextForRow(month_input_->GetSelectedIndex())
          : base::string16(),
      year_input_->GetVisible()
          ? year_input_->GetTextForRow(year_input_->GetSelectedIndex())
          : base::string16(),
      storage_checkbox_ ? storage_checkbox_->GetChecked() : false,
      /*enable_fido_auth=*/false);
  return false;
}

void CardUnmaskPromptViews::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  if (controller_->InputCvcIsValid(new_contents))
    cvc_input_->SetInvalid(false);

  UpdateButtonLabels();
  DialogModelChanged();
}

void CardUnmaskPromptViews::OnPerformAction(views::Combobox* combobox) {
  if (ExpirationDateIsValid()) {
    if (month_input_->GetInvalid()) {
      month_input_->SetInvalid(false);
      year_input_->SetInvalid(false);
      SetRetriableErrorMessage(base::string16());
    }
  } else if (month_input_->GetSelectedIndex() !=
                 month_combobox_model_.GetDefaultIndex() &&
             year_input_->GetSelectedIndex() !=
                 year_combobox_model_.GetDefaultIndex()) {
    month_input_->SetInvalid(true);
    year_input_->SetInvalid(true);
    SetRetriableErrorMessage(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_UNMASK_INVALID_EXPIRATION_DATE));
  }

  UpdateButtonLabels();
  DialogModelChanged();
}

void CardUnmaskPromptViews::InitIfNecessary() {
  if (!children().empty())
    return;
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // The layout is a FillLayout that will contain the progress or error overlay
  // on top of the actual contents in |controls_container| (instructions, input
  // fields).
  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Inset the whole main section.
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::CONTROL));

  auto controls_container = std::make_unique<views::View>();
  controls_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  controls_container_ = AddChildView(std::move(controls_container));

  // Instruction text of the dialog.
  auto instructions =
      std::make_unique<views::Label>(controller_->GetInstructionsMessage());
  instructions->SetEnabledColor(views::style::GetColor(
      *instructions.get(), ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
      views::style::STYLE_SECONDARY));
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
  month_input->set_listener(this);
  month_input->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_EXPIRATION_MONTH));
  month_input_ = input_row->AddChildView(std::move(month_input));
  auto year_input = std::make_unique<views::Combobox>(&year_combobox_model_);
  year_input->set_listener(this);
  year_input->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_EXPIRATION_YEAR));
  year_input_ = input_row->AddChildView(std::move(year_input));
  if (!controller_->ShouldRequestExpirationDate()) {
    month_input_->SetVisible(false);
    year_input_->SetVisible(false);
  }

  std::unique_ptr<views::Textfield> cvc_input = CreateCvcTextfield();
  cvc_input->set_controller(this);
  cvc_input_ = input_row->AddChildView(std::move(cvc_input));

  auto cvc_image = std::make_unique<views::ImageView>();
  cvc_image->SetImage(rb.GetImageSkiaNamed(controller_->GetCvcImageRid()));
  cvc_image->set_tooltip_text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_CVC_IMAGE_DESCRIPTION));
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

  const SkColor warning_text_color = views::style::GetColor(
      *instructions_, ChromeTextContext::CONTEXT_BODY_TEXT_SMALL, STYLE_RED);
  auto error_icon = std::make_unique<views::ImageView>();
  error_icon->SetImage(
      gfx::CreateVectorIcon(kBrowserToolsErrorIcon, warning_text_color));
  temporary_error->SetVisible(false);
  temporary_error->AddChildView(std::move(error_icon));

  auto error_label = std::make_unique<views::Label>();
  error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  error_label->SetEnabledColor(warning_text_color);
  error_label_ = temporary_error->AddChildView(std::move(error_label));
  temporary_error_layout->SetFlexForView(error_label_, 1);
  temporary_error_ = input_container->AddChildView(std::move(temporary_error));

  controls_container_->AddChildView(std::move(input_container));

  // On top of the main contents, we add the progress/error overlay and hide it.
  // A child view will be added to it when about to be shown.
  auto overlay = std::make_unique<views::View>();
  views::GridLayout* overlay_layout = ResetOverlayLayout(overlay.get());
  overlay->SetVisible(false);

  progress_throbber_ =
      overlay_layout->AddView(std::make_unique<views::Throbber>());

  auto overlay_label = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_IN_PROGRESS));
  overlay_label->SetEnabledColor(
      overlay_label->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_ThrobberSpinningColor));
  overlay_label_ = overlay_layout->AddView(std::move(overlay_label));

  overlay_ = AddChildView(std::move(overlay));
}

bool CardUnmaskPromptViews::ExpirationDateIsValid() const {
  if (!controller_->ShouldRequestExpirationDate())
    return true;

  return controller_->InputExpirationIsValid(
      month_input_->GetTextForRow(month_input_->GetSelectedIndex()),
      year_input_->GetTextForRow(year_input_->GetSelectedIndex()));
}

void CardUnmaskPromptViews::ClosePrompt() {
  GetWidget()->Close();
}

void CardUnmaskPromptViews::UpdateButtonLabels() {
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   controller_->GetOkButtonLabel());
}

CardUnmaskPromptView* CreateCardUnmaskPromptView(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents) {
  return new CardUnmaskPromptViews(controller, web_contents);
}

}  // namespace autofill
