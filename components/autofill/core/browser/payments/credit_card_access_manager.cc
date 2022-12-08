// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/functional/not_fn.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
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
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
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
#include "components/autofill/core/browser/payments/fido_authentication_strike_database.h"
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
    CreditCardFormEventLogger* form_event_logger)
    : driver_(driver),
      client_(client),
      payments_client_(client_->GetPaymentsClient()),
      personal_data_manager_(personal_data_manager),
      form_event_logger_(form_event_logger) {}

CreditCardAccessManager::~CreditCardAccessManager() = default;

void CreditCardAccessManager::UpdateCreditCardFormEventLogger() {
  std::vector<CreditCard*> credit_cards =
      personal_data_manager_->GetCreditCards();

  size_t server_record_type_count = 0;
  size_t local_record_type_count = 0;
  for (CreditCard* credit_card : credit_cards) {
    if (credit_card->record_type() == CreditCard::LOCAL_CARD)
      local_record_type_count++;
    else
      server_record_type_count++;
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
    title->assign(card->CardIdentifierStringForAutofillDisplay());
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
  if (!base::ranges::any_of(personal_data_manager_->GetCreditCardsToSuggest(
                                client_->AreServerCardsSupported()),
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

  // If |is_user_verifiable_| is set, then directly call
  // GetUnmaskDetailsIfUserIsVerifiable(), otherwise fetch value for
  // |is_user_verifiable_|.
  if (is_user_verifiable_.has_value()) {
    GetUnmaskDetailsIfUserIsVerifiable(is_user_verifiable_.value());
  } else {
    is_user_verifiable_called_timestamp_ = AutofillTickClock::NowTicks();

    GetOrCreateFIDOAuthenticator()->IsUserVerifiable(base::BindOnce(
        &CreditCardAccessManager::GetUnmaskDetailsIfUserIsVerifiable,
        weak_ptr_factory_.GetWeakPtr()));
  }
#endif
}

void CreditCardAccessManager::GetUnmaskDetailsIfUserIsVerifiable(
    bool is_user_verifiable) {
  is_user_verifiable_ = is_user_verifiable;

  if (is_user_verifiable_called_timestamp_.has_value()) {
    AutofillMetrics::LogUserVerifiabilityCheckDuration(
        AutofillTickClock::NowTicks() -
        is_user_verifiable_called_timestamp_.value());
  }

  // If user is verifiable, then make preflight call to payments to fetch unmask
  // details, otherwise the only option is to perform CVC Auth, which does not
  // require any. Do nothing if request is already in progress.
  if (is_user_verifiable_.value_or(false) &&
      !unmask_details_request_in_progress_) {
    unmask_details_request_in_progress_ = true;
    preflight_call_timestamp_ = AutofillTickClock::NowTicks();
    payments_client_->GetUnmaskDetails(
        base::BindOnce(&CreditCardAccessManager::OnDidGetUnmaskDetails,
                       weak_ptr_factory_.GetWeakPtr()),
        personal_data_manager_->app_locale());
    AutofillMetrics::LogCardUnmaskPreflightCalled();
  }
}

void CreditCardAccessManager::OnDidGetUnmaskDetails(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::UnmaskDetails& unmask_details) {
  // Log latency for preflight call.
  if (preflight_call_timestamp_.has_value()) {
    AutofillMetrics::LogCardUnmaskPreflightDuration(
        AutofillTickClock::NowTicks() - *preflight_call_timestamp_);
  }

  unmask_details_request_in_progress_ = false;
  unmask_details_ = unmask_details;
  unmask_details_.offer_fido_opt_in = unmask_details_.offer_fido_opt_in &&
                                      !payments_client_->is_off_the_record();

  // Set delay as fido request timeout if available, otherwise set to default.
  int delay_ms = kDelayForGetUnmaskDetails;
  if (unmask_details_.fido_request_options.has_value()) {
    const auto* request_timeout =
        unmask_details_.fido_request_options->FindKeyOfType(
            "timeout_millis", base::Value::Type::INTEGER);
    if (request_timeout)
      delay_ms = request_timeout->GetInt();
  }

#if !BUILDFLAG(IS_IOS)
  opt_in_intention_ =
      GetOrCreateFIDOAuthenticator()->GetUserOptInIntention(unmask_details);
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
                                  nullptr, u"");
    return;
  }

  // If card is nullptr we reset all states and return error.
  if (!card) {
    accessor->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                  nullptr, u"");
    Reset();
    return;
  }

  // Log the server card unmasking attempt, and differentiate based on server
  // card or virtual card.
  CreditCard::RecordType record_type = card->record_type();
  if (ShouldLogServerCardUnmaskAttemptMetrics(record_type)) {
    AutofillMetrics::LogServerCardUnmaskAttempt(
        record_type == CreditCard::VIRTUAL_CARD
            ? AutofillClient::PaymentsRpcCardType::kVirtualCard
            : AutofillClient::PaymentsRpcCardType::kServerCard);
  }

  // If card has been previously unmasked, use cached data.
  std::unordered_map<std::string, CachedServerCardInfo>::iterator it =
      unmasked_card_cache_.find(GetKeyForUnmaskedCardsCache(*card));
  if (it != unmasked_card_cache_.end()) {  // key is in cache
    accessor->OnCreditCardFetched(CreditCardFetchResult::kSuccess,
                                  /*credit_card=*/&it->second.card,
                                  /*cvc=*/it->second.cvc);
    std::string metrics_name = card->record_type() == CreditCard::VIRTUAL_CARD
                                   ? "Autofill.UsedCachedVirtualCard"
                                   : "Autofill.UsedCachedServerCard";
    base::UmaHistogramCounts1000(metrics_name, ++it->second.cache_uses);
    if (card->record_type() == CreditCard::VIRTUAL_CARD) {
      AutofillMetrics::LogServerCardUnmaskResult(
          AutofillMetrics::ServerCardUnmaskResult::kLocalCacheHit,
          AutofillClient::PaymentsRpcCardType::kVirtualCard,
          AutofillMetrics::VirtualCardUnmaskFlowType::kUnspecified);
    }

    Reset();
    return;
  }

  // Return immediately if local card and log that unmask details were ignored.
  if (card->record_type() != CreditCard::MASKED_SERVER_CARD &&
      card->record_type() != CreditCard::VIRTUAL_CARD) {
    accessor->OnCreditCardFetched(CreditCardFetchResult::kSuccess, card, u"");
#if !BUILDFLAG(IS_IOS)
    // Latency metrics should only be logged if the user is verifiable.
    if (is_user_verifiable_.value_or(false)) {
      AutofillMetrics::LogUserPerceivedLatencyOnCardSelection(
          AutofillMetrics::PreflightCallEvent::kDidNotChooseMaskedCard,
          GetOrCreateFIDOAuthenticator()->IsUserOptedIn());
    }
#endif
    Reset();
    return;
  }

  card_ = std::make_unique<CreditCard>(*card);
  accessor_ = accessor;

  // Direct to different flows based on the card record type.
  if (card_->record_type() == CreditCard::VIRTUAL_CARD)
    FetchVirtualCard();
  else
    FetchMaskedServerCard();
}

