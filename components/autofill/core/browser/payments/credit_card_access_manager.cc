// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/webauthn_callback_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_IOS)
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
      form_event_logger_(form_event_logger),
      can_fetch_unmask_details_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                base::WaitableEvent::InitialState::SIGNALED) {
}

CreditCardAccessManager::~CreditCardAccessManager() {}

void CreditCardAccessManager::UpdateCreditCardFormEventLogger() {
  std::vector<CreditCard*> credit_cards = GetCreditCards();

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
  form_event_logger_->set_is_context_secure(client_->IsContextSecure());
}

std::vector<CreditCard*> CreditCardAccessManager::GetCreditCards() {
  return personal_data_manager_->GetCreditCards();
}

std::vector<CreditCard*> CreditCardAccessManager::GetCreditCardsToSuggest() {
  const std::vector<CreditCard*> cards_to_suggest =
      personal_data_manager_->GetCreditCardsToSuggest(
          client_->AreServerCardsSupported());

  return cards_to_suggest;
}

bool CreditCardAccessManager::ShouldDisplayGPayLogo() {
  for (const CreditCard* credit_card : GetCreditCardsToSuggest()) {
    if (IsLocalCard(credit_card))
      return false;
  }
  return true;
}

bool CreditCardAccessManager::UnmaskedCardCacheIsEmpty() {
  return unmasked_card_cache_.empty();
}

std::vector<const CachedServerCardInfo*>
CreditCardAccessManager::GetCachedUnmaskedCards() const {
  std::vector<const CachedServerCardInfo*> unmasked_cards;
  for (auto const& iter : unmasked_card_cache_) {
    unmasked_cards.push_back(&iter.second);
  }
  return unmasked_cards;
}

bool CreditCardAccessManager::IsCardPresentInUnmaskedCache(
    const std::string& server_id) const {
  return unmasked_card_cache_.find(server_id) != unmasked_card_cache_.end();
}

bool CreditCardAccessManager::ServerCardsAvailable() {
  for (const CreditCard* credit_card : GetCreditCardsToSuggest()) {
    if (!IsLocalCard(credit_card))
      return true;
  }
  return false;
}

bool CreditCardAccessManager::DeleteCard(const CreditCard* card) {
  // Server cards cannot be deleted from within Chrome.
  bool allowed_to_delete = IsLocalCard(card);

  if (allowed_to_delete)
    personal_data_manager_->DeleteLocalCreditCards({*card});

  return allowed_to_delete;
}

bool CreditCardAccessManager::GetDeletionConfirmationText(
    const CreditCard* card,
    std::u16string* title,
    std::u16string* body) {
  if (!IsLocalCard(card))
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

CreditCard* CreditCardAccessManager::GetCreditCard(std::string guid) {
  if (base::IsValidGUID(guid)) {
    return personal_data_manager_->GetCreditCardByGUID(guid);
  }
  return nullptr;
}

void CreditCardAccessManager::PrepareToFetchCreditCard() {
#if !defined(OS_IOS)
  // No need to fetch details if there are no server cards.
  if (!ServerCardsAvailable())
    return;

  // Do not make an unnecessary preflight call unless signaled.
  if (!can_fetch_unmask_details_.IsSignaled())
    return;

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
    payments_client_->GetUnmaskDetails(
        base::BindOnce(&CreditCardAccessManager::OnDidGetUnmaskDetails,
                       weak_ptr_factory_.GetWeakPtr()),
        personal_data_manager_->app_locale());
    preflight_call_timestamp_ = AutofillTickClock::NowTicks();
    AutofillMetrics::LogCardUnmaskPreflightCalled();
  }
}

