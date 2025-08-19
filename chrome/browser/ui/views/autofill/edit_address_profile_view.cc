// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/address_editor_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller.h"
#include "chrome/browser/ui/autofill/edit_address_profile_view.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/autofill/address_editor_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

class AutofillBubbleUI : public AutofillBubbleBase {
 public:
  AutofillBubbleUI(std::unique_ptr<views::Widget> dialog,
                   EditAddressProfileView* profile_view);
  AutofillBubbleUI(const AutofillBubbleUI&) = delete;
  AutofillBubbleUI& operator=(const AutofillBubbleUI&) = delete;
  ~AutofillBubbleUI() override;

 private:
  // Overrides from AutofillBubbleBase:
  void Hide() override;
  bool IsMouseHovered() const override;

  void CloseWidget(views::Widget::ClosedReason closed_reason);

  std::unique_ptr<views::Widget> dialog_;
  raw_ptr<EditAddressProfileView> profile_view_ = nullptr;
};

AutofillBubbleUI::AutofillBubbleUI(std::unique_ptr<views::Widget> dialog,
                                   EditAddressProfileView* profile_view)
    : dialog_(std::move(dialog)), profile_view_(profile_view) {
  dialog_->MakeCloseSynchronous(
      base::BindOnce(&AutofillBubbleUI::CloseWidget, base::Unretained(this)));
}

AutofillBubbleUI::~AutofillBubbleUI() = default;

void AutofillBubbleUI::Hide() {
  dialog_->Close();
}

bool AutofillBubbleUI::IsMouseHovered() const {
  // The edit view is not part of the bubbles managed by `BubbleManager`.
  NOTREACHED();
}

void AutofillBubbleUI::CloseWidget(views::Widget::ClosedReason closed_reason) {
  // We need to hold the dialog here so it remains alive long enough for the
  // stack to be cleaned up from the WidgetClosed() call. This keeps potential
  // dangling pointer checks from triggering as the stack unwinds.
  auto dialog = std::move(dialog_);
  profile_view_->WidgetClosed();
  dialog.reset();
}

}  // namespace

std::unique_ptr<AutofillBubbleBase> ShowEditAddressProfileDialogView(
    content::WebContents* web_contents,
    EditAddressProfileDialogController* controller) {
  auto* dialog = new EditAddressProfileView(controller);
  dialog->ShowForWebContents(web_contents);
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(web_contents);
  auto widget =
      tab_interface->GetTabFeatures()
          ->tab_dialog_manager()
          ->CreateAndShowDialog(
              dialog, std::make_unique<tabs::TabDialogManager::Params>());
  dialog->RequestFocus();
  return std::make_unique<AutofillBubbleUI>(std::move(widget), dialog);
}

EditAddressProfileView::EditAddressProfileView(
    EditAddressProfileDialogController* controller)
    : controller_(controller) {
  DCHECK(controller);

  // TODO(crbug.com/338254375): Remove the following line once this is the
  // default state for widgets.
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetAcceptCallbackWithClose(base::BindRepeating(
      &EditAddressProfileView::OnAcceptButtonClicked, base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      &EditAddressProfileView::OnUserDecision, base::Unretained(this),
      AutofillClient::AddressPromptUserDecision::kEditDeclined));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_DIALOG));

  SetProperty(views::kElementIdentifierKey, kTopViewId);
  SetTitle(controller_->GetWindowTitle());
  SetButtonLabel(ui::mojom::DialogButton::kOk, controller_->GetOkButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_CANCEL_BUTTON_LABEL));
}

EditAddressProfileView::~EditAddressProfileView() = default;

void EditAddressProfileView::ShowForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  auto address_editor_controller = std::make_unique<AddressEditorController>(
      controller_->GetProfileToEdit(),
      autofill::PersonalDataManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()),
      controller_->GetIsValidatable());

  // Storing subscription (which gets canceled in the destructor) in a property
  // secures using of Unretained(this).
  on_is_valid_change_subscription_ =
      address_editor_controller->AddIsValidChangedCallback(
          base::BindRepeating(&EditAddressProfileView::UpdateActionButtonState,
                              base::Unretained(this)));

  address_editor_view_ = AddChildView(std::make_unique<AddressEditorView>(
      std::move(address_editor_controller)));

  const std::u16string& footer_message = controller_->GetFooterMessage();
  if (!footer_message.empty()) {
    AddChildView(
        views::Builder<views::Label>()
            .SetText(footer_message)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .SetMultiLine(true)
            .Build());
  }
}

views::View* EditAddressProfileView::GetInitiallyFocusedView() {
  return address_editor_view_ ? address_editor_view_->initial_focus_view()
                              : nullptr;
}

void EditAddressProfileView::WidgetClosed() {
  if (controller_) {
    std::exchange(controller_, nullptr)
        ->OnDialogClosed(
            decision_,
            decision_ ==
                    AutofillClient::AddressPromptUserDecision::kEditAccepted
                ? base::optional_ref(address_editor_view_->GetAddressProfile())
                : std::nullopt);
  }
}

void EditAddressProfileView::ChildPreferredSizeChanged(views::View* child) {
  const int width = fixed_width();
  GetWidget()->SetSize(gfx::Size(width, GetHeightForWidth(width)));
}

AddressEditorView* EditAddressProfileView::GetAddressEditorViewForTesting() {
  return address_editor_view_;
}

void EditAddressProfileView::OnUserDecision(
    AutofillClient::AddressPromptUserDecision decision) {
  decision_ = decision;
}

void EditAddressProfileView::UpdateActionButtonState(bool is_valid) {
  SetButtonEnabled(ui::mojom::DialogButton::kOk, is_valid);
}

bool EditAddressProfileView::OnAcceptButtonClicked() {
  bool is_form_valid = address_editor_view_->ValidateAllFields();
  if (is_form_valid) {
    OnUserDecision(AutofillClient::AddressPromptUserDecision::kEditAccepted);
  }
  return is_form_valid;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EditAddressProfileView, kTopViewId);

}  // namespace autofill
