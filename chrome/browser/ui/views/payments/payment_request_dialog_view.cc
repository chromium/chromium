// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/payments/contact_info_editor_view_controller.h"
#include "chrome/browser/ui/views/payments/credit_card_editor_view_controller.h"
#include "chrome/browser/ui/views/payments/cvc_unmask_view_controller.h"
#include "chrome/browser/ui/views/payments/error_message_view_controller.h"
#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_method_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"
#include "chrome/browser/ui/views/payments/profile_list_view_controller.h"
#include "chrome/browser/ui/views/payments/shipping_address_editor_view_controller.h"
#include "chrome/browser/ui/views/payments/shipping_option_view_controller.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payments_experimental_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

namespace {

// This function creates an instance of a PaymentRequestSheetController
// subclass of concrete type |Controller|, passing it non-owned pointers to
// |dialog| and the |request| that initiated that dialog. |map| should be owned
// by |dialog|.
std::unique_ptr<views::View> CreateViewAndInstallController(
    std::unique_ptr<PaymentRequestSheetController> controller,
    payments::ControllerMap* map) {
  std::unique_ptr<views::View> view = controller->CreateView();
  (*map)[view.get()] = std::move(controller);
  return view;
}

}  // namespace

PaymentRequestDialogView::PaymentRequestDialogView(
    PaymentRequest* request,
    PaymentRequestDialogView::ObserverForTest* observer)
    : request_(request), observer_for_testing_(observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  request->spec()->AddObserver(this);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  view_stack_ = std::make_unique<ViewStack>();
  view_stack_->set_owned_by_client();
  AddChildView(view_stack_.get());

  SetupSpinnerOverlay();

  if (!request->state()->IsInitialized()) {
    request->state()->AddInitializationObserver(this);
    ++number_of_initialization_tasks_;
  }

  if (!request->spec()->IsInitialized()) {
    request->spec()->AddInitializationObserver(this);
    ++number_of_initialization_tasks_;
  }

  if (number_of_initialization_tasks_ > 0) {
    ShowProcessingSpinner();
  } else if (observer_for_testing_) {
    // When testing, signal that the processing spinner events have passed, even
    // though the UI does not need to show it.
    observer_for_testing_->OnProcessingSpinnerShown();
    observer_for_testing_->OnProcessingSpinnerHidden();
  }

  ShowInitialPaymentSheet();

  chrome::RecordDialogCreation(chrome::DialogIdentifier::PAYMENT_REQUEST);
}

PaymentRequestDialogView::~PaymentRequestDialogView() {}

void PaymentRequestDialogView::RequestFocus() {
  view_stack_->RequestFocus();
}

ui::ModalType PaymentRequestDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

views::View* PaymentRequestDialogView::GetInitiallyFocusedView() {
  return view_stack_.get();
}

bool PaymentRequestDialogView::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Called when the widget is about to close. We send a message to the
  // PaymentRequest object to signal user cancellation.
  //
  // The order of destruction is important here. First destroy all the views
  // because they may have pointers/delegates to their controllers. Then destroy
  // all the controllers, because they may have pointers to PaymentRequestSpec/
  // PaymentRequestState. Then send the signal to PaymentRequest to destroy.
  being_closed_ = true;
  for (const auto& controller : controller_map_) {
    controller.second->Stop();
  }
  view_stack_.reset();
  controller_map_.clear();
  request_->UserCancelled();
  return true;
}

bool PaymentRequestDialogView::ShouldShowCloseButton() const {
  // Don't show the normal close button on the dialog. This is because the
  // typical dialog header doesn't allow displaying anything other that the
  // title and the close button. This is insufficient for the PaymentRequest
  // dialog, which must sometimes show the back arrow next to the title.
  // Moreover, the title (and back arrow) should animate with the view they're
  // attached to.
  return false;
}

void PaymentRequestDialogView::ShowDialog() {
  constrained_window::ShowWebModalDialogViews(this, request_->web_contents());
}

void PaymentRequestDialogView::CloseDialog() {
  // This calls PaymentRequestDialogView::Cancel() before closing.
  // ViewHierarchyChanged() also gets called after Cancel().
  GetWidget()->Close();
}

void PaymentRequestDialogView::ShowErrorMessage() {
  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<ErrorMessageViewController>(
                            request_->spec(), request_->state(), this),
                        &controller_map_),
                    /* animate = */ false);
  HideProcessingSpinner();

  if (observer_for_testing_)
    observer_for_testing_->OnErrorMessageShown();
}

void PaymentRequestDialogView::ShowProcessingSpinner() {
  throbber_->Start();
  throbber_overlay_->SetVisible(true);
  if (observer_for_testing_)
    observer_for_testing_->OnProcessingSpinnerShown();
}

bool PaymentRequestDialogView::IsInteractive() const {
  return !throbber_overlay_->GetVisible();
}

