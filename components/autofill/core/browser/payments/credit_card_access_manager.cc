// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/not_fn.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/webauthn_callback_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/strike_databases/payments/fido_authentication_strike_database.h"
#endif

namespace autofill {

namespace {
// Timeout to wait for unmask details from Google Payments in milliseconds.
constexpr int64_t kUnmaskDetailsResponseTimeoutMs = 3 * 1000;  // 3 sec
// Time to wait between multiple calls to GetUnmaskDetails().
constexpr int64_t kDelayForGetUnmaskDetails = 3 * 60 * 1000;  // 3 min

// Suffix for server IDs in the cache indicating that a card is a virtual card.
const char kVirtualCardIdentifier[] = "_vcn";

}  // namespace

CreditCardAccessManager::CreditCardAccessManager(
    AutofillDriver* driver,
    AutofillClient* client,
    PersonalDataManager* personal_data_manager,
    autofill_metrics::CreditCardFormEventLogger* form_event_logger)
    : driver_(driver),
      client_(client),
      payments_client_(client_->GetPaymentsClient()),
      personal_data_manager_(personal_data_manager),
      form_event_logger_(form_event_logger) {}

CreditCardAccessManager::~CreditCardAccessManager() {
  // This clears the GUID of the most recently autofilled card with no
  // interactive authentication flow upon page navigation, as page navigation
  // results in us destroying the current CreditCardAccessManager and creating a
  // new one.
  if (client_) {
    if (auto* form_data_importer = client_->GetFormDataImporter()) {
      form_data_importer
          ->SetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted(
              absl::nullopt);
    }
  }
}

void CreditCardAccessManager::UpdateCreditCardFormEventLogger() {
  std::vector<CreditCard*> credit_cards =
      personal_data_manager_->GetCreditCards();

  size_t server_record_type_count = 0;
  size_t local_record_type_count = 0;
  for (CreditCard* credit_card : credit_cards) {
    if (credit_card->record_type() == CreditCard::RecordType::kLocalCard) {
      local_record_type_count++;
    } else {
      server_record_type_count++;
    }
  }
  form_event_logger_->set_server_record_type_count(server_record_type_count);
  form_event_logger_->set_local_record_type_count(local_record_type_count);
}

bool CreditCardAccessManager::UnmaskedCardCacheIsEmpty() {
  return unmasked_card_cache_.empty();
}

std::vector<const CachedServerCardInfo*>
CreditCardAccessManager::GetCachedUnmaskedCards() const {
  std::vector<const CachedServerCardInfo*> unmasked_cards;
  for (const auto& [key, card_info] : unmasked_card_cache_)
    unmasked_cards.push_back(&card_info);
  return unmasked_cards;
}

bool CreditCardAccessManager::IsCardPresentInUnmaskedCache(
    const CreditCard& card) const {
  return unmasked_card_cache_.find(GetKeyForUnmaskedCardsCache(card)) !=
         unmasked_card_cache_.end();
}

bool CreditCardAccessManager::DeleteCard(const CreditCard* card) {
  // Server cards cannot be deleted from within Chrome.
  bool allowed_to_delete = CreditCard::IsLocalCard(card);

  if (allowed_to_delete)
    personal_data_manager_->DeleteLocalCreditCards({*card});

  return allowed_to_delete;
}

bool CreditCardAccessManager::GetDeletionConfirmationText(
    const CreditCard* card,
    std::u16string* title,
    std::u16string* body) {
  if (!CreditCard::IsLocalCard(card))
    return false;

  if (title)
    title->assign(card->CardNameAndLastFourDigits());
  if (body) {
    body->assign(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
  }

  return true;
}

bool CreditCardAccessManager::ShouldClearPreviewedForm() {
  return !is_authentication_in_progress_;
}

void CreditCardAccessManager::PrepareToFetchCreditCard() {
#if !BUILDFLAG(IS_IOS)
  // No need to fetch details if there are no server cards.
  if (!base::ranges::any_of(personal_data_manager_->GetCreditCardsToSuggest(),
                            base::not_fn(&CreditCard::IsLocalCard))) {
    return;
  }

  // Do not make a preflight call if unnecessary, such as if one is already in
  // progress or a recently-returned call should be currently used.
  if (!can_fetch_unmask_details_)
    return;
  // As we may now be making a GetUnmaskDetails call, disallow further calls
  // until the current preflight call has been used or has timed out.
  can_fetch_unmask_details_ = false;

  // Reset in case a late response was ignored.
  ready_to_start_authentication_.Reset();

  // If `is_user_verifiable_` is set, then directly call
  // `GetUnmaskDetailsIfUserIsVerifiable()`, otherwise fetch value for
  // `is_user_verifiable_`.
  if (is_user_verifiable_.has_value()) {
    GetUnmaskDetailsIfUserIsVerifiable(is_user_verifiable_.value());
  } else {
    is_user_verifiable_called_timestamp_ = AutofillTickClock::NowTicks();

    GetOrCreateFidoAuthenticator()->IsUserVerifiable(base::BindOnce(
        &CreditCardAccessManager::GetUnmaskDetailsIfUserIsVerifiable,
        weak_ptr_factory_.GetWeakPtr()));
  }
#endif
}

void CreditCardAccessManager::GetUnmaskDetailsIfUserIsVerifiable(
    bool is_user_verifiable) {
#if !BUILDFLAG(IS_IOS)
  is_user_verifiable_ = is_user_verifiable;

  if (is_user_verifiable_called_timestamp_.has_value()) {
    autofill_metrics::LogUserVerifiabilityCheckDuration(
        AutofillTickClock::NowTicks() -
        is_user_verifiable_called_timestamp_.value());
  }

  // If there is already an unmask details request in progress, do not initiate
  // another one and return early.
  if (unmask_details_request_in_progress_) {
    return;
  }

  // Log that we are initiating the card unmask preflight flow.
  autofill_metrics::LogCardUnmaskPreflightInitiated();

  // If user is verifiable, then make preflight call to payments to fetch unmask
  // details, otherwise the only option is to perform CVC Auth, which does not
  // require any.
  if (is_user_verifiable_.value_or(false)) {
    unmask_details_request_in_progress_ = true;
    preflight_call_timestamp_ = AutofillTickClock::NowTicks();
    payments_client_->GetUnmaskDetails(
        base::BindOnce(&CreditCardAccessManager::OnDidGetUnmaskDetails,
                       weak_ptr_factory_.GetWeakPtr()),
        personal_data_manager_->app_locale());
    autofill_metrics::LogCardUnmaskPreflightCalled(
        GetOrCreateFidoAuthenticator()->IsUserOptedIn());
  }
#endif
}

void CreditCardAccessManager::OnDidGetUnmaskDetails(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::UnmaskDetails& unmask_details) {
  // Log latency for preflight call.
  if (preflight_call_timestamp_.has_value()) {
    autofill_metrics::LogCardUnmaskPreflightDuration(
        AutofillTickClock::NowTicks() - *preflight_call_timestamp_);
  }

  unmask_details_request_in_progress_ = false;
  unmask_details_ = unmask_details;

  // TODO(crbug.com/1409151): Rename `offer_fido_opt_in`, and check that the
  // user is off the record separately.
  unmask_details_.offer_fido_opt_in =
      unmask_details_.offer_fido_opt_in && !client_->IsOffTheRecord();

  // Set delay as fido request timeout if available, otherwise set to default.
  int delay_ms = kDelayForGetUnmaskDetails;
  if (unmask_details_.fido_request_options.has_value()) {
    const absl::optional<int> request_timeout =
        unmask_details_.fido_request_options->FindInt("timeout_millis");
    if (request_timeout.has_value()) {
      delay_ms = *request_timeout;
    }
  }

#if !BUILDFLAG(IS_IOS)
  opt_in_intention_ =
      GetOrCreateFidoAuthenticator()->GetUserOptInIntention(unmask_details);
#endif
  ready_to_start_authentication_.Signal();

  // Use the weak_ptr here so that the delayed task won't be executed if the
  // |credit_card_access_manager| is reset.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CreditCardAccessManager::SignalCanFetchUnmaskDetails,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(delay_ms));
}

void CreditCardAccessManager::FetchCreditCard(
    const CreditCard* card,
    base::WeakPtr<Accessor> accessor) {
  // Return error if authentication is already in progress, but don't reset
  // status.
  if (is_authentication_in_progress_) {
    accessor->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                  nullptr);
    return;
  }

  // If card is nullptr we reset all states and return error.
  if (!card) {
    accessor->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                  nullptr);
    Reset();
    return;
  }

