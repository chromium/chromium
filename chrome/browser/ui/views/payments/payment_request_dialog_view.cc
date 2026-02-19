// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/payments/contact_info_editor_view_controller.h"
#include "chrome/browser/ui/views/payments/error_message_view_controller.h"
#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_method_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"
#include "chrome/browser/ui/views/payments/profile_list_view_controller.h"
#include "chrome/browser/ui/views/payments/shipping_address_editor_view_controller.h"
#include "chrome/browser/ui/views/payments/shipping_option_view_controller.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace payments {

namespace {

// The minimum ratio of browser window to Payment Request/Handler dialog that is
// considered "large enough". We add a small buffer because even if the dialog
// can technically fit, it's a bad experience if it consumes the entire window.
constexpr float kMinimumWindowToDialogRatio = 1.05f;

// The amount of time to wait before re-checking if the dialog fits in the
// browser window during a resize.
constexpr int kResizeThrottleMs = 100;

views::Widget* GetBrowserWindowWidget(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  return views::Widget::GetTopLevelWidgetForNativeView(
      web_contents->GetNativeView());
}

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

// static
base::WeakPtr<PaymentRequestDialogView> PaymentRequestDialogView::Create(
    base::WeakPtr<PaymentRequest> request,
    base::WeakPtr<PaymentRequestDialogView::ObserverForTest> observer) {
  return (new PaymentRequestDialogView(request, observer))
      ->weak_ptr_factory_.GetWeakPtr();
}

void PaymentRequestDialogView::RequestFocus() {
  view_stack_->RequestFocus();
}

views::View* PaymentRequestDialogView::GetInitiallyFocusedView() {
  return view_stack_;
}

void PaymentRequestDialogView::OnDialogClosed() {
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
  RemoveChildViewT(view_stack_.get());
  controller_map_.clear();
  request_->OnUserCancelled();

  if (observer_for_testing_) {
    observer_for_testing_->OnDialogClosed();
  }
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
  if (!DialogFitsInBrowserWindow()) {
    base::UmaHistogramEnumeration(
        "PaymentRequest.WindowSizeCheckRejectionReason",
        WindowSizeCheckRejectionReason::kRejectedAtShow);

    // To avoid tearing down the PaymentRequest class in the middle of showing
    // the dialog, we post this call asynchronously.
    //
    // TODO(crbug.com/483395708): Refactor PaymentRequest::Show so that it can
    // handle this case synchronously, e.g. via a bool return parameter.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PaymentRequest::OnInternalError, request_,
                                  errors::kBrowserWindowTooSmall));

    // Because we never handed over ownership of this object to the widget in
    // ShowWebModalDialogViews, we need to manually delete ourselves.
    //
    // TODO(crbug.com/483413436): Refactor ownership model so that explicit
    // deletion of 'this' is not required.
    delete this;
    return;
  }

  views::Widget* widget = constrained_window::ShowWebModalDialogViews(
      this, request_->web_contents());
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(widget);
  occlusion_observation_.Observe(widget);
}

void PaymentRequestDialogView::CloseDialog() {
  if (GetWidget()) {
    // This calls PaymentRequestDialogView::Cancel() before closing.
    // ViewHierarchyChanged() also gets called after Cancel().
    GetWidget()->Close();
  }
}

void PaymentRequestDialogView::ShowErrorMessage() {
  if (being_closed_ || !request_->spec()) {
    return;
  }

  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<ErrorMessageViewController>(
                            request_->spec(), request_->state(),
                            weak_ptr_factory_.GetWeakPtr()),
                        &controller_map_),
                    /* animate = */ false);
  HideProcessingSpinner();

  if (observer_for_testing_) {
    observer_for_testing_->OnErrorMessageShown();
  }
}