void CreditCardAccessManager::OnDidGetUnmaskDetails(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::UnmaskDetails& unmask_details) {
  // Log latency for preflight call.
  AutofillMetrics::LogCardUnmaskPreflightDuration(
      AutofillTickClock::NowTicks() - preflight_call_timestamp_);

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

#if !defined(OS_IOS)
  opt_in_intention_ =
      GetOrCreateFIDOAuthenticator()->GetUserOptInIntention(unmask_details);
#endif
  ready_to_start_authentication_.Signal();

  // Use the weak_ptr here so that the delayed task won't be executed if the
  // |credit_card_access_manager| is reset.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CreditCardAccessManager::SignalCanFetchUnmaskDetails,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void CreditCardAccessManager::FetchCreditCard(
    const CreditCard* card,
    base::WeakPtr<Accessor> accessor,
    const base::TimeTicks& form_parsed_timestamp) {
  // Return error if authentication is already in progress or card is nullptr.
  if (is_authentication_in_progress_ || !card) {
    accessor->OnCreditCardFetched(/*did_succeed=*/false, nullptr);
    return;
  }

  // If card has been previously unmasked, use cached data.
  std::string identifier = card->record_type() == CreditCard::VIRTUAL_CARD
                               ? card->server_id() + kVirtualCardIdentifier
                               : card->server_id();
  std::unordered_map<std::string, CachedServerCardInfo>::iterator it =
      unmasked_card_cache_.find(identifier);
  if (it != unmasked_card_cache_.end()) {  // key is in cache
    accessor->OnCreditCardFetched(/*did_succeed=*/true,
                                  /*credit_card=*/&it->second.card,
                                  /*cvc=*/it->second.cvc);
    std::string metrics_name = card->record_type() == CreditCard::VIRTUAL_CARD
                                   ? "Autofill.UsedCachedVirtualCard"
                                   : "Autofill.UsedCachedServerCard";
    base::UmaHistogramCounts1000(metrics_name, ++it->second.cache_uses);
    return;
  }

  // Latency metrics should only be logged if the user is verifiable and the
  // flag is turned on. If flag is turned off, then |is_user_verifiable_| is not
  // set.
#if !defined(OS_IOS)
  bool should_log_latency_metrics = is_user_verifiable_.value_or(false);
#endif
  // Return immediately if local card and log that unmask details were ignored.
  if (card->record_type() != CreditCard::MASKED_SERVER_CARD) {
    accessor->OnCreditCardFetched(/*did_succeed=*/true, card);
#if !defined(OS_IOS)
    if (should_log_latency_metrics) {
      AutofillMetrics::LogUserPerceivedLatencyOnCardSelection(
          AutofillMetrics::PreflightCallEvent::kDidNotChooseMaskedCard,
          GetOrCreateFIDOAuthenticator()->IsUserOptedIn());
    }
#endif
    return;
  }

  card_ = std::make_unique<CreditCard>(*card);
  accessor_ = accessor;
  form_parsed_timestamp_ = form_parsed_timestamp;
  is_authentication_in_progress_ = true;

  bool get_unmask_details_returned =
      ready_to_start_authentication_.IsSignaled();
  bool user_is_opted_in = IsFidoAuthenticationEnabled();
  bool should_wait_to_authenticate =
      user_is_opted_in && !get_unmask_details_returned;

  // Logging metrics.
#if !defined(OS_IOS)
  if (should_log_latency_metrics) {
    AutofillMetrics::LogUserPerceivedLatencyOnCardSelection(
        get_unmask_details_returned
            ? AutofillMetrics::PreflightCallEvent::
                  kPreflightCallReturnedBeforeCardChosen
            : AutofillMetrics::PreflightCallEvent::
                  kCardChosenBeforePreflightCallReturned,
        GetOrCreateFIDOAuthenticator()->IsUserOptedIn());
  }
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // On desktop, show the verify pending dialog for opted-in user, unless it is
  // already known that selected card requires CVC.
  if (user_is_opted_in &&
      (!get_unmask_details_returned || IsSelectedCardFidoAuthorized())) {
    ShowVerifyPendingDialog();
  }
#endif

  if (should_wait_to_authenticate) {
    card_selected_without_unmask_details_timestamp_ =
        AutofillTickClock::NowTicks();

    // Wait for |ready_to_start_authentication_| to be signaled by
    // OnDidGetUnmaskDetails() or until timeout before calling Authenticate().
    ready_to_start_authentication_.OnEventOrTimeOut(
        base::BindOnce(&CreditCardAccessManager::Authenticate,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kUnmaskDetailsResponseTimeoutMs));
  } else {
    Authenticate(get_unmask_details_returned);
  }
}

void CreditCardAccessManager::FIDOAuthOptChange(bool opt_in) {
#if defined(OS_IOS)
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
#if defined(OS_IOS)
  return;
#else
  // TODO(crbug/949269): Add a rate limiter to counter spam clicking.
  FIDOAuthOptChange(opt_in);
#endif
}