  // Get the card's record type to correctly handle its fetching.
  CreditCard::RecordType record_type = card->record_type();

  // Log the server card unmasking attempt, and differentiate based on server
  // card or virtual card.
  if (ShouldLogServerCardUnmaskAttemptMetrics(record_type)) {
    autofill_metrics::LogServerCardUnmaskAttempt(
        record_type == CreditCard::RecordType::kVirtualCard
            ? AutofillClient::PaymentsRpcCardType::kVirtualCard
            : AutofillClient::PaymentsRpcCardType::kServerCard);
  }

  // If card has been previously unmasked, use cached data.
  std::unordered_map<std::string, CachedServerCardInfo>::iterator it =
      unmasked_card_cache_.find(GetKeyForUnmaskedCardsCache(*card));
  if (it != unmasked_card_cache_.end()) {  // key is in cache
    it->second.card.set_cvc(it->second.cvc);
    accessor->OnCreditCardFetched(CreditCardFetchResult::kSuccess,
                                  /*credit_card=*/&it->second.card);
    std::string metrics_name =
        record_type == CreditCard::RecordType::kVirtualCard
            ? "Autofill.UsedCachedVirtualCard"
            : "Autofill.UsedCachedServerCard";
    base::UmaHistogramCounts1000(metrics_name, ++it->second.cache_uses);
    if (record_type == CreditCard::RecordType::kVirtualCard) {
      autofill_metrics::LogServerCardUnmaskResult(
          autofill_metrics::ServerCardUnmaskResult::kLocalCacheHit,
          AutofillClient::PaymentsRpcCardType::kVirtualCard,
          autofill_metrics::VirtualCardUnmaskFlowType::kUnspecified);
    }

    Reset();
    return;
  }

  card_ = std::make_unique<CreditCard>(*card);
  accessor_ = accessor;

  switch (record_type) {
    case CreditCard::RecordType::kVirtualCard:
      return FetchVirtualCard();
    case CreditCard::RecordType::kMaskedServerCard:
      return FetchMaskedServerCard();
    case CreditCard::RecordType::kLocalCard:
    case CreditCard::RecordType::kFullServerCard:
      return FetchLocalOrFullServerCard();
  }
}

void CreditCardAccessManager::FIDOAuthOptChange(bool opt_in) {
#if BUILDFLAG(IS_IOS)
  return;
#else
  if (opt_in) {
    ShowWebauthnOfferDialog(/*card_authorization_token=*/std::string());
  } else {
    // We should not offer to update any user preferences when the user is off
    // the record. This also protects against a possible crash when attempting
    // to add the maximum amount of strikes to the FIDO auth strike database, as
    // strike databases are not present in incognito mode and should not be
    // used.
    if (client_->IsOffTheRecord()) {
      return;
    }

    GetOrCreateFidoAuthenticator()->OptOut();
    if (auto* strike_database =
            GetOrCreateFidoAuthenticator()
                ->GetOrCreateFidoAuthenticationStrikeDatabase()) {
      strike_database->AddStrikes(
          FidoAuthenticationStrikeDatabase::kStrikesToAddWhenUserOptsOut);
    }
  }
#endif
}

void CreditCardAccessManager::OnSettingsPageFIDOAuthToggled(bool opt_in) {
#if BUILDFLAG(IS_IOS)
  return;
#else
  // TODO(crbug/949269): Add a rate limiter to counter spam clicking.
  FIDOAuthOptChange(opt_in);
#endif
}

void CreditCardAccessManager::SignalCanFetchUnmaskDetails() {
  can_fetch_unmask_details_ = true;
}

void CreditCardAccessManager::CacheUnmaskedCardInfo(const CreditCard& card,
                                                    const std::u16string& cvc) {
  DCHECK(card.record_type() == CreditCard::RecordType::kFullServerCard ||
         card.record_type() == CreditCard::RecordType::kVirtualCard);
  std::string identifier =
      card.record_type() == CreditCard::RecordType::kVirtualCard
          ? card.server_id() + kVirtualCardIdentifier
          : card.server_id();
  CachedServerCardInfo card_info = {card, cvc, /*cache_uses=*/0};
  unmasked_card_cache_[identifier] = card_info;
}

void CreditCardAccessManager::StartAuthenticationFlow(bool fido_auth_enabled) {
#if BUILDFLAG(IS_IOS)
  // There is no FIDO auth available on iOS and there are no virtual cards on
  // iOS either, so offer CVC auth immediately.
  Authenticate(UnmaskAuthFlowType::kCvc);
#else
  if (card_->record_type() == CreditCard::RecordType::kVirtualCard) {
    StartAuthenticationFlowForVirtualCard(fido_auth_enabled);
  } else {
    StartAuthenticationFlowForMaskedServerCard(fido_auth_enabled);
  }
#endif
}

void CreditCardAccessManager::StartAuthenticationFlowForVirtualCard(
    bool fido_auth_enabled) {
  // TODO(crbug.com/1243475): Currently if the card is a virtual card and FIDO
  // auth was provided by issuer, we prefer FIDO auth. Remove FIDO preference
  // and allow user selections later.
  if (fido_auth_enabled) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    ShowVerifyPendingDialog();
#endif
    Authenticate(UnmaskAuthFlowType::kFido);
    return;
  }

  // Otherwise, we first check if other options are provided. If not, end the
  // session and return an error.
  std::vector<CardUnmaskChallengeOption>& challenge_options =
      virtual_card_unmask_response_details_.card_unmask_challenge_options;
  if (challenge_options.empty()) {
    accessor_->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                   nullptr);
    client_->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/true));
    Reset();
    autofill_metrics::LogServerCardUnmaskResult(
        autofill_metrics::ServerCardUnmaskResult::
            kOnlyFidoAvailableButNotOptedIn,
        AutofillClient::PaymentsRpcCardType::kVirtualCard,
        autofill_metrics::VirtualCardUnmaskFlowType::kFidoOnly);
    return;
  }

  // If we only have one challenge option, and it is a CVC challenge option, go
  // directly to the CVC input dialog.
  if (challenge_options.size() == 1 &&
      challenge_options[0].type == CardUnmaskChallengeOptionType::kCvc) {
    selected_challenge_option_ = &challenge_options[0];
    Authenticate(UnmaskAuthFlowType::kCvc);
    return;
  }

  // If we have multiple challenge options available, render the challenge
  // option selection dialog. This dialog also handles the case where we only
  // have an OTP challenge option.
  ShowUnmaskAuthenticatorSelectionDialog();
}

