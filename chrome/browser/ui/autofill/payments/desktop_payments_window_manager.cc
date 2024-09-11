// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"

#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/metrics/payments/payments_window_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/payments_window_manager_util.h"
#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace autofill::payments {

namespace {

using Vcn3dsFlowEvent = autofill_metrics::Vcn3dsFlowEvent;

gfx::Rect GetPopupSizeForVcn3ds() {
  // The first two arguments do not matter as position gets overridden by
  // the tab modal pop-up code. The 600x640 size of the pop-up was decided as
  // the ideal size for user experience. This decision largely factored in how
  // to minimize scrolling while maintaining a presentable pop-up.
  return gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/600, /*height=*/640);
}

}  // namespace

DesktopPaymentsWindowManager::DesktopPaymentsWindowManager(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  scoped_observation_.Observe(BrowserList::GetInstance());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

DesktopPaymentsWindowManager::~DesktopPaymentsWindowManager() = default;

void DesktopPaymentsWindowManager::InitVcn3dsAuthentication(
    Vcn3dsContext context) {
  CHECK_EQ(flow_type_, FlowType::kNoFlow);
  CHECK_EQ(context.card.record_type(), CreditCard::RecordType::kVirtualCard);
  CHECK(!context.completion_callback.is_null());

  // The VCN 3DS metadata fields are returned from the Payments server. They
  // must always be present, so that Chrome knows what params to look for on
  // navigation. Since they are outside of Chrome's control, unexpected values
  // must be gracefully handled by displaying an error dialog.
  if (const std::optional<Vcn3dsChallengeOptionMetadata>& metadata =
          context.challenge_option.vcn_3ds_metadata;
      !metadata.has_value() || metadata->url_to_open.is_empty() ||
      metadata->success_query_param_name.empty() ||
      metadata->failure_query_param_name.empty()) {
    client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/false));
    return;
  }

  flow_type_ = FlowType::kVcn3ds;
  vcn_3ds_context_ = std::move(context);
  autofill_metrics::LogVcn3dsFlowEvent(
      Vcn3dsFlowEvent::kFlowStarted,
      /*user_consent_already_given=*/vcn_3ds_context_
          ->user_consent_already_given);
  if (vcn_3ds_context_->user_consent_already_given) {
    autofill_metrics::LogVcn3dsFlowEvent(
        Vcn3dsFlowEvent::kUserConsentDialogSkipped,
        /*user_consent_already_given=*/vcn_3ds_context_
            ->user_consent_already_given);
    CreatePopup(
        vcn_3ds_context_->challenge_option.vcn_3ds_metadata->url_to_open,
        GetPopupSizeForVcn3ds());
  } else {
    ShowVcn3dsConsentDialog();
  }
}

void DesktopPaymentsWindowManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (flow_type_ == FlowType::kVcn3ds) {
    OnDidFinishNavigationForVcn3ds();
  }
}

void DesktopPaymentsWindowManager::WebContentsDestroyed() {
  if (flow_type_ == FlowType::kVcn3ds) {
    OnWebContentsDestroyedForVcn3ds();
  }
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
void DesktopPaymentsWindowManager::OnBrowserSetLastActive(Browser* browser) {
  // If there is an ongoing payments window manager pop-up flow, and the
  // original tab's WebContents become active, activate the pop-up's
  // WebContents. This functionality is only required on Linux and LaCros, as on
  // other desktop platforms the pop-up will always be the top-most browser
  // window due to differences in window management on these platforms.
  if (web_contents()) {
    CHECK_NE(flow_type_, FlowType::kNoFlow);
    if (browser->tab_strip_model()->GetActiveWebContents() ==
        &client_->GetWebContents()) {
      web_contents()->GetDelegate()->ActivateContents(web_contents());
    }
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

void DesktopPaymentsWindowManager::CreatePopup(const GURL& url,
                                               gfx::Rect popup_size) {
  // Create a pop-up window. The created pop-up will not have any relationship
  // to the underlying tab, because `params.opener` is not set. Ensuring the
  // original tab is not a related site instance to the pop-up is critical for
  // security reasons.
  content::WebContents& source_contents = client_->GetWebContents();
  NavigateParams params(
      Profile::FromBrowserContext(source_contents.GetBrowserContext()), url,
      ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.source_contents = &source_contents;
  params.is_tab_modal_popup = true;
  params.window_features.bounds = std::move(popup_size);

  if (base::WeakPtr<content::NavigationHandle> navigation_handle =
          Navigate(&params)) {
    if (flow_type_ == FlowType::kVcn3ds) {
      vcn_3ds_popup_shown_timestamp_ = base::TimeTicks::Now();
    }
    content::WebContentsObserver::Observe(navigation_handle->GetWebContents());
  } else {
    autofill_metrics::LogVcn3dsFlowEvent(
        Vcn3dsFlowEvent::kPopupNotShown,
        /*user_consent_already_given=*/vcn_3ds_context_
            ->user_consent_already_given);
    client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/false));
  }
}

void DesktopPaymentsWindowManager::OnDidFinishNavigationForVcn3ds() {
  base::expected<RedirectCompletionResult, Vcn3dsAuthenticationResult> result =
      ParseUrlForVcn3ds(
          web_contents()->GetVisibleURL(),
          vcn_3ds_context_->challenge_option.vcn_3ds_metadata.value());
  if (result.has_value() ||
      result.error() == Vcn3dsAuthenticationResult::kAuthenticationFailed) {
    // To safely close the pop-up during a navigation event, a task must be
    // posted to the current base::SequencedTaskRunner, as the web contents must
    // complete notifying all of its observers of the navigation event before
    // closing. Closing before this has finished can result in a use-after-free.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&content::WebContents::Close,
                                  web_contents()->GetWeakPtr()));
  }
}

