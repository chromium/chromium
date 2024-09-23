// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
#include "components/payments/content/initialization_task.h"
#include "components/payments/content/payment_request_dialog.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {
class AutofillProfile;
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
  METADATA_HEADER(PaymentRequestDialogView, views::DialogDelegateView)

 public:
  class ObserverForTest {
   public:
    virtual void OnDialogOpened() = 0;

    virtual void OnDialogClosed() = 0;

    virtual void OnContactInfoOpened() = 0;

    virtual void OnOrderSummaryOpened() = 0;

    virtual void OnPaymentMethodOpened() = 0;

    virtual void OnShippingAddressSectionOpened() = 0;

    virtual void OnShippingOptionSectionOpened() = 0;

    virtual void OnShippingAddressEditorOpened() = 0;

    virtual void OnContactInfoEditorOpened() = 0;

    virtual void OnBackNavigation() = 0;

    virtual void OnBackToPaymentSheetNavigation() = 0;

    virtual void OnEditorViewUpdated() = 0;

    virtual void OnErrorMessageShown() = 0;

    virtual void OnSpecDoneUpdating() = 0;

    virtual void OnProcessingSpinnerShown() = 0;

    virtual void OnProcessingSpinnerHidden() = 0;

    virtual void OnPaymentHandlerWindowOpened() = 0;

    virtual void OnPaymentHandlerTitleSet() = 0;
  };

  PaymentRequestDialogView(const PaymentRequestDialogView&) = delete;
  PaymentRequestDialogView& operator=(const PaymentRequestDialogView&) = delete;

  // Build a Dialog around the PaymentRequest object. |observer| is used to
  // be notified of dialog events as they happen (but may be NULL) and should
  // outlive this object.
  static base::WeakPtr<PaymentRequestDialogView> Create(
      base::WeakPtr<PaymentRequest> request,
      base::WeakPtr<PaymentRequestDialogView::ObserverForTest> observer);

  // views::View
  void RequestFocus() override;

  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;

  // views::DialogDelegate:
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
  void ConfirmPaymentForTesting() override;
  bool ClickOptOutForTesting() override;

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

  // Hides the full dialog spinner with the "processing" label.
  void HideProcessingSpinner();

  Profile* GetProfile();

  // Calculates the actual payment handler dialog height based on the preferred
  // height and current browser window size.
  int GetActualPaymentHandlerDialogHeight() const;

  // Calculates the dialog width depending on whether or not the large payment
  // handler window is currently showing.
  int GetActualDialogWidth() const;

  // Called when a PaymentHandler dialog detects a title being set from the
  // underlying WebContents.
  void OnPaymentHandlerTitleSet();

  ViewStack* view_stack_for_testing() { return view_stack_; }
  ControllerMap* controller_map_for_testing() { return &controller_map_; }
  views::View* throbber_overlay_for_testing() { return throbber_overlay_; }

 private:
  // The browsertest validates the calculated dialog size.
  friend class PaymentHandlerWindowSizeTest;

  PaymentRequestDialogView(
      base::WeakPtr<PaymentRequest> request,
      base::WeakPtr<PaymentRequestDialogView::ObserverForTest> observer);
  ~PaymentRequestDialogView() override;

  void OnDialogOpened();
  void ShowInitialPaymentSheet();
  void SetupSpinnerOverlay();
  void OnDialogClosed();
  void ResizeDialogWindow();

  // views::View
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // The PaymentRequest object that initiated this dialog.
  base::WeakPtr<PaymentRequest> request_;
  ControllerMap controller_map_;
  raw_ptr<ViewStack, AcrossTasksDanglingUntriaged> view_stack_;

  // A full dialog overlay that shows a spinner and the "processing" label. It's
  // hidden until ShowProcessingSpinner is called.
  raw_ptr<views::View> throbber_overlay_;
  raw_ptr<views::Throbber> throbber_;

  base::WeakPtr<ObserverForTest> observer_for_testing_;

  // Used when the dialog is being closed to avoid re-entrance into the
  // controller_map_ or view_stack_.
  bool being_closed_ = false;

  // The number of initialization tasks that are not yet initialized.
  size_t number_of_initialization_tasks_ = 0;

  // True when payment handler screen is shown, as it is larger than the Payment
  // Request sheet view.
  bool is_showing_large_payment_handler_window_ = false;

  // Calculated based on the browser content size at the time of opening payment
  // handler window.
  int payment_handler_window_height_ = 0;

  base::WeakPtrFactory<PaymentRequestDialogView> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_