void CreditCardAccessManager::StartAuthenticationFlowForMaskedServerCard(
    bool fido_auth_enabled) {
  UnmaskAuthFlowType flow_type;
#if BUILDFLAG(IS_IOS)
  // There is no FIDO auth available on iOS, so offer CVC auth immediately.
  flow_type = UnmaskAuthFlowType::kCvc;
#else
  if (!fido_auth_enabled) {
    // If FIDO auth is not enabled we offer CVC auth.
    flow_type = UnmaskAuthFlowType::kCvc;
  } else if (!IsSelectedCardFidoAuthorized()) {
    // If FIDO auth is enabled but the card has not been authorized for FIDO, we
    // offer CVC auth followed with a FIDO authorization.
    flow_type = UnmaskAuthFlowType::kCvcThenFido;
  } else if (!card_->IsExpired(AutofillClock::Now())) {
    // If the FIDO auth is enabled and card has been authorized and card is not
    // expired, we offer FIDO auth.
    flow_type = UnmaskAuthFlowType::kFido;
  } else {
    // For other cases we offer CVC auth as well. E.g. A card that has been
    // authorized but is expired.
    flow_type = UnmaskAuthFlowType::kCvc;
  }
#endif

  Authenticate(flow_type);
}

void CreditCardAccessManager::Authenticate(
    UnmaskAuthFlowType unmask_auth_flow_type) {
  unmask_auth_flow_type_ = unmask_auth_flow_type;

  // Reset now that we have started authentication.
  ready_to_start_authentication_.Reset();
  unmask_details_request_in_progress_ = false;

  form_event_logger_->LogCardUnmaskAuthenticationPromptShown(
      unmask_auth_flow_type_);

  // If FIDO auth was suggested, log which authentication method was
  // actually used.
  switch (unmask_auth_flow_type_) {
    case UnmaskAuthFlowType::kFido: {
      autofill_metrics::LogCardUnmaskTypeDecision(
          autofill_metrics::CardUnmaskTypeDecisionMetric::kFidoOnly);
#if BUILDFLAG(IS_IOS)
      NOTREACHED();
#else
      // If |is_authentication_in_progress_| is false, it means the process has
      // been cancelled via the verification pending dialog. Do not run
      // CreditCardFidoAuthenticator::Authenticate() in this case (should not
      // fall back to CVC auth either).
      if (!is_authentication_in_progress_) {
        Reset();
        return;
      }

      // For virtual cards the |fido_request_option| comes from the
      // UnmaskResponseDetails while for masked server cards, it comes from the
      // UnmaskDetails.
      base::Value::Dict fido_request_options;
      absl::optional<std::string> context_token;
      if (card_->record_type() == CreditCard::RecordType::kVirtualCard) {
        context_token = virtual_card_unmask_response_details_.context_token;
        fido_request_options = std::move(
            virtual_card_unmask_response_details_.fido_request_options.value());
      } else {
        fido_request_options =
            std::move(unmask_details_.fido_request_options.value());
      }
      GetOrCreateFidoAuthenticator()->Authenticate(
          *card_, weak_ptr_factory_.GetWeakPtr(),
          std::move(fido_request_options), context_token);
#endif
      break;
    }
    case UnmaskAuthFlowType::kCvcThenFido:
      autofill_metrics::LogCardUnmaskTypeDecision(
          autofill_metrics::CardUnmaskTypeDecisionMetric::kCvcThenFido);
      ABSL_FALLTHROUGH_INTENDED;
    case UnmaskAuthFlowType::kCvc:
    case UnmaskAuthFlowType::kCvcFallbackFromFido: {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      // Close the Webauthn verify pending dialog if it enters CVC
      // authentication flow since the card unmask prompt will pop up.
      client_->CloseWebauthnDialog();
#endif

      // Delegate the task to CreditCardCvcAuthenticator.
      // If we are in the virtual card CVC auth case, we must also pass in
      // the vcn context token and the selected challenge option.
      if (card_->record_type() == CreditCard::RecordType::kVirtualCard) {
        DCHECK(selected_challenge_option_);
        client_->GetCvcAuthenticator()->Authenticate(
            card_.get(), weak_ptr_factory_.GetWeakPtr(), personal_data_manager_,
            virtual_card_unmask_response_details_.context_token,
            *selected_challenge_option_);
      } else {
        client_->GetCvcAuthenticator()->Authenticate(
            card_.get(), weak_ptr_factory_.GetWeakPtr(),
            personal_data_manager_);
      }
      break;
    }
    case UnmaskAuthFlowType::kOtp:
    case UnmaskAuthFlowType::kOtpFallbackFromFido: {
      // Delegate the task to CreditCardOtpAuthenticator.
      DCHECK(selected_challenge_option_);
      client_->GetOtpAuthenticator()->OnChallengeOptionSelected(
          card_.get(), *selected_challenge_option_,
          weak_ptr_factory_.GetWeakPtr(),
          virtual_card_unmask_response_details_.context_token,
          payments::GetBillingCustomerId(personal_data_manager_));
      break;
    }
    case UnmaskAuthFlowType::kNone:
      // Run into other unexpected types.
      NOTREACHED();
      Reset();
      break;
  }
}

#if !BUILDFLAG(IS_IOS)
CreditCardFidoAuthenticator*
CreditCardAccessManager::GetOrCreateFidoAuthenticator() {
  if (!fido_authenticator_)
    fido_authenticator_ =
        std::make_unique<CreditCardFidoAuthenticator>(driver_, client_);
  return fido_authenticator_.get();
}
#endif

void CreditCardAccessManager::OnCvcAuthenticationComplete(
    const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response) {
  is_authentication_in_progress_ = false;
  can_fetch_unmask_details_ = true;

  // Save credit card for caching purpose. CVC is also saved if response
  // contains CVC. `response.card` can be nullptr in the case of an error in the
  // response. If the response has an error, the `ShouldRespondImmediately()`
  // call below will return true and we will safely pass nullptr and that it is
  // an error into `accessor_->OnCreditCardFetched()`, and end the flow.
  if (response.card) {
    // TODO(crbug/1478392): Deprecate `response.cvc` and `response.card.cvc`.
    card_ = std::make_unique<CreditCard>(*response.card);
    card_->set_cvc(response.cvc);
  }

  // Log completed CVC authentication if auth was successful. Do not log for
  // kCvcThenFido flow since that is yet to be completed.
  if (response.did_succeed &&
      unmask_auth_flow_type_ != UnmaskAuthFlowType::kCvcThenFido) {
    form_event_logger_->LogCardUnmaskAuthenticationPromptCompleted(
        unmask_auth_flow_type_);
  }

  bool should_register_card_with_fido = ShouldRegisterCardWithFido(response);
  if (ShouldRespondImmediately(response)) {
    // If ShouldRespondImmediately() returns true,
    // |should_register_card_with_fido| should be false.
    DCHECK(!should_register_card_with_fido);
    accessor_->OnCreditCardFetched(response.did_succeed
                                       ? CreditCardFetchResult::kSuccess
                                       : CreditCardFetchResult::kTransientError,
                                   card_.get());
    unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;
  } else if (should_register_card_with_fido) {
#if !BUILDFLAG(IS_IOS)
    absl::optional<base::Value::Dict> request_options = absl::nullopt;
    if (unmask_details_.fido_request_options.has_value()) {
      // For opted-in user (CVC then FIDO case), request options are returned in
      // unmask detail response.
      request_options = unmask_details_.fido_request_options->Clone();
    } else if (response.request_options.has_value()) {
      // For Android users, request_options are provided from GetRealPan if the
      // user has chosen to opt-in.
      request_options = response.request_options->Clone();
    }

    // Additionally authorizes the card with FIDO. It also delays the form
    // filling.
    GetOrCreateFidoAuthenticator()->Authorize(weak_ptr_factory_.GetWeakPtr(),
                                              response.card_authorization_token,
                                              request_options->Clone());
#endif
  }
  if (ShouldOfferFidoOptInDialog(response)) {
    // CreditCardFidoAuthenticator will handle enrollment completely.
    ShowWebauthnOfferDialog(response.card_authorization_token);
  }

  HandleFidoOptInStatusChange();
  // TODO(crbug.com/1249665): Add Reset() to this function after cleaning up the
  // FIDO opt-in status change. This should not have any negative impact now
  // except for readability and cleanness. The result of
  // ShouldOfferFidoOptInDialog() and |opt_in_intention_| are to some extent
  // duplicate. We should be able to combine them into one function.
}