void PaymentRequestDialogView::ShowProcessingSpinner() {
  throbber_->Start();
  throbber_overlay_->SetVisible(true);
  throbber_overlay_->GetViewAccessibility().SetIsIgnored(false);
  throbber_overlay_->GetViewAccessibility().SetIsLeaf(false);
  if (observer_for_testing_) {
    observer_for_testing_->OnProcessingSpinnerShown();
  }
}

bool PaymentRequestDialogView::IsInteractive() const {
  return !throbber_overlay_->GetVisible();
}

void PaymentRequestDialogView::ShowPaymentHandlerScreen(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {
  if (!request_->spec()) {
    return;
  }

  // The Payment Handler window is larger than the Payment Request sheet, which
  // causes us to make different decisions when e.g. animating it.
  is_showing_large_payment_handler_window_ = true;

  // Calculate |payment_handler_window_height_|
  auto* browser = chrome::FindBrowserWithTab(request_->web_contents());
  int browser_window_content_height =
      browser->window()->GetContentsSize().height();
  payment_handler_window_height_ =
      std::max(kDialogHeight, std::min(kPreferredPaymentHandlerDialogHeight,
                                       browser_window_content_height));

  ResizeDialogWindow();

  // Once we have resized the dialog, re-check that it still fits in the
  // available window space.
  if (!DialogFitsInBrowserWindow()) {
    base::UmaHistogramEnumeration(
        "PaymentRequest.WindowSizeCheckRejectionReason",
        WindowSizeCheckRejectionReason::kRejectedAtPaymentHandlerTransition);
    std::move(callback).Run(false, 0, 0);
    request_->OnInternalError(errors::kBrowserWindowTooSmall);
    return;
  }

  view_stack_->Push(
      CreateViewAndInstallController(
          std::make_unique<PaymentHandlerWebFlowViewController>(
              request_->spec(), request_->state(),
              weak_ptr_factory_.GetWeakPtr(), request_->web_contents(),
              GetProfile(), url, std::move(callback)),
          &controller_map_),
      // Do not animate the view when the dialog size changes or payment sheet
      // is skipped.
      /* animate = */ !is_showing_large_payment_handler_window_ &&
          !request_->skipped_payment_request_ui());
  request_->OnPaymentHandlerOpenWindowCalled();
  HideProcessingSpinner();
  if (observer_for_testing_) {
    observer_for_testing_->OnPaymentHandlerWindowOpened();
  }
}

void PaymentRequestDialogView::RetryDialog() {
  if (!request_->spec()) {
    return;
  }

  HideProcessingSpinner();
  GoBackToPaymentSheet(false /* animate */);

  if (request_->spec()->has_shipping_address_error()) {
    autofill::AutofillProfile* profile =
        request_->state()->invalid_shipping_profile();
    ShowShippingAddressEditor(
        BackNavigationType::kOneStep,
        /*on_edited=*/
        base::BindOnce(&PaymentRequestState::SetSelectedShippingProfile,
                       request_->state(), profile),
        /*on_added=*/
        base::OnceCallback<void(const autofill::AutofillProfile&)>(), profile);
  }

  if (request_->spec()->has_payer_error()) {
    autofill::AutofillProfile* profile =
        request_->state()->invalid_contact_profile();
    ShowContactInfoEditor(
        BackNavigationType::kOneStep,
        /*on_edited=*/
        base::BindOnce(&PaymentRequestState::SetSelectedContactProfile,
                       request_->state(), profile),
        /*on_added=*/
        base::OnceCallback<void(const autofill::AutofillProfile&)>(), profile);
  }
}

void PaymentRequestDialogView::ConfirmPaymentForTesting() {
  Pay();
}

bool PaymentRequestDialogView::ClickOptOutForTesting() {
  NOTIMPLEMENTED();
  return false;
}

void PaymentRequestDialogView::OnStartUpdating(
    PaymentRequestSpec::UpdateReason reason) {
  ShowProcessingSpinner();
}

void PaymentRequestDialogView::OnSpecUpdated() {
  if (!request_->spec()) {
    return;
  }

  if (request_->spec()->current_update_reason() !=
      PaymentRequestSpec::UpdateReason::NONE) {
    HideProcessingSpinner();
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnSpecDoneUpdating();
  }
}

void PaymentRequestDialogView::OnInitialized(
    InitializationTask* initialization_task) {
  initialization_task->RemoveInitializationObserver(this);
  if (--number_of_initialization_tasks_ > 0) {
    return;
  }

  HideProcessingSpinner();

  if (request_->state()->are_requested_methods_supported()) {
    OnDialogOpened();
  }
}

void PaymentRequestDialogView::OnWidgetDestroying(views::Widget* widget) {
  browser_widget_observation_.Reset();
}

void PaymentRequestDialogView::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  // Note that the widget we are tracking in this method is that of the
  // containing browser window, not of the Payment Request/Handler dialog.

  if (new_bounds.size() == last_observed_browser_window_size_) {
    return;
  }
  last_observed_browser_window_size_ = new_bounds.size();

  // Throttle calls to `CheckIfDialogFitsInBrowserWindow`, as it requires
  // (re)calculating widget bounds each time it is called.
  base::TimeDelta elapsed =
      base::TimeTicks::Now() - last_check_for_too_small_window_time_;
  if (elapsed >= base::Milliseconds(kResizeThrottleMs)) {
    check_for_too_small_window_timer_.Stop();
    CheckIfDialogFitsInBrowserWindow();
  } else if (!check_for_too_small_window_timer_.IsRunning()) {
    check_for_too_small_window_timer_.Start(
        FROM_HERE, base::Milliseconds(kResizeThrottleMs) - elapsed,
        base::BindOnce(
            &PaymentRequestDialogView::CheckIfDialogFitsInBrowserWindow,
            weak_ptr_factory_.GetWeakPtr()));
  }
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

  // Do not animate views when the dialog size changes.
  view_stack_->Pop(!is_showing_large_payment_handler_window_ /* = animate */);

  // Back navigation from payment handler window should resize the dialog;
  if (is_showing_large_payment_handler_window_) {
    is_showing_large_payment_handler_window_ = false;
    ResizeDialogWindow();
  }
  if (observer_for_testing_) {
    observer_for_testing_->OnBackNavigation();
  }
}