void CreditCardAccessManager::FIDOAuthOptChange(bool opt_in) {
#if BUILDFLAG(IS_IOS)
  return;
#else
  if (opt_in) {
    ShowWebauthnOfferDialog(/*card_authorization_token=*/std::string());
  } else {
    GetOrCreateFIDOAuthenticator()->OptOut();
    GetOrCreateFIDOAuthenticator()
        ->GetOrCreateFidoAuthenticationStrikeDatabase()
        ->AddStrikes(
            FidoAuthenticationStrikeDatabase::kStrikesToAddWhenUserOptsOut);
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
  DCHECK(card.record_type() == CreditCard::FULL_SERVER_CARD ||
         card.record_type() == CreditCard::VIRTUAL_CARD);
  std::string identifier = card.record_type() == CreditCard::VIRTUAL_CARD
                               ? card.server_id() + kVirtualCardIdentifier
                               : card.server_id();
  CachedServerCardInfo card_info = {card, cvc, /*cache_uses=*/0};
  unmasked_card_cache_[identifier] = card_info;
}

void CreditCardAccessManager::GetAuthenticationType(bool fido_auth_enabled) {
#if BUILDFLAG(IS_IOS)
  // There is no FIDO auth available on iOS and there are no virtual cards on
  // iOS either, so offer CVC auth immediately.
  OnDidGetAuthenticationType(UnmaskAuthFlowType::kCvc);
#else
  if (card_->record_type() == CreditCard::VIRTUAL_CARD)
    GetAuthenticationTypeForVirtualCard(fido_auth_enabled);
  else
    GetAuthenticationTypeForMaskedServerCard(fido_auth_enabled);
#endif
}

void CreditCardAccessManager::GetAuthenticationTypeForVirtualCard(
    bool fido_auth_enabled) {
  // TODO(crbug.com/1243475): Currently if the card is a virtual card and FIDO
  // auth was provided by issuer, we prefer FIDO auth. Remove FIDO preference
  // and allow user selections later.
  if (fido_auth_enabled) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    ShowVerifyPendingDialog();
#endif
    OnDidGetAuthenticationType(UnmaskAuthFlowType::kFido);
    return;
  }

  // Otherwise, we first check if other options are provided. If not, end the
  // session and return an error.
  std::vector<CardUnmaskChallengeOption>& challenge_options =
      virtual_card_unmask_response_details_.card_unmask_challenge_options;
  if (challenge_options.empty()) {
    accessor_->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                   nullptr, u"");
    client_->ShowVirtualCardErrorDialog(
        AutofillErrorDialogContext::WithPermanentOrTemporaryError(
            /*is_permanent_error=*/true));
    Reset();
    AutofillMetrics::LogServerCardUnmaskResult(
        AutofillMetrics::ServerCardUnmaskResult::
            kOnlyFidoAvailableButNotOptedIn,
        AutofillClient::PaymentsRpcCardType::kVirtualCard,
        AutofillMetrics::VirtualCardUnmaskFlowType::kFidoOnly);
    return;
  }

  // If we only have one challenge option, and it is a CVC challenge option, go
  // directly to the CVC input dialog.
  if (challenge_options.size() == 1 &&
      challenge_options[0].type == CardUnmaskChallengeOptionType::kCvc) {
    selected_challenge_option_ = &challenge_options[0];
    OnDidGetAuthenticationType(UnmaskAuthFlowType::kCvc);
    return;
  }

  // If we have multiple challenge options available, render the challenge
  // option selection dialog. This dialog also handles the case where we only
  // have an OTP challenge option.
  ShowUnmaskAuthenticatorSelectionDialog();
}