#if BUILDFLAG(IS_ANDROID)
bool CreditCardAccessManager::ShouldOfferFidoAuth() const {
  if (!unmask_details_.offer_fido_opt_in &&
      unmask_details_.fido_request_options.has_value()) {
    // Server instructed the client to not offer fido because the client is
    // already opted in. This can be verified with the presence of request
    // options in the server response.
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kAlreadyOptedIn);
    return false;
  }

  if (!unmask_details_.offer_fido_opt_in) {
    // If the server thinks FIDO opt-in is not required for this user, then we
    // won't offer the FIDO opt-in checkbox on the card unmask dialog. Since the
    // client is not opted-in and device is eligible, this could mean that the
    // server does not have a valid key for this device or the server is in a
    // bad state.
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kUnmaskDetailsOfferFidoOptInFalse);
    return false;
  }

  if (opt_in_intention_ == UserOptInIntention::kIntentToOptIn) {
    // If the user opted-in through the settings page, do not show checkbox.
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kOptedInFromSettings);
    return false;
  }

  if (card_->record_type() == CreditCard::RecordType::kVirtualCard) {
    // We should not offer FIDO opt-in for virtual cards.
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kVirtualCard);
    return false;
  }

  // No situations were found where we should not show the checkbox, so we
  // should return true to indicate that we should display the checkbox to the
  // user.
  return true;
}

bool CreditCardAccessManager::UserOptedInToFidoFromSettingsPageOnMobile()
    const {
  return opt_in_intention_ == UserOptInIntention::kIntentToOptIn;
}
#endif

#if !BUILDFLAG(IS_IOS)
void CreditCardAccessManager::OnFIDOAuthenticationComplete(
    const CreditCardFidoAuthenticator::FidoAuthenticationResponse& response) {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableFIDOProgressDialog)) {
    // Close the progress dialog when the authentication for getting the full
    // card completes.
    client_->CloseAutofillProgressDialog(
        /*show_confirmation_before_closing=*/true);
  }
#else
  // Close the Webauthn verify pending dialog. If FIDO authentication succeeded,
  // card is filled to the form, otherwise fall back to CVC authentication which
  // does not need the verify pending dialog either.
  client_->CloseWebauthnDialog();
#endif

  if (response.did_succeed) {
    form_event_logger_->LogCardUnmaskAuthenticationPromptCompleted(
        unmask_auth_flow_type_);
    if (card_->record_type() == CreditCard::RecordType::kVirtualCard) {
      autofill_metrics::LogServerCardUnmaskResult(
          autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked,
          AutofillClient::PaymentsRpcCardType::kVirtualCard,
          autofill_metrics::VirtualCardUnmaskFlowType::kFidoOnly);
    }

    // Save credit card for caching purpose.
    if (response.card) {
      card_ = std::make_unique<CreditCard>(*response.card);
    }
    accessor_->OnCreditCardFetched(CreditCardFetchResult::kSuccess,
                                   card_.get());
    Reset();
  } else if (
      response.failure_type ==
          payments::FullCardRequest::VIRTUAL_CARD_RETRIEVAL_TRANSIENT_FAILURE ||
      response.failure_type ==
          payments::FullCardRequest::VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE) {
    CreditCardFetchResult result =
        response.failure_type == payments::FullCardRequest::
                                     VIRTUAL_CARD_RETRIEVAL_TRANSIENT_FAILURE
            ? CreditCardFetchResult::kTransientError
            : CreditCardFetchResult::kPermanentError;
    // If it is an virtual card retrieval error, we don't want to invoke the CVC
    // authentication afterwards. Instead reset all states, notify accessor and
    // invoke the error dialog.
    client_->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/response.failure_type ==
            payments::FullCardRequest::
                VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE));
    accessor_->OnCreditCardFetched(result, nullptr);

    if (card_->record_type() == CreditCard::RecordType::kVirtualCard) {
      autofill_metrics::LogServerCardUnmaskResult(
          autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError,
          AutofillClient::PaymentsRpcCardType::kVirtualCard,
          autofill_metrics::VirtualCardUnmaskFlowType::kFidoOnly);
    }
    Reset();
  } else {
    // If it is an authentication error, start the CVC authentication process
    // for masked server cards or the virtual card authentication process for
    // virtual cards.
    if (card_->record_type() == CreditCard::RecordType::kVirtualCard) {
      StartAuthenticationFlowForVirtualCard(/*fido_auth_enabled=*/false);
    } else {
      Authenticate(UnmaskAuthFlowType::kCvcFallbackFromFido);
    }
  }
}

void CreditCardAccessManager::OnFidoAuthorizationComplete(bool did_succeed) {
  if (did_succeed) {
    accessor_->OnCreditCardFetched(CreditCardFetchResult::kSuccess,
                                   card_.get());
    form_event_logger_->LogCardUnmaskAuthenticationPromptCompleted(
        unmask_auth_flow_type_);
  }
  Reset();
}
#endif

void CreditCardAccessManager::OnOtpAuthenticationComplete(
    const CreditCardOtpAuthenticator::OtpAuthenticationResponse& response) {
  // Save credit card for caching purpose. CVC is also saved if response
  // contains CVC. `response.card` can be nullptr in the case of an error in the
  // response. If the response has an error, we will safely pass nullptr and
  // that it is an error into `accessor_->OnCreditCardFetched()`, and end the
  // flow.
  if (response.card) {
    card_ = std::make_unique<CreditCard>(*response.card);
    card_->set_cvc(response.cvc);
  }
  accessor_->OnCreditCardFetched(
      response.result == CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                             Result::kSuccess
          ? CreditCardFetchResult::kSuccess
          : CreditCardFetchResult::kTransientError,
      card_.get());

  autofill_metrics::ServerCardUnmaskResult result;
  switch (response.result) {
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kSuccess:
      result =
          autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kFlowCancelled:
      result = autofill_metrics::ServerCardUnmaskResult::kFlowCancelled;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kGenericError:
      result = autofill_metrics::ServerCardUnmaskResult::kUnexpectedError;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kAuthenticationError:
      result = autofill_metrics::ServerCardUnmaskResult::kAuthenticationError;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kVirtualCardRetrievalError:
      result =
          autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kUnknown:
      NOTREACHED();
      return;
  }

  autofill_metrics::VirtualCardUnmaskFlowType flow_type;
  if (unmask_auth_flow_type_ == UnmaskAuthFlowType::kOtp) {
    flow_type = autofill_metrics::VirtualCardUnmaskFlowType::kOtpOnly;
  } else {
    DCHECK(unmask_auth_flow_type_ == UnmaskAuthFlowType::kOtpFallbackFromFido);
    flow_type =
        autofill_metrics::VirtualCardUnmaskFlowType::kOtpFallbackFromFido;
  }
  autofill_metrics::LogServerCardUnmaskResult(
      result, AutofillClient::PaymentsRpcCardType::kVirtualCard, flow_type);

  HandleFidoOptInStatusChange();
  Reset();
}