void PaymentRequestDialogView::GoBackToPaymentSheet(bool animate) {
  // This assumes that the Payment Sheet is the first view in the stack. Thus if
  // there is only one view, we are already showing the payment sheet.
  if (view_stack_->GetSize() > 1) {
    // Do not animate views when the dialog size changes.
    view_stack_->PopMany(view_stack_->GetSize() - 1,
                         animate && !is_showing_large_payment_handler_window_);

    // Back navigation from payment handler window should resize the dialog;
    if (is_showing_large_payment_handler_window_) {
      ResizeDialogWindow();
      is_showing_large_payment_handler_window_ = false;
    }
  }
  if (observer_for_testing_) {
    observer_for_testing_->OnBackToPaymentSheetNavigation();
  }
}

void PaymentRequestDialogView::ShowContactProfileSheet() {
  if (!request_->spec()) {
    return;
  }

  view_stack_->Push(
      CreateViewAndInstallController(
          ProfileListViewController::GetContactProfileViewController(
              request_->spec(), request_->state(),
              weak_ptr_factory_.GetWeakPtr()),
          &controller_map_),
      /* animate */ true);
  if (observer_for_testing_) {
    observer_for_testing_->OnContactInfoOpened();
  }
}

void PaymentRequestDialogView::ShowOrderSummary() {
  if (!request_->spec()) {
    return;
  }

  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<OrderSummaryViewController>(
                            request_->spec(), request_->state(),
                            weak_ptr_factory_.GetWeakPtr()),
                        &controller_map_),
                    /* animate = */ true);
  if (observer_for_testing_) {
    observer_for_testing_->OnOrderSummaryOpened();
  }
}