void PaymentRequestDialogView::ShowPaymentHandlerScreen(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {
  view_stack_->Push(
      CreateViewAndInstallController(
          std::make_unique<PaymentHandlerWebFlowViewController>(
              request_->spec(), request_->state(), this,
              request_->web_contents(), GetProfile(), url, std::move(callback)),
          &controller_map_),
      /* animate = */ !request_->skipped_payment_request_ui());
  HideProcessingSpinner();
}

void PaymentRequestDialogView::RetryDialog() {
  HideProcessingSpinner();
  GoBackToPaymentSheet(false /* animate */);

  if (request_->spec()->has_shipping_address_error()) {
    autofill::AutofillProfile* profile =
        request_->state()->invalid_shipping_profile();
    ShowShippingAddressEditor(
        BackNavigationType::kOneStep,
        /*on_edited=*/
        base::BindOnce(
            &PaymentRequestState::SetSelectedShippingProfile,
            request_->state()->AsWeakPtr(), profile,
            PaymentRequestState::SectionSelectionStatus::kEditedSelected),
        /*on_added=*/
        base::OnceCallback<void(const autofill::AutofillProfile&)>(), profile);
  }

  if (request_->spec()->has_payer_error()) {
    autofill::AutofillProfile* profile =
        request_->state()->invalid_contact_profile();
    ShowContactInfoEditor(
        BackNavigationType::kOneStep,
        /*on_edited=*/
        base::BindOnce(
            &PaymentRequestState::SetSelectedContactProfile,
            request_->state()->AsWeakPtr(), profile,
            PaymentRequestState::SectionSelectionStatus::kEditedSelected),
        /*on_added=*/
        base::OnceCallback<void(const autofill::AutofillProfile&)>(), profile);
  }
}

void PaymentRequestDialogView::OnStartUpdating(
    PaymentRequestSpec::UpdateReason reason) {
  ShowProcessingSpinner();
}

void PaymentRequestDialogView::OnSpecUpdated() {
  if (request_->spec()->current_update_reason() !=
      PaymentRequestSpec::UpdateReason::NONE) {
    HideProcessingSpinner();
  }

  if (observer_for_testing_)
    observer_for_testing_->OnSpecDoneUpdating();
}

void PaymentRequestDialogView::OnInitialized(
    InitializationTask* initialization_task) {
  initialization_task->RemoveInitializationObserver(this);
  if (--number_of_initialization_tasks_ > 0)
    return;

  HideProcessingSpinner();

  if (request_->state()->are_requested_methods_supported())
    OnDialogOpened();
}

void PaymentRequestDialogView::Pay() {
  request_->Pay();
}

void PaymentRequestDialogView::GoBack() {
  // If payment request UI is skipped when calling PaymentRequest.show, then
  // abort payment request when back button is clicked. This only happens for
  // service worker based payment handler under circumstance.
  if (request_->skipped_payment_request_ui()) {
    CloseDialog();
    return;
  }

  view_stack_->Pop();

  if (observer_for_testing_)
    observer_for_testing_->OnBackNavigation();
}

void PaymentRequestDialogView::GoBackToPaymentSheet(bool animate) {
  // This assumes that the Payment Sheet is the first view in the stack. Thus if
  // there is only one view, we are already showing the payment sheet.
  if (view_stack_->size() > 1)
    view_stack_->PopMany(view_stack_->size() - 1, animate);

  if (observer_for_testing_)
    observer_for_testing_->OnBackToPaymentSheetNavigation();
}

void PaymentRequestDialogView::ShowContactProfileSheet() {
  view_stack_->Push(
      CreateViewAndInstallController(
          ProfileListViewController::GetContactProfileViewController(
              request_->spec(), request_->state(), this),
          &controller_map_),
      /* animate */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnContactInfoOpened();
}

void PaymentRequestDialogView::ShowOrderSummary() {
  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<OrderSummaryViewController>(
                            request_->spec(), request_->state(), this),
                        &controller_map_),
                    /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnOrderSummaryOpened();
}

void PaymentRequestDialogView::ShowPaymentMethodSheet() {
  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<PaymentMethodViewController>(
                            request_->spec(), request_->state(), this),
                        &controller_map_),
                    /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnPaymentMethodOpened();
}

void PaymentRequestDialogView::ShowShippingProfileSheet() {
  view_stack_->Push(
      CreateViewAndInstallController(
          ProfileListViewController::GetShippingProfileViewController(
              request_->spec(), request_->state(), this),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnShippingAddressSectionOpened();
}

void PaymentRequestDialogView::ShowShippingOptionSheet() {
  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<ShippingOptionViewController>(
                            request_->spec(), request_->state(), this),
                        &controller_map_),
                    /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnShippingOptionSectionOpened();
}

void PaymentRequestDialogView::ShowCvcUnmaskPrompt(
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate,
    content::WebContents* web_contents) {
  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<CvcUnmaskViewController>(
                            request_->spec(), request_->state(), this,
                            credit_card, result_delegate, web_contents),
                        &controller_map_),
                    /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnCvcPromptShown();
}