void DesktopPaymentsWindowManager::OnWebContentsDestroyedForVcn3ds() {
  CHECK(vcn_3ds_popup_shown_timestamp_.has_value());
  base::expected<RedirectCompletionResult, Vcn3dsAuthenticationResult> result =
      ParseUrlForVcn3ds(
          web_contents()->GetVisibleURL(),
          vcn_3ds_context_->challenge_option.vcn_3ds_metadata.value());

  // If the result implies that the authentication inside of the pop-up was
  // successful, continue the flow without resetting.
  if (result.has_value()) {
    CHECK(!result.value()->empty());
    autofill_metrics::LogVcn3dsAuthLatency(
        base::TimeTicks::Now() - vcn_3ds_popup_shown_timestamp_.value(),
        /*success=*/true);
    client_->GetPaymentsAutofillClient()->ShowAutofillProgressDialog(
        AutofillProgressDialogType::k3dsFetchVcnProgressDialog,
        base::BindOnce(&DesktopPaymentsWindowManager::
                           OnVcn3dsAuthenticationProgressDialogCancelled,
                       weak_ptr_factory_.GetWeakPtr()));
    return client_->GetPaymentsAutofillClient()->LoadRiskData(base::BindOnce(
        &DesktopPaymentsWindowManager::OnDidLoadRiskDataForVcn3ds,
        weak_ptr_factory_.GetWeakPtr(), std::move(result.value())));
  }

  // If the authentication was known to fail inside of the pop-up (for example,
  // a user retried too many times for the issuer or network's auth mechanism
  // inside of the pop-up browser window), trigger the error dialog. Otherwise,
  // it is assumed that the user manually closed the pop-up, so triggering an
  // error dialog would be a bad user experience. If the Payments Server
  // introduced invalid query parameters on the last redirect, this would fail
  // to handle that correctly, but it is not feasible to distinguish that from
  // the user closing the pop-up.
  if (result.error() == Vcn3dsAuthenticationResult::kAuthenticationFailed) {
    autofill_metrics::LogVcn3dsFlowEvent(
        Vcn3dsFlowEvent::kAuthenticationInsidePopupFailed,
        /*user_consent_already_given=*/vcn_3ds_context_
            ->user_consent_already_given);
    autofill_metrics::LogVcn3dsAuthLatency(
        base::TimeTicks::Now() - vcn_3ds_popup_shown_timestamp_.value(),
        /*success=*/false);
    client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/true));
  } else {
    autofill_metrics::LogVcn3dsFlowEvent(
        Vcn3dsFlowEvent::kFlowCancelledUserClosedPopup,
        /*user_consent_already_given=*/vcn_3ds_context_
            ->user_consent_already_given);
  }

  // The callback is always run at this point, which can be either when the user
  // closed the pop-up or an error occurred. This is so that the requester is
  // notified of the flow's completion.
  // TODO(crbug.com/334967738): Check whether the user closed the pop-up window
  // directly once an API for it is built.
  Vcn3dsAuthenticationResponse response;
  response.result = result.error();
  std::move(vcn_3ds_context_->completion_callback).Run(std::move(response));
  Reset();
}