void CreditCardAccessManager::GetAuthenticationTypeForMaskedServerCard(
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

  OnDidGetAuthenticationType(flow_type);
}

void CreditCardAccessManager::OnDidGetAuthenticationType(
    UnmaskAuthFlowType unmask_auth_flow_type) {
  unmask_auth_flow_type_ = unmask_auth_flow_type;

  // If FIDO auth was suggested, log which authentication method was
  // actually used.
  switch (unmask_auth_flow_type_) {
    case UnmaskAuthFlowType::kFido:
      AutofillMetrics::LogCardUnmaskTypeDecision(
          AutofillMetrics::CardUnmaskTypeDecisionMetric::kFidoOnly);
      break;
    case UnmaskAuthFlowType::kCvcThenFido:
      AutofillMetrics::LogCardUnmaskTypeDecision(
          AutofillMetrics::CardUnmaskTypeDecisionMetric::kCvcThenFido);
      break;
    case UnmaskAuthFlowType::kCvc:
    case UnmaskAuthFlowType::kOtp:
    case UnmaskAuthFlowType::kOtpFallbackFromFido:
      break;
    case UnmaskAuthFlowType::kNone:
    case UnmaskAuthFlowType::kCvcFallbackFromFido:
      NOTREACHED();
      break;
  }

  Authenticate();
}