void CreditCardAccessManager::SignalCanFetchUnmaskDetails() {
  can_fetch_unmask_details_.Signal();
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

UnmaskAuthFlowType CreditCardAccessManager::GetAuthenticationType(
    bool get_unmask_details_returned) {
  bool fido_auth_enabled =
      get_unmask_details_returned && unmask_details_.unmask_auth_method ==
                                         AutofillClient::UnmaskAuthMethod::FIDO;
#if !defined(OS_IOS)
  // Even if payments return FIDO enabled, we have to double check local pref,
  // because if user locally opted out, we need to fall back to CVC flow.
  fido_auth_enabled &= GetOrCreateFIDOAuthenticator()->IsUserOptedIn();
#endif

  bool card_is_authorized_for_fido =
      fido_auth_enabled && IsSelectedCardFidoAuthorized();

  // If FIDO authentication was suggested, but card is not in authorized list,
  // must authenticate with CVC followed by FIDO in order to authorize this card
  // for future FIDO use.
  bool should_follow_up_cvc_with_fido_auth =
      fido_auth_enabled && !card_is_authorized_for_fido;

  // Only use FIDO if card is authorized and not expired.
  bool card_is_eligible_for_fido =
      card_is_authorized_for_fido && !card_->IsExpired(AutofillClock::Now());

  if (card_is_eligible_for_fido)
    return UnmaskAuthFlowType::kFido;
  if (should_follow_up_cvc_with_fido_auth)
    return UnmaskAuthFlowType::kCvcThenFido;
  return UnmaskAuthFlowType::kCvc;
}

void CreditCardAccessManager::Authenticate(bool get_unmask_details_returned) {
  // Reset now that we have started authentication.
  ready_to_start_authentication_.Reset();
  unmask_details_request_in_progress_ = false;

  // If the user had to wait for Unmask Details, log the latency.
  if (card_selected_without_unmask_details_timestamp_.has_value()) {
    AutofillMetrics::LogUserPerceivedLatencyOnCardSelectionDuration(
        AutofillTickClock::NowTicks() -
        card_selected_without_unmask_details_timestamp_.value());
    AutofillMetrics::LogUserPerceivedLatencyOnCardSelectionTimedOut(
        /*did_time_out=*/!get_unmask_details_returned);
    card_selected_without_unmask_details_timestamp_ = absl::nullopt;
  }

  unmask_auth_flow_type_ = GetAuthenticationType(get_unmask_details_returned);
  form_event_logger_->LogCardUnmaskAuthenticationPromptShown(
      unmask_auth_flow_type_);

  // If FIDO auth was suggested, logging which authentication method was
  // actually used.
  if (unmask_auth_flow_type_ == UnmaskAuthFlowType::kFido) {
    AutofillMetrics::LogCardUnmaskTypeDecision(
        AutofillMetrics::CardUnmaskTypeDecisionMetric::kFidoOnly);
  }
  if (unmask_auth_flow_type_ == UnmaskAuthFlowType::kCvcThenFido) {
    AutofillMetrics::LogCardUnmaskTypeDecision(
        AutofillMetrics::CardUnmaskTypeDecisionMetric::kCvcThenFido);
  }

  if (unmask_auth_flow_type_ == UnmaskAuthFlowType::kFido) {
#if defined(OS_IOS)
    NOTREACHED();
#else
    // If |is_authentication_in_progress_| is false, it means the process has
    // been cancelled via the verify pending dialog. Do not run
    // CreditCardFIDOAuthenticator::Authenticate in this case (should not fall
    // back to CVC auth either).
    if (!is_authentication_in_progress_)
      return;

    DCHECK(unmask_details_.fido_request_options.has_value());
    GetOrCreateFIDOAuthenticator()->Authenticate(
        card_.get(), weak_ptr_factory_.GetWeakPtr(), form_parsed_timestamp_,
        std::move(unmask_details_.fido_request_options.value()));
#endif
  } else {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    // Close the Webauthn verify pending dialog if it enters CVC authentication
    // flow since the card unmask prompt will pop up.
    client_->CloseWebauthnDialog();
#endif
    GetOrCreateCVCAuthenticator()->Authenticate(
        card_.get(), weak_ptr_factory_.GetWeakPtr(), personal_data_manager_,
        form_parsed_timestamp_);
  }
}

CreditCardCVCAuthenticator*
CreditCardAccessManager::GetOrCreateCVCAuthenticator() {
  if (!cvc_authenticator_)
    cvc_authenticator_ = std::make_unique<CreditCardCVCAuthenticator>(client_);
  return cvc_authenticator_.get();
}

#if !defined(OS_IOS)
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
  can_fetch_unmask_details_.Signal();

  // Log completed CVC authentication if auth was successful. Do not log for
  // kCvcThenFido flow since that is yet to be completed.
  if (response.did_succeed &&
      unmask_auth_flow_type_ != UnmaskAuthFlowType::kCvcThenFido) {
    form_event_logger_->LogCardUnmaskAuthenticationPromptCompleted(
        unmask_auth_flow_type_);
  }

  // Store request options temporarily if given. They will be used for
  // AdditionallyPerformFidoAuth.
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

  // Local boolean denotes whether to fill the form immediately. If CVC
  // authentication failed, report error immediately. If GetRealPan did not
  // return card authorization token (we can't call any FIDO-related flows,
  // either opt-in or register new card, without token), fill the form
  // immediately.
  bool should_respond_immediately =
      !response.did_succeed || response.card_authorization_token.empty();
#if defined(OS_ANDROID)
  // GetRealPan did not return RequestOptions (user did not specify intent to
  // opt-in) AND flow is not registering a new card, also fill the form
  // directly.
  should_respond_immediately |=
      (!response.request_options.has_value() &&
       unmask_auth_flow_type_ != UnmaskAuthFlowType::kCvcThenFido);
#else
  // On desktop, if flow is not kCvcThenFido, it means it is not registering a
  // new card, we can fill the form immediately.
  should_respond_immediately |=
      unmask_auth_flow_type_ != UnmaskAuthFlowType::kCvcThenFido;
#endif

  // Local boolean denotes whether to call AdditionallyPerformFidoAuth which
  // delays the form filling and invokes an Authorization flow. If
  // |unmask_auth_flow_type_| is kCvcThenFido, then the user is already opted-in
  // and the new card must additionally be authorized through WebAuthn. Note
  // that this and |should_respond_immediately| are mutually exclusive (can not
  // both be true).
  bool should_authorize_with_fido =
      unmask_auth_flow_type_ == UnmaskAuthFlowType::kCvcThenFido;
#if defined(OS_ANDROID)
  // For Android, we will delay the form filling for both intent-to-opt-in user
  // opting in and opted-in user registering a new card (kCvcThenFido). So we
  // check one more scenario for Android here. If the GetRealPan response
  // includes |request_options|, that means the user showed intention to opt-in
  // while unmasking and must complete the challenge before successfully
  // opting-in and filling the form.
  should_authorize_with_fido |= response.request_options.has_value();
#endif
  // Card authorization token is required in order to call
  // AdditionallyPerformFidoAuth.
  should_authorize_with_fido &= !response.card_authorization_token.empty();

  // Local boolean denotes whether to show the dialog that offers opting-in to
  // FIDO authentication after the CVC check. Note that this and
  // |should_respond_immediately| are NOT mutually exclusive. If both are true,
  // it represents the Desktop opt-in flow (fill the form first, and prompt the
  // opt-in dialog).
  bool should_offer_fido_auth = false;
  // For iOS, FIDO auth is not supported yet. For Android, users have already
  // been offered opt-in at this point.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  should_offer_fido_auth = unmask_details_.offer_fido_opt_in &&
                           !response.card_authorization_token.empty() &&
                           !GetOrCreateFIDOAuthenticator()
                                ->GetOrCreateFidoAuthenticationStrikeDatabase()
                                ->IsMaxStrikesLimitReached();
#endif

  // Ensure that |should_respond_immediately| and |should_authorize_with_fido|
  // can't be true at the same time
  DCHECK(!(should_respond_immediately && should_authorize_with_fido));
  if (should_respond_immediately) {
    accessor_->OnCreditCardFetched(response.did_succeed, response.card,
                                   response.cvc);
    unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;
  } else if (should_authorize_with_fido) {
    AdditionallyPerformFidoAuth(response, request_options->Clone());
  }
  if (should_offer_fido_auth) {
    // CreditCardFIDOAuthenticator will handle enrollment completely.
    ShowWebauthnOfferDialog(response.card_authorization_token);
  }

#if !defined(OS_IOS)
  // If user intended to opt out, we will opt user out after cvc auth completes
  // (no matter cvc auth succeeded or failed).
  if (opt_in_intention_ == UserOptInIntention::kIntentToOptOut) {
    FIDOAuthOptChange(/*opt_in=*/false);
  }
  // Reset |opt_in_intention_| after cvc auth completes.
  opt_in_intention_ = UserOptInIntention::kUnspecified;
#endif
}