void PaymentRequestDialogView::ShowPaymentMethodSheet() {
  if (!request_->spec()) {
    return;
  }

  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<PaymentMethodViewController>(
                            request_->spec(), request_->state(),
                            weak_ptr_factory_.GetWeakPtr()),
                        &controller_map_),
                    /* animate = */ true);
  if (observer_for_testing_) {
    observer_for_testing_->OnPaymentMethodOpened();
  }
}

void PaymentRequestDialogView::ShowShippingProfileSheet() {
  if (!request_->spec()) {
    return;
  }

  view_stack_->Push(
      CreateViewAndInstallController(
          ProfileListViewController::GetShippingProfileViewController(
              request_->spec(), request_->state(),
              weak_ptr_factory_.GetWeakPtr()),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_) {
    observer_for_testing_->OnShippingAddressSectionOpened();
  }
}

void PaymentRequestDialogView::ShowShippingOptionSheet() {
  if (!request_->spec()) {
    return;
  }

  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<ShippingOptionViewController>(
                            request_->spec(), request_->state(),
                            weak_ptr_factory_.GetWeakPtr()),
                        &controller_map_),
                    /* animate = */ true);
  if (observer_for_testing_) {
    observer_for_testing_->OnShippingOptionSectionOpened();
  }
}

void PaymentRequestDialogView::ShowShippingAddressEditor(
    BackNavigationType back_navigation_type,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
    autofill::AutofillProfile* profile) {
  if (!request_->spec()) {
    return;
  }

  view_stack_->Push(
      CreateViewAndInstallController(
          std::make_unique<ShippingAddressEditorViewController>(
              request_->spec(), request_->state(),
              weak_ptr_factory_.GetWeakPtr(), back_navigation_type,
              std::move(on_edited), std::move(on_added), profile,
              request_->IsOffTheRecord()),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_) {
    observer_for_testing_->OnShippingAddressEditorOpened();
  }
}

void PaymentRequestDialogView::ShowContactInfoEditor(
    BackNavigationType back_navigation_type,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
    autofill::AutofillProfile* profile) {
  if (!request_->spec()) {
    return;
  }

  view_stack_->Push(
      CreateViewAndInstallController(
          std::make_unique<ContactInfoEditorViewController>(
              request_->spec(), request_->state(),
              weak_ptr_factory_.GetWeakPtr(), back_navigation_type,
              std::move(on_edited), std::move(on_added), profile,
              request_->IsOffTheRecord()),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_) {
    observer_for_testing_->OnContactInfoEditorOpened();
  }
}

void PaymentRequestDialogView::EditorViewUpdated() {
  if (observer_for_testing_) {
    observer_for_testing_->OnEditorViewUpdated();
  }
}

void PaymentRequestDialogView::HideProcessingSpinner() {
  throbber_->Stop();
  // TODO(crbug.com/40894873): Instead of setting the throbber to invisible, can
  // we destroy and remove it from the view when it's not being used?
  throbber_overlay_->SetVisible(false);
  // Screen readers do not ignore invisible elements, so force the screen
  // reader to skip the invisible throbber by making it an ignored leaf node in
  // the accessibility tree.
  throbber_overlay_->GetViewAccessibility().SetIsIgnored(true);
  throbber_overlay_->GetViewAccessibility().SetIsLeaf(true);
  if (observer_for_testing_) {
    observer_for_testing_->OnProcessingSpinnerHidden();
  }
}

Profile* PaymentRequestDialogView::GetProfile() {
  return Profile::FromBrowserContext(
      request_->web_contents()->GetBrowserContext());
}

PaymentRequestDialogView::PaymentRequestDialogView(
    base::WeakPtr<PaymentRequest> request,
    base::WeakPtr<PaymentRequestDialogView::ObserverForTest> observer)
    : request_(request), observer_for_testing_(observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(request);
  DCHECK(request->spec());

  // Observe our top-level browser window widget, in order to reject the Payment
  // Request/Handler dialog if the window is resized (programmatically or by the
  // user) to be too small to contain the dialog.
  if (base::FeatureList::IsEnabled(
          features::kPaymentRequestRejectTooSmallWindows)) {
    views::Widget* browser_widget =
        GetBrowserWindowWidget(request_->web_contents());
    if (browser_widget) {
      browser_widget_observation_.Observe(browser_widget);
      last_observed_browser_window_size_ =
          browser_widget->GetWindowBoundsInScreen().size();
    }
  }

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetModalType(ui::mojom::ModalType::kChild);

  SetCloseCallback(base::BindOnce(&PaymentRequestDialogView::OnDialogClosed,
                                  weak_ptr_factory_.GetWeakPtr()));
  // The dialog's CancelCallback may be called during initialization e.g. by
  // pressing the escape key, calling OnDialogClosed will ensure the correct
  // order of destruction.
  SetCancelCallback(base::BindOnce(&PaymentRequestDialogView::OnDialogClosed,
                                   weak_ptr_factory_.GetWeakPtr()));

  request->spec()->AddObserver(this);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  view_stack_ = AddChildView(std::make_unique<ViewStack>());
  // ViewStack paints to a layer, and currently layers don't clip to the bounds
  // of the window opaque layer. Until this is fixed, we have to set rounded
  // corners directly here.
  //
  // TODO(crbug.com/358379367): Remove once layers obey the clip by default.
  view_stack_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(GetCornerRadius()));

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
}

PaymentRequestDialogView::~PaymentRequestDialogView() = default;

void PaymentRequestDialogView::OnDialogOpened() {
  if (!request_->spec()) {
    return;
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnDialogOpened();
  }
}

void PaymentRequestDialogView::ShowInitialPaymentSheet() {
  if (!request_->spec()) {
    return;
  }

  view_stack_->Push(CreateViewAndInstallController(
                        std::make_unique<PaymentSheetViewController>(
                            request_->spec(), request_->state(),
                            weak_ptr_factory_.GetWeakPtr()),
                        &controller_map_),
                    /* animate = */ false);

  if (number_of_initialization_tasks_ > 0) {
    return;
  }

  if (request_->state()->are_requested_methods_supported()) {
    OnDialogOpened();
  }
}

void PaymentRequestDialogView::SetupSpinnerOverlay() {
  throbber_overlay_ = AddChildView(std::make_unique<views::View>());

  throbber_overlay_->SetPaintToLayer();
  // Currently layers don't clip to the bounds of the parent window opaque
  // layer. Until this is fixed, we have to set rounded corners directly here.
  //
  // TODO(crbug.com/358379367): Remove once layers obey the clip by default.
  throbber_overlay_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(GetCornerRadius()));
  throbber_overlay_->SetVisible(false);
  // The throbber overlay has to have a solid white background to hide whatever
  // would be under it.
  throbber_overlay_->SetBackground(
      views::CreateSolidBackground(ui::kColorDialogBackground));

  views::BoxLayout* layout =
      throbber_overlay_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  throbber_ =
      throbber_overlay_->AddChildView(std::make_unique<views::Throbber>());
  throbber_overlay_->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_PROCESSING_MESSAGE)));
}