void PaymentRequestDialogView::ShowCreditCardEditor(
    BackNavigationType back_navigation_type,
    int next_ui_tag,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::CreditCard&)> on_added,
    autofill::CreditCard* credit_card) {
  view_stack_->Push(
      CreateViewAndInstallController(
          std::make_unique<CreditCardEditorViewController>(
              request_->spec(), request_->state(), this, back_navigation_type,
              next_ui_tag, std::move(on_edited), std::move(on_added),
              credit_card, request_->IsIncognito()),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnCreditCardEditorOpened();
}

void PaymentRequestDialogView::ShowShippingAddressEditor(
    BackNavigationType back_navigation_type,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
    autofill::AutofillProfile* profile) {
  view_stack_->Push(
      CreateViewAndInstallController(
          std::make_unique<ShippingAddressEditorViewController>(
              request_->spec(), request_->state(), this, back_navigation_type,
              std::move(on_edited), std::move(on_added), profile,
              request_->IsIncognito()),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnShippingAddressEditorOpened();
}

void PaymentRequestDialogView::ShowContactInfoEditor(
    BackNavigationType back_navigation_type,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
    autofill::AutofillProfile* profile) {
  view_stack_->Push(
      CreateViewAndInstallController(
          std::make_unique<ContactInfoEditorViewController>(
              request_->spec(), request_->state(), this, back_navigation_type,
              std::move(on_edited), std::move(on_added), profile,
              request_->IsIncognito()),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnContactInfoEditorOpened();
}

void PaymentRequestDialogView::EditorViewUpdated() {
  if (observer_for_testing_)
    observer_for_testing_->OnEditorViewUpdated();
}

void PaymentRequestDialogView::HideProcessingSpinner() {
  throbber_->Stop();
  throbber_overlay_->SetVisible(false);
  if (observer_for_testing_)
    observer_for_testing_->OnProcessingSpinnerHidden();
}

Profile* PaymentRequestDialogView::GetProfile() {
  return Profile::FromBrowserContext(
      request_->web_contents()->GetBrowserContext());
}

void PaymentRequestDialogView::OnDialogOpened() {
  if (request_->spec()->request_shipping() &&
      !request_->state()->selected_shipping_profile() &&
      PaymentsExperimentalFeatures::IsEnabled(
          features::kStrictHasEnrolledAutofillInstrument)) {
    view_stack_->Push(
        CreateViewAndInstallController(
            ProfileListViewController::GetShippingProfileViewController(
                request_->spec(), request_->state(), this),
            &controller_map_),
        /* animate = */ false);
  }

  if (observer_for_testing_)
    observer_for_testing_->OnDialogOpened();
}

void PaymentRequestDialogView::ShowInitialPaymentSheet() {
  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<PaymentSheetViewController>(
                            request_->spec(), request_->state(), this),
                        &controller_map_),
                    /* animate = */ false);

  if (number_of_initialization_tasks_ > 0)
    return;

  if (request_->state()->are_requested_methods_supported())
    OnDialogOpened();
}

void PaymentRequestDialogView::SetupSpinnerOverlay() {
  auto throbber = std::make_unique<views::Throbber>();
  auto throbber_overlay = std::make_unique<views::View>();

  throbber_overlay->SetPaintToLayer();
  throbber_overlay->SetVisible(false);
  // The throbber overlay has to have a solid white background to hide whatever
  // would be under it.
  throbber_overlay->SetBackground(views::CreateThemedSolidBackground(
      throbber_overlay.get(), ui::NativeTheme::kColorId_DialogBackground));

  views::GridLayout* layout =
      throbber_overlay->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* throbber_columns = layout->AddColumnSet(0);
  throbber_columns->AddPaddingColumn(0.5, 0);
  throbber_columns->AddColumn(views::GridLayout::Alignment::CENTER,
                              views::GridLayout::Alignment::TRAILING,
                              views::GridLayout::kFixedSize,
                              views::GridLayout::SizeType::USE_PREF, 0, 0);
  throbber_columns->AddPaddingColumn(0.5, 0);

  views::ColumnSet* label_columns = layout->AddColumnSet(1);
  label_columns->AddPaddingColumn(0.5, 0);
  label_columns->AddColumn(views::GridLayout::Alignment::CENTER,
                           views::GridLayout::Alignment::LEADING,
                           views::GridLayout::kFixedSize,
                           views::GridLayout::SizeType::USE_PREF, 0, 0);
  label_columns->AddPaddingColumn(0.5, 0);

  layout->StartRow(0.5, 0);
  throbber_ = layout->AddView(std::move(throbber));

  layout->StartRow(0.5, 1);
  layout->AddView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_PROCESSING_MESSAGE)));

  throbber_overlay_ = AddChildView(std::move(throbber_overlay));
}

gfx::Size PaymentRequestDialogView::CalculatePreferredSize() const {
  return gfx::Size(GetActualDialogWidth(), kDialogHeight);
}

void PaymentRequestDialogView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (being_closed_)
    return;

  // When a view that is associated with a controller is removed from this
  // view's descendants, dispose of the controller.
  if (!details.is_add &&
      controller_map_.find(details.child) != controller_map_.end()) {
    DCHECK(!details.move_view);
    controller_map_.erase(details.child);
  }
}

}  // namespace payments