#if defined(OS_ANDROID)
bool CreditCardAccessManager::ShouldOfferFidoAuth() const {
  // If the user opted-in through the settings page, do not show checkbox.
  return unmask_details_.offer_fido_opt_in &&
         opt_in_intention_ != UserOptInIntention::kIntentToOptIn;
}

bool CreditCardAccessManager::UserOptedInToFidoFromSettingsPageOnMobile()
    const {
  return opt_in_intention_ == UserOptInIntention::kIntentToOptIn;
}
#endif

#if !defined(OS_IOS)
void CreditCardAccessManager::OnFIDOAuthenticationComplete(
    bool did_succeed,
    const CreditCard* card,
    const std::u16string& cvc) {
#if !defined(OS_ANDROID)
  // Close the Webauthn verify pending dialog. If FIDO authentication succeeded,
  // card is filled to the form, otherwise fall back to CVC authentication which
  // does not need the verify pending dialog either.
  client_->CloseWebauthnDialog();
#endif

  if (did_succeed) {
    is_authentication_in_progress_ = false;
    accessor_->OnCreditCardFetched(did_succeed, card, cvc);
    can_fetch_unmask_details_.Signal();

    form_event_logger_->LogCardUnmaskAuthenticationPromptCompleted(
        unmask_auth_flow_type_);
    unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;
  } else {
    unmask_auth_flow_type_ = UnmaskAuthFlowType::kCvcFallbackFromFido;
    form_event_logger_->LogCardUnmaskAuthenticationPromptShown(
        unmask_auth_flow_type_);
    GetOrCreateCVCAuthenticator()->Authenticate(
        card_.get(), weak_ptr_factory_.GetWeakPtr(), personal_data_manager_,
        form_parsed_timestamp_);
  }
}