gfx::Size PaymentRequestDialogView::CalculatePreferredSize(
    const views::SizeBounds& /*available_size*/) const {
  if (is_showing_large_payment_handler_window_) {
    return gfx::Size(GetActualDialogWidth(),
                     GetActualPaymentHandlerDialogHeight());
  }
  return gfx::Size(GetActualDialogWidth(), kDialogHeight);
}

int PaymentRequestDialogView::GetActualPaymentHandlerDialogHeight() const {
  DCHECK_NE(0, payment_handler_window_height_);
  return payment_handler_window_height_ > 0 ? payment_handler_window_height_
                                            : kDialogHeight;
}

int PaymentRequestDialogView::GetActualDialogWidth() const {
  int actual_width = views::LayoutProvider::Get()->GetSnappedDialogWidth(
      is_showing_large_payment_handler_window_
          ? kPreferredPaymentHandlerDialogWidth
          : kDialogMinWidth);
  return actual_width;
}

void PaymentRequestDialogView::OnPaymentHandlerTitleSet() {
  if (observer_for_testing_) {
    observer_for_testing_->OnPaymentHandlerTitleSet();
  }
}

void PaymentRequestDialogView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (being_closed_) {
    return;
  }

  // When a view that is associated with a controller is removed from this
  // view's descendants, dispose of the controller.
  if (!details.is_add &&
      controller_map_.find(details.child) != controller_map_.end()) {
    DCHECK(!details.move_view);
    controller_map_.erase(details.child);
  }
}

