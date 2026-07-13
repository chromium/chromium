// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/facilitated_payments/core/browser/device_delegate.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/strike_database/strike_database.h"
#include "url/origin.h"

namespace payments::facilitated {

constexpr std::string_view kPixFopSuffix = "Pix";

PixAccountLinkingManager::PixAccountLinkingManager(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator)
    : NativeAccountLinkingHandler(client, std::move(api_client_creator)) {}

PixAccountLinkingManager::~PixAccountLinkingManager() = default;

std::string_view PixAccountLinkingManager::GetHistogramSuffix() const {
  return kPixFopSuffix;
}

base::DictValue
PixAccountLinkingManager::GetPayloadForGetDetailsForCreatePaymentInstrument() {
  return base::DictValue();
}

void PixAccountLinkingManager::DoOnClientTokenReceived(
    const std::vector<uint8_t>& client_token) {
  client_token_ = client_token;
}

void PixAccountLinkingManager::DoOnAccountLinkingResult(
    AccountLinkingResult result) {
  // TODO b/505507305 - Show success or error screen based on `result`.
  DismissPrompt();
}

void PixAccountLinkingManager::MaybeShowPixAccountLinkingPrompt(
    const url::Origin& pix_payment_page_origin) {
  // Reset to default state to prepare for a new account linking flow.
  Reset();
  pix_payment_page_origin_ = pix_payment_page_origin;

  if (auto* strike_database = GetOrCreateStrikeDatabase()) {
    auto decision = strike_database->GetStrikeDatabaseDecision();
    switch (decision) {
      case PixAccountLinkingStrikeDatabase::kDoNotBlock:
        break;
      case PixAccountLinkingStrikeDatabase::kMaxStrikeLimitReached:
        LogAccountLinkingFlowExitedReason(
            kPixFopSuffix, AccountLinkingFlowExitedReason::kMaxStrikes);
        return;
      case PixAccountLinkingStrikeDatabase::kRequiredDelayNotPassed:
        LogAccountLinkingFlowExitedReason(
            kPixFopSuffix,
            AccountLinkingFlowExitedReason::kRequiredDelayNotPassed);
        return;
    }
  }

  WalletEligibilityForPixAccountLinking wallet_eligibility =
      client()->GetDeviceDelegate()->IsPixAccountLinkingSupported();
  switch (wallet_eligibility) {
    case WalletEligibilityForPixAccountLinking::kWalletNotInstalled:
      LogAccountLinkingFlowExitedReason(
          kPixFopSuffix, AccountLinkingFlowExitedReason::kWalletNotInstalled);
      return;
    case WalletEligibilityForPixAccountLinking::kWalletVersionNotSupported:
      LogAccountLinkingFlowExitedReason(
          kPixFopSuffix,
          AccountLinkingFlowExitedReason::kWalletVersionNotSupported);
      return;
    case WalletEligibilityForPixAccountLinking::kEligible:
      break;
  }

  if (!client()
           ->GetPaymentsDataManager()
           ->IsFacilitatedPaymentsPixAccountLinkingUserPrefEnabled()) {
    LogAccountLinkingFlowExitedReason(
        kPixFopSuffix, AccountLinkingFlowExitedReason::kUserOptedOut);
    return;
  }

  if (!client()->HasScreenlockOrBiometricSetup()) {
    LogAccountLinkingFlowExitedReason(
        kPixFopSuffix,
        AccountLinkingFlowExitedReason::kNoScreenlockOrBiometricSetup);
    return;
  }

  // Make a request to payments backend to check if user is eligible for Pix
  // account linking.
  auto billing_customer_id = autofill::payments::GetBillingCustomerId(
      CHECK_DEREF(client()->GetPaymentsDataManager()));
  if (billing_customer_id == 0) {
    // If the user is not a payments customer and has copied a Pix code, we
    // automatically assume that they are eligible for account linking.
    is_eligible_for_pix_account_linking_ = true;
  } else {
    // The user is an existing payments customer. Make a backend call to check
    // eligibility for Pix account linking.
    client()
        ->GetFacilitatedPaymentsNetworkInterface()
        ->GetDetailsForCreatePaymentInstrument(
            billing_customer_id,
            /*client_token=*/std::vector<uint8_t>{},
            base::BindOnce(
                &PixAccountLinkingManager::
                    OnGetDetailsForCreatePaymentInstrumentResponseReceived,
                weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
            client()->GetPaymentsDataManager()->app_locale());
  }
  // TODO(crbug.com/417330610): Move this to after the user comes back to Chrome
  // and GetDetailsForCreatePaymentInstrument is completed.
  client()->GetDeviceDelegate()->SetOnReturnToChromeCallbackAndObserveAppState(
      base::BindOnce(
          &PixAccountLinkingManager::ShowPixAccountLinkingPromptIfEligible,
          weak_ptr_factory_.GetWeakPtr()));
}

void PixAccountLinkingManager::Reset() {
  is_eligible_for_pix_account_linking_ = std::nullopt;
  if (is_prompt_showing_) {
    // This should NOT happen as the account linking flow cannot be triggered
    // when the bottom sheet is open.
    // TODO(crbug.com/427597144): Replace with CHECK(!is_prompt_showing_) in
    // MaybeShowPixAccountLinkingPrompt after M144.
    base::debug::DumpWithoutCrashing();
    client()->DismissPrompt();
  }
  is_prompt_showing_ = false;
  pix_payment_page_origin_ = url::Origin();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void PixAccountLinkingManager::ShowPixAccountLinkingPromptIfEligible() {
  // If the server-side eligibility check is incomplete, or if ineligible for
  // account linking, exit.
  if (!is_eligible_for_pix_account_linking_.has_value() ||
      !is_eligible_for_pix_account_linking_.value()) {
    LogAccountLinkingFlowExitedReason(
        kPixFopSuffix, AccountLinkingFlowExitedReason::kServerSideIneligible);
    return;
  }

  // If the user has switched to a different tab, don't show the prompt.
  if (!client()->IsWebContentsVisibleOrOccluded()) {
    LogAccountLinkingFlowExitedReason(
        kPixFopSuffix, AccountLinkingFlowExitedReason::kTabIsNotActive);
    return;
  }

  // If the user has navigated to a different website than the one where the Pix
  // code was copied from, do NOT show the prompt. Same origin means the two
  // URLs have the same scheme, the same host, and the same port.
  if (!pix_payment_page_origin_.IsSameOriginWith(
          client()->GetLastCommittedOrigin())) {
    LogAccountLinkingFlowExitedReason(
        kPixFopSuffix, AccountLinkingFlowExitedReason::kUserSwitchedWebsite);
    return;
  }

  base::TimeDelta delay =
      base::Seconds(kPixAccountLinkingNativeTriggerDelaySeconds.Get());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &PixAccountLinkingManager::ShowPixAccountLinkingPromptAfterDelay,
          weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void PixAccountLinkingManager::ShowPixAccountLinkingPromptAfterDelay() {
  client()->SetUiEventListener(
      base::BindRepeating(&PixAccountLinkingManager::OnUiScreenEvent,
                          weak_ptr_factory_.GetWeakPtr()));
  is_prompt_showing_ = true;
  int strike_count = 0;
  if (auto* strike_database = GetOrCreateStrikeDatabase()) {
    strike_count = strike_database->GetStrikes();
  }
  client()->ShowPixAccountLinkingPrompt(
      strike_count,
      base::BindOnce(&PixAccountLinkingManager::OnAccepted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PixAccountLinkingManager::OnDeclined,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PixAccountLinkingManager::DismissPrompt() {
  NativeAccountLinkingHandler::DismissPrompt();
}

void PixAccountLinkingManager::OnAccepted() {
  LogPixAccountLinkingPromptAccepted();
  DismissPrompt();
  // Clear strikes when user accepts the prompt.
  if (auto* strike_database = GetOrCreateStrikeDatabase()) {
    strike_database->ClearStrikes();
  }
  auto account_info =
      client()->GetPaymentsDataManager()->GetAccountInfoForPaymentsServer();
  if (!account_info.IsEmpty() && !account_info.email.empty()) {
    client()->GetDeviceDelegate()->LaunchPixAccountLinkingPage(
        account_info.email);
  }
}

void PixAccountLinkingManager::OnDeclined() {
  LogAccountLinkingFlowExitedReason(
      kPixFopSuffix, AccountLinkingFlowExitedReason::kUserDeclined);
  DismissPrompt();

  if (auto* strike_database = GetOrCreateStrikeDatabase()) {
    strike_database->AddStrike();
  }
}

void PixAccountLinkingManager::OnUiScreenEvent(UiEvent ui_event_type) {
  switch (ui_event_type) {
    case UiEvent::kNewScreenShown: {
      CHECK(is_prompt_showing_);
      LogPixAccountLinkingPromptShown();
      break;
    }
    case UiEvent::kScreenCouldNotBeShown: {
      CHECK(is_prompt_showing_);
      LogAccountLinkingFlowExitedReason(
          kPixFopSuffix, AccountLinkingFlowExitedReason::kScreenNotShown);
      is_prompt_showing_ = false;
      break;
    }
    case UiEvent::kScreenClosedNotByUser: {
      if (is_prompt_showing_) {
        LogAccountLinkingFlowExitedReason(
            kPixFopSuffix,
            AccountLinkingFlowExitedReason::kScreenClosedNotByUser);
      }
      is_prompt_showing_ = false;
      break;
    }
    case UiEvent::kScreenClosedByUser: {
      CHECK(is_prompt_showing_);
      LogAccountLinkingFlowExitedReason(
          kPixFopSuffix, AccountLinkingFlowExitedReason::kScreenClosedByUser);
      is_prompt_showing_ = false;
      break;
    }
    default:
      NOTREACHED() << "Unhandled UiEvent " << std::to_underlying(ui_event_type);
  }
}

void PixAccountLinkingManager::
    OnGetDetailsForCreatePaymentInstrumentResponseReceived(
        base::TimeTicks start_time,
        autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
        bool is_eligible_for_pix_account_linking,
        const std::vector<uint8_t>& /*action_token*/) {
  // `action_token` is unused here because web-based Pix account linking does
  // not require it. The native account linking flow is coordinated by
  // `NativeAccountLinkingHandler`, which caches the token in its own callback
  // `OnGetDetailsForCreatePaymentInstrumentResponseReceived` and uses it to
  // invoke the native instrument manager.
  LogAccountLinkingGetDetailsForCreatePaymentInstrumentResultAndLatency(
      kPixFopSuffix, is_eligible_for_pix_account_linking,
      base::TimeTicks::Now() - start_time);
  is_eligible_for_pix_account_linking_ = is_eligible_for_pix_account_linking;
}

PixAccountLinkingStrikeDatabase*
PixAccountLinkingManager::GetOrCreateStrikeDatabase() {
  if (!strike_database_) {
    auto* strike_db_provider = client()->GetStrikeDatabase();
    if (strike_db_provider) {
      strike_database_ =
          std::make_unique<PixAccountLinkingStrikeDatabase>(strike_db_provider);
    }
  }
  return strike_database_.get();
}

}  // namespace payments::facilitated
