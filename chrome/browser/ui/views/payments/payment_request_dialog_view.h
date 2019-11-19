// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
#include "components/payments/content/initialization_task.h"
#include "components/payments/content/payment_request_dialog.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

class Profile;

namespace payments {

class PaymentRequest;
class PaymentRequestSheetController;

// Maps views owned by PaymentRequestDialogView::view_stack_ to their
// controller. PaymentRequestDialogView is responsible for listening for those
// views being removed from the hierarchy and delete the associated controllers.
using ControllerMap =
    std::map<views::View*, std::unique_ptr<PaymentRequestSheetController>>;

enum class BackNavigationType {
  kOneStep = 0,
  kPaymentSheet,
};

// The dialog delegate that represents a desktop WebPayments dialog. This class
// is responsible for displaying the view associated with the current state of
// the WebPayments flow and managing the transition between those states.
class PaymentRequestDialogView : public views::DialogDelegateView,
                                 public PaymentRequestDialog,
                                 public PaymentRequestSpec::Observer,
                                 public InitializationTask::Observer {
 public:
  class ObserverForTest {
   public:
    virtual void OnDialogOpened() = 0;

    virtual void OnContactInfoOpened() = 0;

    virtual void OnOrderSummaryOpened() = 0;

    virtual void OnPaymentMethodOpened() = 0;

    virtual void OnShippingAddressSectionOpened() = 0;

    virtual void OnShippingOptionSectionOpened() = 0;

    virtual void OnCreditCardEditorOpened() = 0;

    virtual void OnShippingAddressEditorOpened() = 0;

    virtual void OnContactInfoEditorOpened() = 0;

    virtual void OnBackNavigation() = 0;

    virtual void OnBackToPaymentSheetNavigation() = 0;

    virtual void OnEditorViewUpdated() = 0;

    virtual void OnErrorMessageShown() = 0;

    virtual void OnSpecDoneUpdating() = 0;

    virtual void OnCvcPromptShown() = 0;

    virtual void OnProcessingSpinnerShown() = 0;

    virtual void OnProcessingSpinnerHidden() = 0;
  };

  // Build a Dialog around the PaymentRequest object. |observer| is used to
  // be notified of dialog events as they happen (but may be NULL) and should
  // outlive this object.
  PaymentRequestDialogView(PaymentRequest* request,
                           PaymentRequestDialogView::ObserverForTest* observer);
  ~PaymentRequestDialogView() override;

  // views::View
  void RequestFocus() override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  views::View* GetInitiallyFocusedView() override;

  // views::DialogDelegate:
  bool Cancel() override;
  bool ShouldShowCloseButton() const override;

  // payments::PaymentRequestDialog:
  void ShowDialog() override;
  void CloseDialog() override;
  void ShowErrorMessage() override;
  void ShowProcessingSpinner() override;
  bool IsInteractive() const override;
  void ShowPaymentHandlerScreen(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) override;
  void RetryDialog() override;

  // PaymentRequestSpec::Observer:
  void OnStartUpdating(PaymentRequestSpec::UpdateReason reason) override;
  void OnSpecUpdated() override;

  // InitializationTask::Observer:
  void OnInitialized(InitializationTask* initialization_task) override;

  void Pay();
  void GoBack();
  void GoBackToPaymentSheet(bool animate = true);
  void ShowContactProfileSheet();
  void ShowOrderSummary();
  void ShowShippingProfileSheet();
  void ShowPaymentMethodSheet();
  void ShowShippingOptionSheet();
  // |credit_card| is the card to be edited, or nullptr for adding a card.
  // |on_edited| is called when |credit_card| was successfully edited, and
  // |on_added| is called when a new credit card was added (the reference is
  // short-lived; callee should make a copy of the CreditCard object).
  // |back_navigation_type| identifies the type of navigation to execute once
  // the editor has completed successfully. |next_ui_tag| is the lowest value
  // that the credit card editor can use to assign to custom controls.
  void ShowCreditCardEditor(
      BackNavigationType back_navigation_type,
      int next_ui_tag,
      base::OnceClosure on_edited,
      base::OnceCallback<void(const autofill::CreditCard&)> on_added,
      autofill::CreditCard* credit_card = nullptr);
  // |profile| is the address to be edited, or nullptr for adding an address.
  // |on_edited| is called when |profile| was successfully edited, and
  // |on_added| is called when a new profile was added (the reference is
  // short-lived; callee should make a copy of the profile object).
  // |back_navigation_type| identifies the type of navigation to execute once
  // the editor has completed successfully.
  void ShowShippingAddressEditor(
      BackNavigationType back_navigation_type,
      base::OnceClosure on_edited,
      base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
      autofill::AutofillProfile* profile);
  // |profile| is the profile to be edited, or nullptr for adding a profile.
  // |on_edited| is called when |profile| was successfully edited, and
  // |on_added| is called when a new profile was added (the reference is
  // short-lived; callee should make a copy of the profile object).
  // |back_navigation_type| identifies the type of navigation to execute once
  // the editor has completed successfully.
  void ShowContactInfoEditor(
      BackNavigationType back_navigation_type,
      base::OnceClosure on_edited,
      base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
      autofill::AutofillProfile* profile = nullptr);
  void EditorViewUpdated();

  void ShowCvcUnmaskPrompt(
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate,
      content::WebContents* web_contents) override;

  // Hides the full dialog spinner with the "processing" label.
  void HideProcessingSpinner();

  Profile* GetProfile();

  ViewStack* view_stack_for_testing() { return view_stack_.get(); }
  views::View* throbber_overlay_for_testing() { return throbber_overlay_; }

 private:
  void OnDialogOpened();
  void ShowInitialPaymentSheet();
  void SetupSpinnerOverlay();

  // views::View
  gfx::Size CalculatePreferredSize() const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // Non-owned reference to the PaymentRequest that initiated this dialog. Since
  // the PaymentRequest object always outlives this one, the pointer should
  // always be valid even though there is no direct ownership relationship
  // between the two.
  PaymentRequest* request_;
  ControllerMap controller_map_;
  std::unique_ptr<ViewStack> view_stack_;

  // A full dialog overlay that shows a spinner and the "processing" label. It's
  // hidden until ShowProcessingSpinner is called.
  views::View* throbber_overlay_;
  views::Throbber* throbber_;

  // May be null.
  ObserverForTest* observer_for_testing_;

  // Used when the dialog is being closed to avoid re-entrancy into the
  // controller_map_.
  bool being_closed_ = false;

  // The number of initialization tasks that are not yet initialized.
  size_t number_of_initialization_tasks_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestDialogView);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_
