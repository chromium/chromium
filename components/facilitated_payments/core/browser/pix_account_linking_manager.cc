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

base::WeakPtr<NativeAccountLinkingHandler>
PixAccountLinkingManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::optional<AccountLinkingParams>
PixAccountLinkingManager::CreateAccountLinkingParams() {
  return std::nullopt;
}

void PixAccountLinkingManager::DoOnClientTokenReceived(
    const std::vector<uint8_t>& client_token) {
  client_token_ = client_token;
  InitiateAccountLinkingNetworkCall(client_token);
}

void PixAccountLinkingManager::DoOnAccountLinkingResult(
    AccountLinkingResult result) {
  switch (result.error_code) {
    case AccountLinkingResultCode::kResultOk:
      LogAccountLinkingResult(kPixFopSuffix, /*is_successful=*/true);
      ui_state_ = UiState::kSuccessScreen;
      client()->ShowPixAccountLinkingSuccessScreen();
      break;
    case AccountLinkingResultCode::kResultCanceled:
      // TODO(crbug.com/545056667): Update to show error notification on user
      // cancellation.
      DismissPrompt();
      LogAccountLinkingResult(kPixFopSuffix, /*is_successful=*/false);
      break;
    case AccountLinkingResultCode::kResultError:
      DismissPrompt();
      LogAccountLinkingResult(kPixFopSuffix, /*is_successful=*/false);
      client()->ShowAccountLinkingFailureNotification(
          FacilitatedPaymentsType::kPix);
      break;
    case AccountLinkingResultCode::kCouldNotInvoke:
      DismissPrompt();
      // Flow exited reason logged by base class. Don't log failure.
      break;
  }
}

void PixAccountLinkingManager::MaybeShowPixAccountLinkingPrompt(
    const url::Origin& pix_payment_page_origin) {
  // Reset to default state to prepare for a new account linking flow.
  Reset();
  pix_payment_page_origin_ = pix_payment_page_origin;

  if (!CanPromptUser()) {
    return;
  }

  // Asynchronously fetch the GMSCore client token, which will automatically
  // initiate the GetDetailsForCreatePaymentInstrument network call to check
  // eligibility and retrieve the action token.
  FetchClientToken();
  client()->GetDeviceDelegate()->SetOnReturnToChromeCallbackAndObserveAppState(
      base::BindOnce(&PixAccountLinkingManager::OnUserReturnedToChrome,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PixAccountLinkingManager::Reset() {
  has_user_returned_to_chrome_ = false;
  has_post_return_delay_passed_ = false;
  is_eligible_for_pix_account_linking_ = std::nullopt;
  if (ui_state_ != UiState::kHidden && ui_state_ != UiState::kSuccessScreen) {
    // This should NOT happen as the account linking flow cannot be triggered
    // when the bottom sheet is open.
    // TODO(crbug.com/427597144): Replace with CHECK(ui_state_ ==
    // UiState::kHidden || ui_state_ == UiState::kSuccessScreen) in
    // MaybeShowPixAccountLinkingPrompt after M144.
    base::debug::DumpWithoutCrashing();
  }
  DismissPrompt();
  is_prompt_accepted_ = false;
  pix_payment_page_origin_ = url::Origin();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void PixAccountLinkingManager::OnUserReturnedToChrome() {
  has_user_returned_to_chrome_ = true;
  base::TimeDelta delay =
      base::Seconds(kPixAccountLinkingNativeTriggerDelaySeconds.Get());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PixAccountLinkingManager::OnPostReturnDelayPassed,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void PixAccountLinkingManager::OnPostReturnDelayPassed() {
  has_post_return_delay_passed_ = true;
  ShowPixAccountLinkingPromptIfEligible();
}

void PixAccountLinkingManager::ShowPixAccountLinkingPromptIfEligible() {
  // Wait until all barriers are complete: user return AND delay passed AND
  // server response.
  if (!has_user_returned_to_chrome_ || !has_post_return_delay_passed_ ||
      !is_eligible_for_pix_account_linking_.has_value()) {
    return;
  }

  // If ineligible for account linking per payments backend, exit.
  if (!is_eligible_for_pix_account_linking_.value()) {
    return;
  }

  // Prevent showing the prompt multiple times if already showing.
  if (ui_state_ != UiState::kHidden) {
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

  ShowPixAccountLinkingPromptAfterDelay();
}

void PixAccountLinkingManager::ShowPixAccountLinkingPromptAfterDelay() {
  client()->SetUiEventListener(
      base::BindRepeating(&PixAccountLinkingManager::OnUiScreenEvent,
                          weak_ptr_factory_.GetWeakPtr()));
  ui_state_ = UiState::kPrompt;
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

strike_database::StrikeDatabaseIntegratorBase*
PixAccountLinkingManager::GetStrikeDatabase() {
  return GetOrCreateStrikeDatabase();
}

bool PixAccountLinkingManager::IsUserPrefEnabled() const {
  return client()
      ->GetPaymentsDataManager()
      ->IsFacilitatedPaymentsPixAccountLinkingUserPrefEnabled();
}

void PixAccountLinkingManager::DoOnAccepted() {
  is_prompt_accepted_ = true;
}

void PixAccountLinkingManager::OnUiScreenEvent(UiEvent ui_event_type) {
  switch (ui_event_type) {
    case UiEvent::kNewScreenShown: {
      CHECK(ui_state_ != UiState::kHidden);
      if (ui_state_ == UiState::kPrompt) {
        LogPixAccountLinkingPromptShown();
      }
      break;
    }
    case UiEvent::kScreenCouldNotBeShown: {
      CHECK(ui_state_ != UiState::kHidden);
      if (ui_state_ == UiState::kPrompt) {
        LogAccountLinkingFlowExitedReason(
            kPixFopSuffix, AccountLinkingFlowExitedReason::kScreenNotShown);
      }
      ui_state_ = UiState::kHidden;
      break;
    }
    case UiEvent::kScreenClosedNotByUser: {
      if (ui_state_ == UiState::kPrompt) {
        LogAccountLinkingFlowExitedReason(
            kPixFopSuffix,
            AccountLinkingFlowExitedReason::kScreenClosedNotByUser);
      }
      ui_state_ = UiState::kHidden;
      break;
    }
    case UiEvent::kScreenClosedByUser: {
      CHECK(ui_state_ != UiState::kHidden);
      if (ui_state_ == UiState::kPrompt) {
        LogAccountLinkingFlowExitedReason(
            kPixFopSuffix, AccountLinkingFlowExitedReason::kScreenClosedByUser);
      }
      ui_state_ = UiState::kHidden;
      break;
    }
    default:
      NOTREACHED() << "Unhandled UiEvent " << std::to_underlying(ui_event_type);
  }
}

void PixAccountLinkingManager::DoOnGetDetailsForCreatePaymentInstrumentResponse(
    bool is_eligible) {
  is_eligible_for_pix_account_linking_ = is_eligible;
  // If the user has already returned to Chrome and the post-return delay has
  // passed, trigger prompt display check now that server response is available.
  if (has_user_returned_to_chrome_ && has_post_return_delay_passed_) {
    ShowPixAccountLinkingPromptIfEligible();
  }
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