bool CreditCardAccessManager::IsUserOptedInToFidoAuth() {
#if BUILDFLAG(IS_IOS)
  return false;
#else
  return is_user_verifiable_.value_or(false) &&
         GetOrCreateFidoAuthenticator()->IsUserOptedIn();
#endif
}

bool CreditCardAccessManager::IsFidoAuthEnabled(bool fido_auth_offered) {
  // FIDO auth is enabled if payments offers FIDO auth, and local pref
  // indicates that the user is opted-in.
  return fido_auth_offered && IsUserOptedInToFidoAuth();
}

bool CreditCardAccessManager::IsSelectedCardFidoAuthorized() {
  DCHECK_NE(unmask_details_.unmask_auth_method,
            AutofillClient::UnmaskAuthMethod::kUnknown);
  return IsUserOptedInToFidoAuth() &&
         unmask_details_.fido_eligible_card_ids.find(card_->server_id()) !=
             unmask_details_.fido_eligible_card_ids.end();
}

bool CreditCardAccessManager::ShouldRespondImmediately(
    const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response) {
#if BUILDFLAG(IS_ANDROID)
  // GetRealPan did not return RequestOptions (user did not specify intent to
  // opt-in) AND flow is not registering a new card, so fill the form
  // directly.
  if (!response.request_options.has_value() &&
      unmask_auth_flow_type_ != UnmaskAuthFlowType::kCvcThenFido) {
    return true;
  }
#else
  if (unmask_auth_flow_type_ != UnmaskAuthFlowType::kCvcThenFido)
    return true;
#endif
  // If the response did not succeed, report the error immediately. If
  // GetRealPan did not return a card authorization token (we can't call any
  // FIDO-related flows, either opt-in or register new card, without the token),
  // fill the form immediately.
  return !response.did_succeed || response.card_authorization_token.empty();
}

bool CreditCardAccessManager::ShouldRegisterCardWithFido(
    const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response) {
  // Card authorization token is required in order to call
  // CreditCardFidoAuthenticator::Authorize(), so if we do not have a card
  // authorization token populated we immediately return false.
  if (response.card_authorization_token.empty())
    return false;

  // |unmask_auth_flow_type_| is kCvcThenFido, then the user is already opted-in
  // and the new card must additionally be authorized through WebAuthn.
  if (unmask_auth_flow_type_ == UnmaskAuthFlowType::kCvcThenFido)
    return true;

#if BUILDFLAG(IS_ANDROID)
  // For Android, we will delay the form filling for both intent-to-opt-in user
  // opting in and opted-in user registering a new card (kCvcThenFido). So we
  // check one more scenario for Android here. If the GetRealPan response
  // includes |request_options|, that means the user showed intention to opt-in
  // while unmasking and must complete the challenge before successfully
  // opting-in and filling the form.
  if (response.request_options.has_value())
    return true;
#endif

  // No conditions to offer FIDO registration are met, so we return false.
  return false;
}

bool CreditCardAccessManager::ShouldOfferFidoOptInDialog(
    const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response) {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // We should not offer FIDO opt-in dialog on mobile.
  return false;
#else
  if (!unmask_details_.offer_fido_opt_in &&
      unmask_details_.fido_request_options.has_value()) {
    // Server instructed the client to not offer fido because the client is
    // already opted in. This can be verified with the presence of request
    // options in the server response.
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kAlreadyOptedIn);
    return false;
  }

  // If this card is not eligible for offering FIDO opt-in, we should not offer
  // the FIDO opt-in dialog. Since the client is not opted-in and device is
  // eligible, this could mean that the server does not have a valid key for
  // this device or the server is in a bad state.
  if (!unmask_details_.offer_fido_opt_in) {
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kUnmaskDetailsOfferFidoOptInFalse);
    return false;
  }

  // A card authorization token is required for FIDO opt-in, so if we did not
  // receive one from the server we should not offer the FIDO opt-in dialog.
  if (response.card_authorization_token.empty()) {
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kCardAuthorizationTokenEmpty);
    return false;
  }

  // If the strike limit was reached for the FIDO opt-in dialog, we should not
  // offer it.
  if (auto* strike_database =
          GetOrCreateFidoAuthenticator()
              ->GetOrCreateFidoAuthenticationStrikeDatabase();
      strike_database && strike_database->ShouldBlockFeature()) {
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kBlockedByStrikeDatabase);
    return false;
  }

  // We should not offer FIDO opt-in for virtual cards.
  if (!card_ || card_->record_type() == CreditCard::RecordType::kVirtualCard) {
    autofill_metrics::LogWebauthnOptInPromoNotOfferedReason(
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kVirtualCard);
    return false;
  }

  // None of the cases where we should not offer the FIDO opt-in dialog were
  // true, so we should offer it.
  return true;
#endif
}

void CreditCardAccessManager::ShowWebauthnOfferDialog(
    std::string card_authorization_token) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  GetOrCreateFidoAuthenticator()->OnWebauthnOfferDialogRequested(
      card_authorization_token);
  client_->ShowWebauthnOfferDialog(
      base::BindRepeating(&CreditCardAccessManager::HandleDialogUserResponse,
                          weak_ptr_factory_.GetWeakPtr()));
