// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"

#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/payments_window_manager_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace autofill::payments {

DesktopPaymentsWindowManager::DesktopPaymentsWindowManager(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

DesktopPaymentsWindowManager::~DesktopPaymentsWindowManager() = default;

void DesktopPaymentsWindowManager::InitVcn3dsAuthentication(
    Vcn3dsContext context) {
  CHECK_EQ(flow_type_, FlowType::kNoFlow);
  CHECK_EQ(context.card.record_type(), CreditCard::RecordType::kVirtualCard);
  flow_type_ = FlowType::kVcn3ds;
  vcn_3ds_context_ = std::move(context);
  CreatePopup(vcn_3ds_context_->challenge_option.url_to_open);
}

void DesktopPaymentsWindowManager::WebContentsDestroyed() {
  if (flow_type_ == FlowType::kVcn3ds) {
    OnWebContentsDestroyedForVcn3ds();
  }
}

void DesktopPaymentsWindowManager::CreatePopup(const GURL& url) {
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

  // TODO(crbug.com/1517762): Handle the case where the pop-up is not shown by
  // displaying an error message.
  if (base::WeakPtr<content::NavigationHandle> navigation_handle =
          Navigate(&params)) {
    content::WebContentsObserver::Observe(navigation_handle->GetWebContents());
  }
}

void DesktopPaymentsWindowManager::OnWebContentsDestroyedForVcn3ds() {
  flow_type_ = FlowType::kNoFlow;
  if (base::expected<PaymentsWindowManager::RedirectCompletionProof,
                     PaymentsWindowManager::Vcn3dsAuthenticationPopupErrorType>
          result = ParseFinalUrlForVcn3ds(web_contents()->GetVisibleURL());
      result.has_value()) {
    CHECK(!result.value()->empty());
    client_->GetPaymentsAutofillClient()->LoadRiskData(base::BindOnce(
        &DesktopPaymentsWindowManager::OnDidLoadRiskDataForVcn3ds,
        weak_ptr_factory_.GetWeakPtr(), std::move(result.value())));
  } else {
    // TODO(crbug.com/1517762): Handle failure response.
  }
}

void DesktopPaymentsWindowManager::OnDidLoadRiskDataForVcn3ds(
    RedirectCompletionProof redirect_completion_proof,
    const std::string& risk_data) {
  client_->ShowAutofillProgressDialog(
      AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog,
      // TODO(crbug.com/1517762): Handle the user cancelling the progress
      // dialog here.
      /*cancel_callback=*/base::DoNothing());
  client_->GetPaymentsNetworkInterface()->UnmaskCard(
      CreateUnmaskRequestDetailsForVcn3ds(*client_, vcn_3ds_context_.value(),
                                          std::move(redirect_completion_proof)),
      // TODO(crbug.com/1517762): Handle the response of the
      // UnmaskCardRequest triggered here.
      /*callback=*/base::DoNothing());
}

}  // namespace autofill::payments