void CreditCardAccessManager::OnFidoAuthorizationComplete(bool did_succeed) {
  if (did_succeed) {
    accessor_->OnCreditCardFetched(/*did_succeed=*/true, card_.get(), cvc_);
    form_event_logger_->LogCardUnmaskAuthenticationPromptCompleted(
        unmask_auth_flow_type_);
  }
  unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;
  cvc_ = std::u16string();
}
#endif

bool CreditCardAccessManager::IsLocalCard(const CreditCard* card) {
  return card && card->record_type() == CreditCard::LOCAL_CARD;
}

bool CreditCardAccessManager::IsFidoAuthenticationEnabled() {
#if defined(OS_IOS)
  return false;
#else
  return is_user_verifiable_.value_or(false) &&
         GetOrCreateFIDOAuthenticator()->IsUserOptedIn();
#endif
}

bool CreditCardAccessManager::IsSelectedCardFidoAuthorized() {
  DCHECK_NE(unmask_details_.unmask_auth_method,
            AutofillClient::UnmaskAuthMethod::UNKNOWN);
  return IsFidoAuthenticationEnabled() &&
         unmask_details_.fido_eligible_card_ids.find(card_->server_id()) !=
             unmask_details_.fido_eligible_card_ids.end();
}

void CreditCardAccessManager::ShowWebauthnOfferDialog(
    std::string card_authorization_token) {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  GetOrCreateFIDOAuthenticator()->OnWebauthnOfferDialogRequested(
      card_authorization_token);
  client_->ShowWebauthnOfferDialog(
      base::BindRepeating(&CreditCardAccessManager::HandleDialogUserResponse,
                          weak_ptr_factory_.GetWeakPtr()));
#endif
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
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
      OnFIDOAuthenticationComplete(/*did_succeed=*/false);
      break;
  }
}
#endif

void CreditCardAccessManager::AdditionallyPerformFidoAuth(
    const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response,
    base::Value request_options) {
#if !defined(OS_IOS)
  // Save credit card for after authorization.
  card_ = std::make_unique<CreditCard>(*(response.card));
  cvc_ = response.cvc;
  GetOrCreateFIDOAuthenticator()->Authorize(weak_ptr_factory_.GetWeakPtr(),
                                            response.card_authorization_token,
                                            request_options.Clone());
#endif
}

}  // namespace autofill