void CreditCardAccessManager::Authenticate() {
  // Reset now that we have started authentication.
  ready_to_start_authentication_.Reset();
  unmask_details_request_in_progress_ = false;

  form_event_logger_->LogCardUnmaskAuthenticationPromptShown(
      unmask_auth_flow_type_);

  switch (unmask_auth_flow_type_) {
    case UnmaskAuthFlowType::kFido: {
#if BUILDFLAG(IS_IOS)
      NOTREACHED();
#else
      // If |is_authentication_in_progress_| is false, it means the process has
      // been cancelled via the verification pending dialog. Do not run
      // CreditCardFIDOAuthenticator::Authenticate() in this case (should not
      // fall back to CVC auth either).
      if (!is_authentication_in_progress_) {
        Reset();
        return;
      }

      // For virtual cards the |fido_request_option| comes from the
      // UnmaskResponseDetails while for masked server cards, it comes from the
      // UnmaskDetails.
      base::Value fido_request_options;
      absl::optional<std::string> context_token;
      if (card_->record_type() == CreditCard::VIRTUAL_CARD) {
        context_token = virtual_card_unmask_response_details_.context_token;
        fido_request_options = std::move(
            virtual_card_unmask_response_details_.fido_request_options.value());
      } else {
        fido_request_options =
            std::move(unmask_details_.fido_request_options.value());
      }
      GetOrCreateFIDOAuthenticator()->Authenticate(
          card_.get(), weak_ptr_factory_.GetWeakPtr(),
          std::move(fido_request_options), context_token);
#endif
      break;
    }
    case UnmaskAuthFlowType::kCvcThenFido:
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
      if (card_->record_type() == CreditCard::VIRTUAL_CARD) {
        DCHECK(selected_challenge_option_);
        client_->GetCVCAuthenticator()->Authenticate(
            card_.get(), weak_ptr_factory_.GetWeakPtr(), personal_data_manager_,
            virtual_card_unmask_response_details_.context_token,
            *selected_challenge_option_);
      } else {
        client_->GetCVCAuthenticator()->Authenticate(
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
CreditCardFIDOAuthenticator*
CreditCardAccessManager::GetOrCreateFIDOAuthenticator() {
  if (!fido_authenticator_)
    fido_authenticator_ =
        std::make_unique<CreditCardFIDOAuthenticator>(driver_, client_);
  return fido_authenticator_.get();
}
#endif

void CreditCardAccessManager::OnCVCAuthenticationComplete(
    const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response) {
  is_authentication_in_progress_ = false;
  can_fetch_unmask_details_ = true;

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
                                   response.card, response.cvc);
    unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;
  } else if (should_register_card_with_fido) {
#if !BUILDFLAG(IS_IOS)
    absl::optional<base::Value> request_options = absl::nullopt;
    if (unmask_details_.fido_request_options.has_value()) {
      // For opted-in user (CVC then FIDO case), request options are returned in
      // unmask detail response.
      request_options = unmask_details_.fido_request_options->Clone();
    } else if (response.request_options.has_value()) {
      // For Android users, request_options are provided from GetRealPan if the
      // user has chosen to opt-in.
      request_options = response.request_options->Clone();
    }

    // Save credit card for after authorization.
    card_ = std::make_unique<CreditCard>(*(response.card));
    cvc_ = response.cvc;

    // Additionally authorizes the card with FIDO. It also delays the form
    // filling.
    GetOrCreateFIDOAuthenticator()->Authorize(weak_ptr_factory_.GetWeakPtr(),
                                              response.card_authorization_token,
                                              request_options->Clone());
#endif
  }
  if (ShouldOfferFidoOptInDialog(response)) {
    // CreditCardFIDOAuthenticator will handle enrollment completely.
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
  // We should not display the FIDO opt-in checkbox for virtual cards.
  if (card_->record_type() == CreditCard::VIRTUAL_CARD)
    return false;

  // If the user opted-in through the settings page, do not show checkbox.
  return unmask_details_.offer_fido_opt_in &&
         opt_in_intention_ != UserOptInIntention::kIntentToOptIn;
}

bool CreditCardAccessManager::UserOptedInToFidoFromSettingsPageOnMobile()
    const {
  return opt_in_intention_ == UserOptInIntention::kIntentToOptIn;
}
#endif

#if !BUILDFLAG(IS_IOS)
void CreditCardAccessManager::OnFIDOAuthenticationComplete(
    const CreditCardFIDOAuthenticator::FidoAuthenticationResponse& response) {
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
    accessor_->OnCreditCardFetched(response.did_succeed
                                       ? CreditCardFetchResult::kSuccess
                                       : CreditCardFetchResult::kTransientError,
                                   response.card, response.cvc);
    form_event_logger_->LogCardUnmaskAuthenticationPromptCompleted(
        unmask_auth_flow_type_);
    if (card_->record_type() == CreditCard::VIRTUAL_CARD) {
      AutofillMetrics::LogServerCardUnmaskResult(
          AutofillMetrics::ServerCardUnmaskResult::kAuthenticationUnmasked,
          AutofillClient::PaymentsRpcCardType::kVirtualCard,
          AutofillMetrics::VirtualCardUnmaskFlowType::kFidoOnly);
    }
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
    client_->ShowVirtualCardErrorDialog(
        AutofillErrorDialogContext::WithPermanentOrTemporaryError(
            /*is_permanent_error=*/response.failure_type ==
            payments::FullCardRequest::
                VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE));
    accessor_->OnCreditCardFetched(result, nullptr, u"");

    if (card_->record_type() == CreditCard::VIRTUAL_CARD) {
      AutofillMetrics::LogServerCardUnmaskResult(
          AutofillMetrics::ServerCardUnmaskResult::kVirtualCardRetrievalError,
          AutofillClient::PaymentsRpcCardType::kVirtualCard,
          AutofillMetrics::VirtualCardUnmaskFlowType::kFidoOnly);
    }
    Reset();
  } else {
    // If it is an authentication error, start the CVC authentication process
    // for masked server cards or the virtual card authentication process for
    // virtual cards.
    if (card_->record_type() == CreditCard::VIRTUAL_CARD) {
      GetAuthenticationTypeForVirtualCard(/*fido_auth_enabled=*/false);
    } else {
      unmask_auth_flow_type_ = UnmaskAuthFlowType::kCvcFallbackFromFido;
      Authenticate();
    }
  }
}

void CreditCardAccessManager::OnFidoAuthorizationComplete(bool did_succeed) {
  if (did_succeed) {
    accessor_->OnCreditCardFetched(CreditCardFetchResult::kSuccess, card_.get(),
                                   cvc_);
    form_event_logger_->LogCardUnmaskAuthenticationPromptCompleted(
        unmask_auth_flow_type_);
  }
  Reset();
}
#endif

void CreditCardAccessManager::OnOtpAuthenticationComplete(
    const CreditCardOtpAuthenticator::OtpAuthenticationResponse& response) {
  accessor_->OnCreditCardFetched(
      response.result == CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                             Result::kSuccess
          ? CreditCardFetchResult::kSuccess
          : CreditCardFetchResult::kTransientError,
      response.card, response.cvc);

  AutofillMetrics::ServerCardUnmaskResult result;
  switch (response.result) {
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kSuccess:
      result = AutofillMetrics::ServerCardUnmaskResult::kAuthenticationUnmasked;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kFlowCancelled:
      result = AutofillMetrics::ServerCardUnmaskResult::kFlowCancelled;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kGenericError:
      result = AutofillMetrics::ServerCardUnmaskResult::kUnexpectedError;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kAuthenticationError:
      result = AutofillMetrics::ServerCardUnmaskResult::kAuthenticationError;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kVirtualCardRetrievalError:
      result =
          AutofillMetrics::ServerCardUnmaskResult::kVirtualCardRetrievalError;
      break;
    case CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::
        kUnknown:
      NOTREACHED();
      return;
  }

  AutofillMetrics::VirtualCardUnmaskFlowType flow_type;
  if (unmask_auth_flow_type_ == UnmaskAuthFlowType::kOtp) {
    flow_type = AutofillMetrics::VirtualCardUnmaskFlowType::kOtpOnly;
  } else {
    DCHECK(unmask_auth_flow_type_ == UnmaskAuthFlowType::kOtpFallbackFromFido);
    flow_type =
        AutofillMetrics::VirtualCardUnmaskFlowType::kOtpFallbackFromFido;
  }
  AutofillMetrics::LogServerCardUnmaskResult(
      result, AutofillClient::PaymentsRpcCardType::kVirtualCard, flow_type);

  HandleFidoOptInStatusChange();
  Reset();
}

bool CreditCardAccessManager::IsUserOptedInToFidoAuth() {
#if BUILDFLAG(IS_IOS)
  return false;
#else
  return is_user_verifiable_.value_or(false) &&
         GetOrCreateFIDOAuthenticator()->IsUserOptedIn();
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
    const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response) {
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
    const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response) {
  // Card authorization token is required in order to call
  // CreditCardFIDOAuthenticator::Authorize(), so if we do not have a card
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
    const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response) {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // We should not offer FIDO opt-in dialog on mobile.
  return false;
#else
  // We should not offer FIDO opt-in for virtual cards.
  if (!card_ || card_->record_type() == CreditCard::VIRTUAL_CARD)
    return false;

  // If this card is not eligible for offering FIDO opt-in, we should not offer
  // the FIDO opt-in dialog.
  if (!unmask_details_.offer_fido_opt_in)
    return false;

  // A card authorization token is required for FIDO opt-in, so if we did not
  // receive one from the server we should not offer the FIDO opt-in dialog.
  if (response.card_authorization_token.empty())
    return false;

  // If the strike limit was reached for the FIDO opt-in dialog, we should not
  // offer it.
  if (GetOrCreateFIDOAuthenticator()
          ->GetOrCreateFidoAuthenticationStrikeDatabase()
          ->ShouldBlockFeature()) {
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
  GetOrCreateFIDOAuthenticator()->OnWebauthnOfferDialogRequested(
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
      GetOrCreateFIDOAuthenticator()->OnWebauthnOfferDialogUserResponse(
          /*did_accept=*/true);
      break;
    case WebauthnDialogCallbackType::kOfferCancelled:
      GetOrCreateFIDOAuthenticator()->OnWebauthnOfferDialogUserResponse(
          /*did_accept=*/false);
      break;
    case WebauthnDialogCallbackType::kVerificationCancelled:
      // TODO(crbug.com/949269): Add tests and logging for canceling verify
      // pending dialog.
      payments_client_->CancelRequest();
      SignalCanFetchUnmaskDetails();
      ready_to_start_authentication_.Reset();
      unmask_details_request_in_progress_ = false;
      GetOrCreateFIDOAuthenticator()->CancelVerification();

      // Indicate that FIDO authentication was canceled, resulting in falling
      // back to CVC auth.
      CreditCardFIDOAuthenticator::FidoAuthenticationResponse response{
          .did_succeed = false};
      OnFIDOAuthenticationComplete(response);
      break;
  }
}
#endif

std::string CreditCardAccessManager::GetKeyForUnmaskedCardsCache(
    const CreditCard& card) const {
  std::string key = card.server_id();
  if (card.record_type() == CreditCard::VIRTUAL_CARD)
    key += kVirtualCardIdentifier;
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
    AutofillMetrics::LogUserPerceivedLatencyOnCardSelection(
        get_unmask_details_returned
            ? AutofillMetrics::PreflightCallEvent::
                  kPreflightCallReturnedBeforeCardChosen
            : AutofillMetrics::PreflightCallEvent::
                  kCardChosenBeforePreflightCallReturned,
        GetOrCreateFIDOAuthenticator()->IsUserOptedIn());
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
    GetAuthenticationType(
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
                                   nullptr, u"");
    AutofillMetrics::LogServerCardUnmaskResult(
        AutofillMetrics::ServerCardUnmaskResult::kUnexpectedError,
        AutofillClient::PaymentsRpcCardType::kVirtualCard,
        AutofillMetrics::VirtualCardUnmaskFlowType::kUnspecified);
    Reset();
    return;
  }

  virtual_card_unmask_request_details_
      .last_committed_primary_main_frame_origin =
      last_committed_primary_main_frame_origin;
  virtual_card_unmask_request_details_.card = *card_;
  virtual_card_unmask_request_details_.billing_customer_number =
      payments::GetBillingCustomerId(personal_data_manager_);

  payments_client_->Prepare();
  client_->LoadRiskData(
      base::BindOnce(&CreditCardAccessManager::OnDidGetUnmaskRiskData,
                     weak_ptr_factory_.GetWeakPtr()));
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
  virtual_card_unmask_response_details_ = response_details;
  if (result == AutofillClient::PaymentsRpcResult::kSuccess) {
    if (!response_details.real_pan.empty()) {
      // Show confirmation on the progress dialog and then dismiss it.
      client_->CloseAutofillProgressDialog(
          /*show_confirmation_before_closing=*/true);

      // If the real pan is not empty, then complete card information has been
      // fetched from the server (this is ensured in Payments Client). Pass the
      // unmasked card to |accessor_| and end the session.
      CreditCard card = *card_;
      DCHECK_EQ(response_details.card_type,
                AutofillClient::PaymentsRpcCardType::kVirtualCard);
      card.SetNumber(base::UTF8ToUTF16(response_details.real_pan));
      card.SetExpirationMonthFromString(
          base::UTF8ToUTF16(response_details.expiration_month),
          /*app_locale=*/std::string());
      card.SetExpirationYearFromString(
          base::UTF8ToUTF16(response_details.expiration_year));
      accessor_->OnCreditCardFetched(CreditCardFetchResult::kSuccess, &card,
                                     base::UTF8ToUTF16(response_details.dcvv));
      AutofillMetrics::LogServerCardUnmaskResult(
          AutofillMetrics::ServerCardUnmaskResult::kRiskBasedUnmasked,
          AutofillClient::PaymentsRpcCardType::kVirtualCard,
          AutofillMetrics::VirtualCardUnmaskFlowType::kUnspecified);
      Reset();
      return;
    }

    // Otherwise further authentication is required to unmask the card.
    DCHECK(!response_details.context_token.empty());
    // Close the progress dialog without showing the confirmation.
    client_->CloseAutofillProgressDialog(
        /*show_confirmation_before_closing=*/false);
    GetAuthenticationType(
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
                                 nullptr, u"");

  AutofillMetrics::ServerCardUnmaskResult unmask_result;
  if (result ==
          AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure ||
      result ==
          AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure) {
    unmask_result =
        AutofillMetrics::ServerCardUnmaskResult::kVirtualCardRetrievalError;
  } else {
    unmask_result =
        AutofillMetrics::ServerCardUnmaskResult::kAuthenticationError;
  }
  AutofillMetrics::LogServerCardUnmaskResult(
      unmask_result, AutofillClient::PaymentsRpcCardType::kVirtualCard,
      AutofillMetrics::VirtualCardUnmaskFlowType::kUnspecified);

  if (response_details.autofill_error_dialog_context) {
    DCHECK(
        response_details.autofill_error_dialog_context->server_returned_title &&
        response_details.autofill_error_dialog_context
            ->server_returned_description);

    // Error fields returned in the server response are more detailed than the
    // virtual card temporary/permanent error messages stored on the client, so
    // prefer the server-returned fields if they exist.
    client_->ShowVirtualCardErrorDialog(
        *response_details.autofill_error_dialog_context);
  } else {
    client_->ShowVirtualCardErrorDialog(
        AutofillErrorDialogContext::WithPermanentOrTemporaryError(
            /*is_permanent_error=*/result ==
            AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure));
  }
  Reset();
}

void CreditCardAccessManager::OnStopWaitingForUnmaskDetails(
    bool get_unmask_details_returned) {
  // If the user had to wait for Unmask Details, log the latency.
  if (card_selected_without_unmask_details_timestamp_.has_value()) {
    AutofillMetrics::LogUserPerceivedLatencyOnCardSelectionDuration(
        AutofillTickClock::NowTicks() -
        card_selected_without_unmask_details_timestamp_.value());
    AutofillMetrics::LogUserPerceivedLatencyOnCardSelectionTimedOut(
        /*did_time_out=*/!get_unmask_details_returned);
    card_selected_without_unmask_details_timestamp_ = absl::nullopt;
  }

  // Start the authentication after the wait ends.
  GetAuthenticationType(
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
                                   nullptr, u"");
    client_->ShowVirtualCardErrorDialog(
        AutofillErrorDialogContext::WithPermanentOrTemporaryError(
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
      selected_authentication_type =
          unmask_auth_flow_type_ == UnmaskAuthFlowType::kFido
              ? UnmaskAuthFlowType::kOtpFallbackFromFido
              : UnmaskAuthFlowType::kOtp;
      break;
    case CardUnmaskChallengeOptionType::kUnknownType:
      NOTREACHED();
      break;
  }
  OnDidGetAuthenticationType(selected_authentication_type);
}

void CreditCardAccessManager::OnVirtualCardUnmaskCancelled() {
  accessor_->OnCreditCardFetched(CreditCardFetchResult::kTransientError,
                                 nullptr, u"");

  if (unmask_auth_flow_type_ == UnmaskAuthFlowType::kOtp ||
      unmask_auth_flow_type_ == UnmaskAuthFlowType::kOtpFallbackFromFido) {
    // It is possible to have the user hit the cancel button during an in-flight
    // Virtual Card Unmask request, so we need to reset the state of the
    // CreditCardOtpAuthenticator as well to ensure the flow does not continue,
    // as continuing the flow can cause a crash.
    client_->GetOtpAuthenticator()->Reset();
  }

  AutofillMetrics::VirtualCardUnmaskFlowType flow_type;
  switch (unmask_auth_flow_type_) {
    case UnmaskAuthFlowType::kOtp:
      flow_type = AutofillMetrics::VirtualCardUnmaskFlowType::kOtpOnly;
      break;
    case UnmaskAuthFlowType::kOtpFallbackFromFido:
      flow_type =
          AutofillMetrics::VirtualCardUnmaskFlowType::kOtpFallbackFromFido;
      break;
    case UnmaskAuthFlowType::kNone:
      flow_type = AutofillMetrics::VirtualCardUnmaskFlowType::kUnspecified;
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
  AutofillMetrics::LogServerCardUnmaskResult(
      AutofillMetrics::ServerCardUnmaskResult::kFlowCancelled,
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
  virtual_card_unmask_response_details_ =
      payments::PaymentsClient::UnmaskResponseDetails();
  ready_to_start_authentication_.Reset();
  can_fetch_unmask_details_ = true;
  card_ = nullptr;
  cvc_ = std::u16string();
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
      challenge_options, challenge_id, &CardUnmaskChallengeOption::id);
  return card_unmask_challenge_options_it != challenge_options.end()
             ? &(*card_unmask_challenge_options_it)
             : nullptr;
}

bool CreditCardAccessManager::ShouldLogServerCardUnmaskAttemptMetrics(
    CreditCard::RecordType record_type) {
  // We always want to log virtual card unmask attempts.
  if (record_type == CreditCard::VIRTUAL_CARD)
    return true;

  // We only want to log masked server card or full server card unmask
  // attempts if the `kAutofillEnableRemadeDownstreamMetrics` feature flag is
  // enabled, due to this being a histogram refactoring that we want to roll out
  // slowly to ensure that it works properly.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableRemadeDownstreamMetrics)) {
    return record_type == CreditCard::MASKED_SERVER_CARD ||
           record_type == CreditCard::FULL_SERVER_CARD;
  }

  // No conditions were met to log a server card unmasking attempt, so return
  // false.
  return false;
}

}  // namespace autofill