void PaymentRequestDialogView::OnOcclusionStateChanged(bool occluded) {
  if (occluded) {
    SetEnabled(false);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PaymentRequestDialogView::CloseDialog,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void PaymentRequestDialogView::ResizeDialogWindow() {
  content::WebContents* const web_contents = request_->web_contents();
  if (GetWidget() && web_contents) {
    constrained_window::UpdateWebContentsModalDialogPosition(
        GetWidget(),
        web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
            ->delegate()
            ->GetWebContentsModalDialogHost(web_contents));
  }
}

void PaymentRequestDialogView::CheckIfDialogFitsInBrowserWindow() {
  last_check_for_too_small_window_time_ = base::TimeTicks::Now();
  if (!DialogFitsInBrowserWindow()) {
    base::UmaHistogramEnumeration(
        "PaymentRequest.WindowSizeCheckRejectionReason",
        WindowSizeCheckRejectionReason::kRejectedAtResize);
    request_->OnInternalError(errors::kBrowserWindowTooSmall);
  }
}

bool PaymentRequestDialogView::DialogFitsInBrowserWindow() const {
  if (!base::FeatureList::IsEnabled(
          features::kPaymentRequestRejectTooSmallWindows)) {
    return true;
  }

  // This method may trigger from the timer after our PaymentRequest is no
  // longer valid but before we ourselves have been torn down.
  if (!request_) {
    return true;
  }

  if (!request_->window_size_check_enabled()) {
    return true;
  }

  views::Widget* browser_widget =
      GetBrowserWindowWidget(request_->web_contents());
  if (browser_widget) {
    gfx::Rect browser_bounds = browser_widget->GetWindowBoundsInScreen();

    // This may be called during initial dialog show, before the widget exists,
    // in order to avoid a flicker before rejecting the show. In that case, we
    // can only do a rough estimation based on the preferred size. If the widget
    // exists, we can use it directly to do the size calculation.
    gfx::Size payment_request_size =
        CalculatePreferredSize(views::SizeBounds());
    gfx::Rect dialog_bounds;
    if (GetWidget()) {
      dialog_bounds = GetWidget()->GetWindowBoundsInScreen();
      gfx::Point origin_in_browser = views::View::ConvertPointFromScreen(
          browser_widget->GetRootView(), dialog_bounds.origin());
      payment_request_size =
          gfx::Size(origin_in_browser.x() + dialog_bounds.width(),
                    origin_in_browser.y() + dialog_bounds.height());
    }

    // Add a small buffer, as even if the Payment Request/Dialog can technically
    // fit in the window, it is a bad experience if it consumes the entire
    // window - the user should remain aware of the background context.
    payment_request_size = gfx::ScaleToRoundedSize(payment_request_size,
                                                   kMinimumWindowToDialogRatio);

    if (browser_bounds.width() < payment_request_size.width() ||
        browser_bounds.height() < payment_request_size.height()) {
      return false;
    }
  }

  return true;
}

BEGIN_METADATA(PaymentRequestDialogView)
ADD_READONLY_PROPERTY_METADATA(int, ActualPaymentHandlerDialogHeight)
ADD_READONLY_PROPERTY_METADATA(int, ActualDialogWidth)
END_METADATA

}  // namespace payments