#endif
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void CreditCardAccessManager::ShowVerifyPendingDialog() {
  client_->ShowWebauthnVerifyPendingDialog(
      base::BindRepeating(&CreditCardAccessManager::HandleDialogUserResponse,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardAccessManager::HandleDialogUserResponse(
    WebauthnDialogCallbackType type) {
  switch (type) {
    case WebauthnDialogCallbackType::kOfferAccepted:
      GetOrCreateFidoAuthenticator()->OnWebauthnOfferDialogUserResponse(
          /*did_accept=*/true);
      break;
    case WebauthnDialogCallbackType::kOfferCancelled:
      GetOrCreateFidoAuthenticator()->OnWebauthnOfferDialogUserResponse(
          /*did_accept=*/false);
      break;
    case WebauthnDialogCallbackType::kVerificationCancelled:
      // TODO(crbug.com/949269): Add tests and logging for canceling verify
      // pending dialog.
      payments_client_->CancelRequest();
      SignalCanFetchUnmaskDetails();
      ready_to_start_authentication_.Reset();
      unmask_details_request_in_progress_ = false;
      GetOrCreateFidoAuthenticator()->CancelVerification();

      // Indicate that FIDO authentication was canceled, resulting in falling
      // back to CVC auth.
      CreditCardFidoAuthenticator::FidoAuthenticationResponse response{
          .did_succeed = false};
      OnFIDOAuthenticationComplete(response);
      break;
  }
}
#endif

std::string CreditCardAccessManager::GetKeyForUnmaskedCardsCache(
    const CreditCard& card) const {
  std::string key = card.server_id();
  if (card.record_type() == CreditCard::RecordType::kVirtualCard) {
    key += kVirtualCardIdentifier;
  }
  return key;
}

void CreditCardAccessManager::FetchMaskedServerCard() {
  is_authentication_in_progress_ = true;

  bool get_unmask_details_returned =
      ready_to_start_authentication_.IsSignaled();
  bool should_wait_to_authenticate =
      IsUserOptedInToFidoAuth() && !get_unmask_details_returned;

  // Latency metrics should only be logged if the user is verifiable.
#if !BUILDFLAG(IS_IOS)
  if (is_user_verifiable_.value_or(false)) {
    autofill_metrics::LogUserPerceivedLatencyOnCardSelection(
        get_unmask_details_returned
            ? autofill_metrics::PreflightCallEvent::
                  kPreflightCallReturnedBeforeCardChosen
            : autofill_metrics::PreflightCallEvent::
                  kCardChosenBeforePreflightCallReturned,
        GetOrCreateFidoAuthenticator()->IsUserOptedIn());
  }
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // On desktop, show the verify pending dialog for opted-in user, unless it is
  // already known that selected card requires CVC.
  if (IsUserOptedInToFidoAuth() &&
      (!get_unmask_details_returned || IsSelectedCardFidoAuthorized())) {
    ShowVerifyPendingDialog();
  }
#endif

  if (should_wait_to_authenticate) {
    card_selected_without_unmask_details_timestamp_ =
        AutofillTickClock::NowTicks();

    // Wait for |ready_to_start_authentication_| to be signaled by
    // OnDidGetUnmaskDetails() or until timeout before calling
    // OnStopWaitingForUnmaskDetails().
    ready_to_start_authentication_.OnEventOrTimeOut(
        base::BindOnce(&CreditCardAccessManager::OnStopWaitingForUnmaskDetails,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(kUnmaskDetailsResponseTimeoutMs));
  } else {
    StartAuthenticationFlow(
        IsFidoAuthEnabled(get_unmask_details_returned &&
                          unmask_details_.unmask_auth_method ==
                              AutofillClient::UnmaskAuthMethod::kFido));
  }
}

void CreditCardAccessManager::FetchVirtualCard() {
  is_authentication_in_progress_ = true;
  client_->ShowAutofillProgressDialog(
      AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog,
      base::BindOnce(&CreditCardAccessManager::OnVirtualCardUnmaskCancelled,
                     weak_ptr_factory_.GetWeakPtr()));

  // Send a risk-based unmasking request to server to attempt to fetch the card.
  absl::optional<GURL> last_committed_primary_main_frame_origin =
      client_->GetLastCommittedPrimaryMainFrameURL().DeprecatedGetOriginAsURL();
  if (!last_committed_primary_main_frame_origin.has_value()) {
    accessor_->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                   nullptr);
    autofill_metrics::LogServerCardUnmaskResult(
        autofill_metrics::ServerCardUnmaskResult::kUnexpectedError,
        AutofillClient::PaymentsRpcCardType::kVirtualCard,
        autofill_metrics::VirtualCardUnmaskFlowType::kUnspecified);
    Reset();
    return;
  }

  virtual_card_unmask_request_details_
      .last_committed_primary_main_frame_origin =
      last_committed_primary_main_frame_origin;
  virtual_card_unmask_request_details_.card = *card_;
  if (ShouldShowCardMetadata(*card_)) {
    virtual_card_unmask_request_details_.client_behavior_signals.push_back(
        ClientBehaviorConstants::kShowingCardArtImageAndCardProductName);
  }
  virtual_card_unmask_request_details_.billing_customer_number =
      payments::GetBillingCustomerId(personal_data_manager_);

  payments_client_->Prepare();
  client_->LoadRiskData(
      base::BindOnce(&CreditCardAccessManager::OnDidGetUnmaskRiskData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardAccessManager::FetchLocalOrFullServerCard() {
#if !BUILDFLAG(IS_IOS)
  // Latency metrics should only be logged if the user is verifiable.
  if (is_user_verifiable_.value_or(false)) {
    autofill_metrics::LogUserPerceivedLatencyOnCardSelection(
        autofill_metrics::PreflightCallEvent::kDidNotChooseMaskedCard,
        GetOrCreateFidoAuthenticator()->IsUserOptedIn());
  }
#endif

  // Check if we need to authenticate the user before filling the local card
  // or full server card.
  if (personal_data_manager_->IsPaymentMethodsMandatoryReauthEnabled()) {
    // `StartDeviceAuthenticationForFilling()` will asynchronously trigger
    // the re-authentication flow, so we should avoid calling `Reset()`
    // until the re-authentication flow is complete.
    StartDeviceAuthenticationForFilling(accessor_, card_.get(), /*cvc=*/u"");
  } else {
    // Fill immediately if local card, and we do not need to authenticate
    // the user.
    accessor_->OnCreditCardFetched(CreditCardFetchResult::kSuccess,
                                   card_.get());

    // This local card autofill flow did not have any interactive
    // authentication, so notify the FormDataImporter of this.
    client_->GetFormDataImporter()
        ->SetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted(
            FormDataImporter::CardGuid(card_->guid()));

    // `accessor_->OnCreditCardFetched()` makes a copy of `card` and `cvc`
    // before it asynchronously fills them into the form. Thus we can safely
    // call `Reset()` here, and we should as from this class' point of view the
    // authentication flow is complete.
    Reset();
  }
}

void CreditCardAccessManager::OnDidGetUnmaskRiskData(
    const std::string& risk_data) {
  virtual_card_unmask_request_details_.risk_data = risk_data;
  payments_client_->UnmaskCard(
      virtual_card_unmask_request_details_,
      base::BindOnce(
          &CreditCardAccessManager::OnVirtualCardUnmaskResponseReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardAccessManager::OnVirtualCardUnmaskResponseReceived(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::UnmaskResponseDetails& response_details) {
  selected_challenge_option_ = nullptr;
  virtual_card_unmask_response_details_ = response_details;
  if (result == AutofillClient::PaymentsRpcResult::kSuccess) {
    if (!response_details.real_pan.empty()) {
      // If the real pan is not empty, then complete card information has been
      // fetched from the server (this is ensured in Payments Client). Pass the
      // unmasked card to `accessor_` and end the session.
      DCHECK_EQ(response_details.card_type,
                AutofillClient::PaymentsRpcCardType::kVirtualCard);
      card_->SetNumber(base::UTF8ToUTF16(response_details.real_pan));
      card_->SetExpirationMonthFromString(
          base::UTF8ToUTF16(response_details.expiration_month),
          /*app_locale=*/std::string());
      card_->SetExpirationYearFromString(
          base::UTF8ToUTF16(response_details.expiration_year));
      // Check if we need to authenticate the user before filling the virtual
      // card.
      if (personal_data_manager_->IsPaymentMethodsMandatoryReauthEnabled()) {
        // On some operating systems (for example, macOS and Windows), the
        // device authentication prompt freezes Chrome. Thus we can only trigger
        // the prompt after the progress dialog has been closed, which we can do
        // by using the `no_interactive_authentication_callback` parameter in
        // `AutofillClient::CloseAutofillProgressDialog()`.
        // TODO(crbug.com/1427216): Implement this flow for Android as well.
        client_->CloseAutofillProgressDialog(
            /*show_confirmation_before_closing=*/false,
            /*no_interactive_authentication_callback=*/base::BindOnce(
                // `StartDeviceAuthenticationForFilling()` will asynchronously
                // trigger the re-authentication flow, so we should avoid
                // calling `Reset()` until the re-authentication flow is
                // complete.
                &CreditCardAccessManager::StartDeviceAuthenticationForFilling,
                weak_ptr_factory_.GetWeakPtr(), accessor_, card_.get(),
                base::UTF8ToUTF16(response_details.dcvv)));
      } else {
        client_->CloseAutofillProgressDialog(
            /*show_confirmation_before_closing=*/true);
        card_->set_cvc(base::UTF8ToUTF16(response_details.dcvv));
        accessor_->OnCreditCardFetched(CreditCardFetchResult::kSuccess,
                                       card_.get());

        // If the server responded with success and the real pan, no interactive
        // authentication happened. It's also possible that the server does not
        // provide the real pan but requests an authentication which is handled
        // below. In this case, since the virtual card has a randomly generated
        // GUID and is not stored in the autofill table, we must set the card
        // identifier as the last four digits of the virtual card.
        client_->GetFormDataImporter()
            ->SetCardIdentifierIfNonInteractiveAuthenticationFlowCompleted(
                FormDataImporter::CardLastFourDigits(
                    base::UTF16ToUTF8(card_->LastFourDigits())));

        autofill_metrics::LogServerCardUnmaskResult(
            autofill_metrics::ServerCardUnmaskResult::kRiskBasedUnmasked,
            AutofillClient::PaymentsRpcCardType::kVirtualCard,
            autofill_metrics::VirtualCardUnmaskFlowType::kUnspecified);
        // `accessor_->OnCreditCardFetched()` makes a copy of `card` and `cvc`
        // before it asynchronously fills them into the form. Thus we can safely
        // call `Reset()` here, and we should as from this class' point of view
        // the authentication flow is complete.
        Reset();
      }
      return;
    }

    // Otherwise further authentication is required to unmask the card.
    DCHECK(!response_details.context_token.empty());
    // Close the progress dialog without showing the confirmation.
    client_->CloseAutofillProgressDialog(
        /*show_confirmation_before_closing=*/false);
    StartAuthenticationFlow(
        IsFidoAuthEnabled(response_details.fido_request_options.has_value()));
    return;
  }

  // If RPC response contains any error, end the session and show the error
  // dialog. If RPC result is kVcnRetrievalPermanentFailure we show VCN
  // permanent error dialog, and for all other cases we show VCN temporary
  // error dialog.
  // Close the progress dialog without showing the confirmation.
  client_->CloseAutofillProgressDialog(
      /*show_confirmation_before_closing=*/false);
  accessor_->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                 nullptr);

  autofill_metrics::ServerCardUnmaskResult unmask_result;
  if (result ==
          AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
      result ==
          AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
    unmask_result =
        autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError;
  } else {
    unmask_result =
        autofill_metrics::ServerCardUnmaskResult::kAuthenticationError;
  }
  autofill_metrics::LogServerCardUnmaskResult(
      unmask_result, AutofillClient::PaymentsRpcCardType::kVirtualCard,
      autofill_metrics::VirtualCardUnmaskFlowType::kUnspecified);

  if (response_details.autofill_error_dialog_context) {
    DCHECK(
        response_details.autofill_error_dialog_context->server_returned_title &&
        response_details.autofill_error_dialog_context
            ->server_returned_description);

    // Error fields returned in the server response are more detailed than the
    // virtual card temporary/permanent error messages stored on the client, so
    // prefer the server-returned fields if they exist.
    client_->ShowAutofillErrorDialog(
        *response_details.autofill_error_dialog_context);
  } else {
    client_->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/result ==
            AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure));
  }
  Reset();
}

void CreditCardAccessManager::OnStopWaitingForUnmaskDetails(
    bool get_unmask_details_returned) {
  // If the user had to wait for Unmask Details, log the latency.
  if (card_selected_without_unmask_details_timestamp_.has_value()) {
    autofill_metrics::LogUserPerceivedLatencyOnCardSelectionDuration(
        AutofillTickClock::NowTicks() -
        card_selected_without_unmask_details_timestamp_.value());
    autofill_metrics::LogUserPerceivedLatencyOnCardSelectionTimedOut(
        /*did_time_out=*/!get_unmask_details_returned);
    card_selected_without_unmask_details_timestamp_ = absl::nullopt;
  }

  // Start the authentication after the wait ends.
  StartAuthenticationFlow(
      IsFidoAuthEnabled(get_unmask_details_returned &&
                        unmask_details_.unmask_auth_method ==
                            AutofillClient::UnmaskAuthMethod::kFido));
}

void CreditCardAccessManager::OnUserAcceptedAuthenticationSelectionDialog(
    const std::string& selected_challenge_option_id) {
  selected_challenge_option_ =
      GetCardUnmaskChallengeOptionForChallengeId(selected_challenge_option_id);

  // This if-statement should never be entered. We will only be selecting
  // challenge options that we passed into the controller from
  // `virtual_card_unmask_response_details_.card_unmask_challenge_options`,
  // which is also the vector that we search for challenge options in. For the
  // context token, the server is guaranteed to always return a VCN context
  // token for the virtual card authentication flow. This if-statement is just
  // here as a safety.
  if (!selected_challenge_option_ ||
      virtual_card_unmask_response_details_.context_token.empty()) {
    NOTREACHED();
    accessor_->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                   nullptr);
    client_->ShowAutofillErrorDialog(
        AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
            /*is_permanent_error=*/false));
    Reset();
    return;
  }

  UnmaskAuthFlowType selected_authentication_type = UnmaskAuthFlowType::kNone;
  switch (selected_challenge_option_->type) {
    case CardUnmaskChallengeOptionType::kCvc:
      selected_authentication_type = UnmaskAuthFlowType::kCvc;
      break;
    case CardUnmaskChallengeOptionType::kSmsOtp:
    case CardUnmaskChallengeOptionType::kEmailOtp:
      selected_authentication_type =
          unmask_auth_flow_type_ == UnmaskAuthFlowType::kFido
              ? UnmaskAuthFlowType::kOtpFallbackFromFido
              : UnmaskAuthFlowType::kOtp;
      break;
    case CardUnmaskChallengeOptionType::kUnknownType:
      NOTREACHED();
      break;
  }
  Authenticate(selected_authentication_type);
}

void CreditCardAccessManager::OnVirtualCardUnmaskCancelled() {
  accessor_->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                 nullptr);

  if (unmask_auth_flow_type_ == UnmaskAuthFlowType::kOtp ||
      unmask_auth_flow_type_ == UnmaskAuthFlowType::kOtpFallbackFromFido) {
    // It is possible to have the user hit the cancel button during an in-flight
    // Virtual Card Unmask request, so we need to reset the state of the
    // CreditCardOtpAuthenticator as well to ensure the flow does not continue,
    // as continuing the flow can cause a crash.
    client_->GetOtpAuthenticator()->Reset();
  }

  autofill_metrics::VirtualCardUnmaskFlowType flow_type;
  switch (unmask_auth_flow_type_) {
    case UnmaskAuthFlowType::kOtp:
      flow_type = autofill_metrics::VirtualCardUnmaskFlowType::kOtpOnly;
      break;
    case UnmaskAuthFlowType::kOtpFallbackFromFido:
      flow_type =
          autofill_metrics::VirtualCardUnmaskFlowType::kOtpFallbackFromFido;
      break;
    case UnmaskAuthFlowType::kNone:
      flow_type = autofill_metrics::VirtualCardUnmaskFlowType::kUnspecified;
      break;
    case UnmaskAuthFlowType::kFido:
    case UnmaskAuthFlowType::kCvcThenFido:
    case UnmaskAuthFlowType::kCvcFallbackFromFido:
      NOTREACHED();
      ABSL_FALLTHROUGH_INTENDED;
    case UnmaskAuthFlowType::kCvc:
      // TODO(crbug/1370329): Add a flow type for the CVC flow for metrics.
      Reset();
      return;
  }
  autofill_metrics::LogServerCardUnmaskResult(
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled,
      AutofillClient::PaymentsRpcCardType::kVirtualCard, flow_type);
  Reset();
}

void CreditCardAccessManager::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;
  is_authentication_in_progress_ = false;
  preflight_call_timestamp_ = absl::nullopt;
  card_selected_without_unmask_details_timestamp_ = absl::nullopt;
  is_user_verifiable_called_timestamp_ = absl::nullopt;
#if !BUILDFLAG(IS_IOS)
  opt_in_intention_ = UserOptInIntention::kUnspecified;
#endif
  unmask_details_ = payments::PaymentsClient::UnmaskDetails();
  virtual_card_unmask_request_details_ =
      payments::PaymentsClient::UnmaskRequestDetails();
  selected_challenge_option_ = nullptr;
  virtual_card_unmask_response_details_ =
      payments::PaymentsClient::UnmaskResponseDetails();
  ready_to_start_authentication_.Reset();
  can_fetch_unmask_details_ = true;
  card_.reset();
  unmask_details_request_in_progress_ = false;
}

void CreditCardAccessManager::HandleFidoOptInStatusChange() {
#if !BUILDFLAG(IS_IOS)
  // If user intended to opt out, we will opt user out after CVC/OTP auth
  // completes (no matter it succeeded or failed).
  if (opt_in_intention_ == UserOptInIntention::kIntentToOptOut) {
    FIDOAuthOptChange(/*opt_in=*/false);
  }
  // Reset |opt_in_intention_| after the authentication completes.
  opt_in_intention_ = UserOptInIntention::kUnspecified;
#endif
}

void CreditCardAccessManager::ShowUnmaskAuthenticatorSelectionDialog() {
  client_->ShowUnmaskAuthenticatorSelectionDialog(
      virtual_card_unmask_response_details_.card_unmask_challenge_options,
      base::BindOnce(
          &CreditCardAccessManager::OnUserAcceptedAuthenticationSelectionDialog,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CreditCardAccessManager::OnVirtualCardUnmaskCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
}

CardUnmaskChallengeOption*
CreditCardAccessManager::GetCardUnmaskChallengeOptionForChallengeId(
    const std::string& challenge_id) {
  std::vector<CardUnmaskChallengeOption>& challenge_options =
      virtual_card_unmask_response_details_.card_unmask_challenge_options;
  auto card_unmask_challenge_options_it = base::ranges::find(
      challenge_options,
      CardUnmaskChallengeOption::ChallengeOptionId(challenge_id),
      &CardUnmaskChallengeOption::id);
  return card_unmask_challenge_options_it != challenge_options.end()
             ? &(*card_unmask_challenge_options_it)
             : nullptr;
}

bool CreditCardAccessManager::ShouldLogServerCardUnmaskAttemptMetrics(
    CreditCard::RecordType record_type) {
  // We always want to log virtual card unmask attempts.
  if (record_type == CreditCard::RecordType::kVirtualCard) {
    return true;
  }

  // We only want to log masked server card or full server card unmask
  // attempts if the `kAutofillEnableRemadeDownstreamMetrics` feature flag is
  // enabled, due to this being a histogram refactoring that we want to roll out
  // slowly to ensure that it works properly.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableRemadeDownstreamMetrics)) {
    return record_type == CreditCard::RecordType::kMaskedServerCard ||
           record_type == CreditCard::RecordType::kFullServerCard;
  }

  // No conditions were met to log a server card unmasking attempt, so return
  // false.
  return false;
}

void CreditCardAccessManager::StartDeviceAuthenticationForFilling(
    base::WeakPtr<Accessor> accessor,
    const CreditCard* card,
    const std::u16string& cvc) {
  is_authentication_in_progress_ = true;

  CreditCard::RecordType record_type = card->record_type();
  CHECK(record_type == CreditCard::RecordType::kLocalCard ||
        record_type == CreditCard::RecordType::kVirtualCard);
  payments::MandatoryReauthAuthenticationMethod authentication_method =
      client_->GetOrCreatePaymentsMandatoryReauthManager()
          ->GetAuthenticationMethod();

  autofill_metrics::LogMandatoryReauthCheckoutFlowUsageEvent(
      record_type, authentication_method,
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted);
  // TODO(crbug.com/1427216): Add the iOS branching logic as well.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  client_->GetOrCreatePaymentsMandatoryReauthManager()->AuthenticateWithMessage(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_AUTOFILL_FILLING_MANDATORY_REAUTH),
      base::BindOnce(
          &CreditCardAccessManager::OnDeviceAuthenticationResponseForFilling,
          weak_ptr_factory_.GetWeakPtr(), accessor, authentication_method, card,
          cvc));
#elif BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1427216): Convert this to
  // MandatoryReauthManager::AuthenticateWithMessage() with the correct message
  // once it is supported. Currently, the message is "Verify it's you".
  client_->GetOrCreatePaymentsMandatoryReauthManager()->Authenticate(
      record_type == CreditCard::RecordType::kLocalCard
          ? device_reauth::DeviceAuthRequester::kLocalCardAutofill
          : device_reauth::DeviceAuthRequester::kVirtualCardAutofill,
      base::BindOnce(
          &CreditCardAccessManager::OnDeviceAuthenticationResponseForFilling,
          weak_ptr_factory_.GetWeakPtr(), accessor, authentication_method, card,
          cvc));
#else
  NOTREACHED_NORETURN();
#endif
}

void CreditCardAccessManager::OnDeviceAuthenticationResponseForFilling(
    base::WeakPtr<Accessor> accessor,
    payments::MandatoryReauthAuthenticationMethod authentication_method,
    const CreditCard* card,
    const std::u16string& cvc,
    bool successful_auth) {
  autofill_metrics::LogMandatoryReauthCheckoutFlowUsageEvent(
      card->record_type(), authentication_method,
      successful_auth
          ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                kFlowSucceeded
          : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                kFlowFailed);
  CHECK(card);
  CreditCard card_with_cvc = *card;
  card_with_cvc.set_cvc(cvc);
  accessor->OnCreditCardFetched(successful_auth
                                    ? CreditCardFetchResult::kSuccess
                                    : CreditCardFetchResult::kTransientError,
                                &card_with_cvc);
  // TODO(crbug.com/1427216): Add logging for the payments autofill device
  // authentication flow.
  // `accessor->OnCreditCardFetched()` makes a copy of `card` and `cvc` before
  // it asynchronously fills them into the form. Thus we can safely call
  // `Reset()` here, and we should as from this class' point of view the
  // authentication flow is complete.
  Reset();
}

}  // namespace autofill