void DesktopPaymentsWindowManager::OnDidLoadRiskDataForVcn3ds(
    RedirectCompletionResult redirect_completion_result,
    const std::string& risk_data) {
  vcn_3ds_context_->risk_data = risk_data;
  client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->UnmaskCard(CreateUnmaskRequestDetailsForVcn3ds(
                       *client_, vcn_3ds_context_.value(),
                       std::move(redirect_completion_result)),
                   base::BindOnce(&DesktopPaymentsWindowManager::
                                      OnVcn3dsAuthenticationResponseReceived,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void DesktopPaymentsWindowManager::OnVcn3dsAuthenticationResponseReceived(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const PaymentsNetworkInterface::UnmaskResponseDetails& response_details) {
  Vcn3dsAuthenticationResponse response =
      CreateVcn3dsAuthenticationResponseFromServerResult(
          result, response_details, std::move(vcn_3ds_context_->card));
  client_->GetPaymentsAutofillClient()->CloseAutofillProgressDialog(
      /*show_confirmation_before_closing=*/response.card.has_value(),
      /*no_interactive_authentication_callback=*/base::OnceClosure());
  if (!response.card.has_value()) {
    autofill_metrics::LogVcn3dsFlowEvent(
        Vcn3dsFlowEvent::kFlowFailedWhileRetrievingVCN,
        /*user_consent_already_given=*/vcn_3ds_context_
            ->user_consent_already_given);
    client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/false));
  }

  autofill_metrics::LogVcn3dsFlowEvent(
      Vcn3dsFlowEvent::kFlowSucceeded,
      /*user_consent_already_given=*/vcn_3ds_context_
          ->user_consent_already_given);
  std::move(vcn_3ds_context_->completion_callback).Run(std::move(response));
  Reset();
}

void DesktopPaymentsWindowManager::
    OnVcn3dsAuthenticationProgressDialogCancelled() {
  autofill_metrics::LogVcn3dsFlowEvent(
      Vcn3dsFlowEvent::kProgressDialogCancelled,
      /*user_consent_already_given=*/vcn_3ds_context_
          ->user_consent_already_given);
  client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->CancelRequest();
  // In the case of the dialog cancelled, we still run the callback to let the
  // caller know the flow has finished unsuccessfully.
  Vcn3dsAuthenticationResponse response;
  response.result = Vcn3dsAuthenticationResult::kAuthenticationNotCompleted;
  std::move(vcn_3ds_context_->completion_callback).Run(std::move(response));
  Reset();
}

void DesktopPaymentsWindowManager::ShowVcn3dsConsentDialog() {
  payments_window_user_consent_dialog_controller_ =
      std::make_unique<PaymentsWindowUserConsentDialogControllerImpl>(
          /*accept_callback=*/base::BindOnce(
              &DesktopPaymentsWindowManager::OnVcn3dsConsentDialogAccepted,
              weak_ptr_factory_.GetWeakPtr()),
          /*cancel_callback=*/base::BindOnce(
              &DesktopPaymentsWindowManager::OnVcn3dsConsentDialogCancelled,
              weak_ptr_factory_.GetWeakPtr()));
  payments_window_user_consent_dialog_controller_->ShowDialog(base::BindOnce(
      &CreateAndShowPaymentsWindowUserConsentDialog,
      payments_window_user_consent_dialog_controller_->GetWeakPtr(),
      base::Unretained(&client_->GetWebContents())));
}

void DesktopPaymentsWindowManager::OnVcn3dsConsentDialogAccepted() {
  autofill_metrics::LogVcn3dsFlowEvent(
      Vcn3dsFlowEvent::kUserConsentDialogAccepted,
      /*user_consent_already_given=*/vcn_3ds_context_
          ->user_consent_already_given);
  CreatePopup(vcn_3ds_context_->challenge_option.vcn_3ds_metadata->url_to_open,
              GetPopupSizeForVcn3ds());
}

void DesktopPaymentsWindowManager::OnVcn3dsConsentDialogCancelled() {
  autofill_metrics::LogVcn3dsFlowEvent(
      Vcn3dsFlowEvent::kUserConsentDialogDeclined,
      /*user_consent_already_given=*/vcn_3ds_context_
          ->user_consent_already_given);
  // In the case of the dialog cancelled, we still run the callback to let the
  // caller know the flow has finished unsuccessfully.
  Vcn3dsAuthenticationResponse response;
  response.result = Vcn3dsAuthenticationResult::kAuthenticationNotCompleted;
  std::move(vcn_3ds_context_->completion_callback).Run(std::move(response));
  Reset();
}

void DesktopPaymentsWindowManager::Reset() {
  vcn_3ds_context_.reset();
  flow_type_ = FlowType::kNoFlow;
  vcn_3ds_popup_shown_timestamp_.reset();
}

}  // namespace autofill::payments
