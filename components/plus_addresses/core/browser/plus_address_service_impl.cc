// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/plus_addresses/core/browser/plus_address_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/plus_address_survey_type.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "components/plus_addresses/core/browser/metrics/plus_address_metrics.h"
#include "components/plus_addresses/core/browser/plus_address_allocator.h"
#include "components/plus_addresses/core/browser/plus_address_blocklist_data.h"
#include "components/plus_addresses/core/browser/plus_address_hats_utils.h"
#include "components/plus_addresses/core/browser/plus_address_http_client.h"
#include "components/plus_addresses/core/browser/plus_address_http_client_impl.h"
#include "components/plus_addresses/core/browser/plus_address_jit_allocator.h"
#include "components/plus_addresses/core/browser/plus_address_preallocator.h"
#include "components/plus_addresses/core/browser/plus_address_types.h"
#include "components/plus_addresses/core/browser/plus_address_ui_utils.h"
#include "components/plus_addresses/core/browser/settings/plus_address_setting_service.h"
#include "components/plus_addresses/core/browser/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/core/browser/webdata/plus_address_webdata_service.h"
#include "components/plus_addresses/core/common/features.h"
#include "components/plus_addresses/core/common/plus_address_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/webdata/common/web_data_results.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace plus_addresses {

namespace {

using autofill::AutofillSuggestionTriggerSource;
using autofill::FormFieldData;
using autofill::Suggestion;
using autofill::SuggestionType;
using PasswordFormClassification = autofill::PasswordFormClassification;

constexpr char16_t kPlusAddressDomain[] = u"@grelay.com";

affiliations::FacetURI OriginToFacet(const url::Origin& origin) {
  // For a valid `origin`, `origin.GetURL().spec()` is always a valid spec.
  // However, using `FacetURI::FromCanonicalSpec(spec)` can lead to mismatches
  // in the underlying representation, since it uses the spec verbatim. E.g.,
  // a trailing "/" is removed by `FacetURI::FromPotentiallyInvalidSpec()`,
  // but kept by `FacetURI::FromCanonicalSpec(spec)`.
  // TODO(b/338342346): Revise `FacetURI::FromCanonicalSpec()`.
  return affiliations::FacetURI::FromPotentiallyInvalidSpec(
      origin.GetURL().spec());
}

std::unique_ptr<PlusAddressAllocator> CreateAllocator(
    PrefService* pref_service,
    PlusAddressSettingService* setting_service,
    PlusAddressHttpClient* http_client,
    PlusAddressPreallocator::IsEnabledCheck is_enabled_check) {
  if (base::FeatureList::IsEnabled(features::kPlusAddressPreallocation)) {
    return std::make_unique<PlusAddressPreallocator>(
        pref_service, setting_service, http_client,
        std::move(is_enabled_check));
  }
  return std::make_unique<PlusAddressJitAllocator>(http_client);
}

// Returns `true` if the origin is part of the set of blocklisted domains and
// `false` otherwise. This means that the domain's origin matches the
// `exclusion_pattern` regex and does not match the `exception_pattern` regex.
bool IsSiteExcluded(const url::Origin& origin) {
  const PlusAddressBlocklistData& blocklist_data =
      PlusAddressBlocklistData::GetInstance();

  const re2::RE2* exception_pattern = blocklist_data.GetExceptionPattern();
  if (exception_pattern &&
      RE2::PartialMatch(origin.host(), *exception_pattern)) {
    return false;
  }

  const re2::RE2* exclusion_pattern = blocklist_data.GetExclusionPattern();
  return exclusion_pattern &&
         RE2::PartialMatch(origin.host(), *exclusion_pattern);
}

std::string GetPlusAddressFromPlusProfile(
    const PlusProfile& affiliated_profile) {
  return affiliated_profile.plus_address.value();
}

// Returns a suggestion to fill an existing plus address.
Suggestion CreateFillPlusAddressSuggestion(std::u16string plus_address) {
  Suggestion suggestion = Suggestion(std::move(plus_address),
                                     SuggestionType::kFillExistingPlusAddress);
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
  }
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  return suggestion;
}

std::vector<autofill::Suggestion> GetSuggestions(
    const std::vector<std::string>& affiliated_plus_addresses) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(affiliated_plus_addresses.size());
  for (const std::string& affiliated_plus_address : affiliated_plus_addresses) {
    suggestions.push_back(CreateFillPlusAddressSuggestion(
        base::UTF8ToUTF16(affiliated_plus_address)));
  }
  // It is required by `autofill::SuggestionGenerator` that this function should
  // not filter plus addresses and should return an `autofill::Suggestion`
  // object for each of them.
  CHECK_EQ(suggestions.size(), affiliated_plus_addresses.size());
  return suggestions;
}

}  // namespace

PlusAddressServiceImpl::PlusAddressServiceImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    PlusAddressSettingService* setting_service,
    std::unique_ptr<PlusAddressHttpClient> plus_address_http_client,
    scoped_refptr<PlusAddressWebDataService> webdata_service,
    affiliations::AffiliationService* affiliation_service,
    FeatureEnabledForProfileCheck feature_enabled_for_profile_check)
    : pref_service_(CHECK_DEREF(pref_service)),
      identity_manager_(CHECK_DEREF(identity_manager)),
      setting_service_(CHECK_DEREF(setting_service)),
      submission_logger_(
          identity_manager,
          base::BindRepeating(&PlusAddressServiceImpl::IsPlusAddress,
                              base::Unretained(this))),
      plus_address_http_client_(std::move(plus_address_http_client)),
      webdata_service_(std::move(webdata_service)),
      plus_address_match_helper_(this, affiliation_service),
      feature_enabled_for_profile_check_(
          std::move(feature_enabled_for_profile_check)) {
  // The allocator is created in the body of the constructor to avoid that it
  // calls into `this` before all members are assigned.
  plus_address_allocator_ =
      CreateAllocator(&pref_service_.get(), &setting_service_.get(),
                      plus_address_http_client_.get(),
                      base::BindRepeating(&PlusAddressServiceImpl::IsEnabled,
                                          base::Unretained(this)));

  if (webdata_service_) {
    webdata_service_observation_.Observe(webdata_service_.get());
    if (IsEnabled()) {
      webdata_service_->GetPlusProfiles(
          base::BindOnce(&PlusAddressServiceImpl::OnWebDataServiceRequestDone,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
  identity_manager_observation_.Observe(identity_manager);
}

PlusAddressServiceImpl::~PlusAddressServiceImpl() {
  for (PlusAddressService::Observer& o : observers_) {
    o.OnPlusAddressServiceShutdown();
  }
}

void PlusAddressServiceImpl::AddObserver(PlusAddressService::Observer* o) {
  observers_.AddObserver(o);
}

void PlusAddressServiceImpl::RemoveObserver(PlusAddressService::Observer* o) {
  observers_.RemoveObserver(o);
}

bool PlusAddressServiceImpl::ShouldShowManualFallback(
    const url::Origin& origin,
    bool is_off_the_record) const {
  if (!IsPlusAddressFillingEnabled(origin)) {
    return false;
  }

  // If there's an existing plus_address with a facet equal to `origin` (i.e. no
  // affiliations considered), it's supported.
  if (GetPlusProfile(OriginToFacet(origin)).has_value()) {
    return true;
  }

  // Unless there's an existing plus address for `origin`, off-the-record
  // sessions are not supported.
  if (is_off_the_record) {
    return false;
  }

  // If the user doesn't have an existing plus address for `origin` and this
  // session is not off-the-record, the global toggle must be enabled.
  return setting_service_->GetIsPlusAddressesEnabled();
}

std::optional<PlusAddress> PlusAddressServiceImpl::GetPlusAddress(
    const affiliations::FacetURI& facet) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<PlusProfile> profile = GetPlusProfile(facet);
  return profile ? std::make_optional(std::move(profile->plus_address))
                 : std::nullopt;
}

void PlusAddressServiceImpl::GetAffiliatedPlusProfiles(
    const url::Origin& origin,
    GetPlusProfilesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  plus_address_match_helper_.GetAffiliatedPlusProfiles(OriginToFacet(origin),
                                                       std::move(callback));
}

base::span<const PlusProfile> PlusAddressServiceImpl::GetPlusProfiles() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_address_cache_.GetPlusProfiles();
}

std::optional<PlusProfile> PlusAddressServiceImpl::GetPlusProfile(
    const affiliations::FacetURI& facet) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!facet.is_valid()) {
    return std::nullopt;
  }
  return plus_address_cache_.FindByFacet(facet);
}

void PlusAddressServiceImpl::SavePlusProfile(const PlusProfile& profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile.is_confirmed);
  // New plus addresses are requested directly from the PlusAddress backend.
  // These addresses become later available through sync. Until the address
  // shows up in sync, it should still be available through
  // `PlusAddressServiceImpl`, even after reloading the data. This requires
  // adding the address to the database.
  if (webdata_service_) {
    webdata_service_->AddOrUpdatePlusProfile(profile);
  }
  // Update the in-memory plus profiles cache.
  plus_address_cache_.InsertProfile(profile);
  for (PlusAddressService::Observer& o : observers_) {
    o.OnPlusAddressesChanged(
        {PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile)});
  }
}

bool PlusAddressServiceImpl::IsPlusAddress(
    const std::string& potential_plus_address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_address_cache_.IsPlusAddress(potential_plus_address);
}

bool PlusAddressServiceImpl::MatchesPlusAddressFormat(
    const std::u16string& value) const {
  return autofill::IsValidEmailAddress(value) &&
         value.ends_with(kPlusAddressDomain);
}

bool PlusAddressServiceImpl::IsPlusAddressFillingEnabled(
    const url::Origin& origin) const {
  // Check that the feature is enabled and the origin is supported (not opaque,
  // excluded, or is non http/https scheme)
  return IsEnabled() && IsSupportedOrigin(origin);
}

bool PlusAddressServiceImpl::IsFieldEligibleForPlusAddress(
    const autofill::AutofillField& field) const {
  auto filling_products = autofill::DenseSet<autofill::FillingProduct>(
      field.Type().GetGroups(), &autofill::GetFillingProductFromFieldTypeGroup);

  if (filling_products.contains(autofill::FillingProduct::kAddress)) {
    return true;
  }

  return (field.server_type() == autofill::FieldType::USERNAME ||
          field.server_type() == autofill::FieldType::SINGLE_USERNAME) &&
         field.heuristic_type() == autofill::FieldType::EMAIL_ADDRESS;
}

void PlusAddressServiceImpl::GetAffiliatedPlusAddresses(
    const url::Origin& origin,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  plus_address_match_helper_.GetAffiliatedPlusProfiles(
      OriginToFacet(origin),
      base::BindOnce(
          [](base::OnceCallback<void(std::vector<std::string>)> inner_callback,
             std::vector<PlusProfile> affiliated_profiles) {
            std::vector<std::string> plus_addresses = base::ToVector(
                affiliated_profiles, GetPlusAddressFromPlusProfile);
            std::move(inner_callback).Run(std::move(plus_addresses));
          },
          std::move(callback)));
}

std::vector<Suggestion> PlusAddressServiceImpl::GetSuggestionsFromPlusAddresses(
    const std::vector<std::string>& plus_addresses) {
  std::vector<Suggestion> suggestions = GetSuggestions(plus_addresses);
  const autofill::DenseSet<SuggestionType> suggestion_types(suggestions,
                                                            &Suggestion::type);

  using enum AutofillPlusAddressDelegate::SuggestionEvent;
  if (suggestion_types.contains(SuggestionType::kFillExistingPlusAddress)) {
    RecordAutofillSuggestionEvent(kExistingPlusAddressSuggested);
  }
  return suggestions;
}

Suggestion PlusAddressServiceImpl::GetManagePlusAddressSuggestion() const {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_TEXT),
      SuggestionType::kManagePlusAddress);
  suggestion.icon = Suggestion::Icon::kGoogleMonochrome;
  return suggestion;
}

void PlusAddressServiceImpl::ReservePlusAddress(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed) {
  if (!IsEnabled()) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kUserSignedOut)));
    return;
  }
  plus_address_allocator_->AllocatePlusAddress(
      origin, PlusAddressAllocator::AllocationMode::kAny,
      base::BindOnce(&PlusAddressServiceImpl::HandleCreateOrConfirmResponse,
                     base::Unretained(this))
          .Then(std::move(on_completed)));
}

void PlusAddressServiceImpl::RefreshPlusAddress(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed) {
  if (!IsEnabled()) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kUserSignedOut)));
    return;
  }
  plus_address_allocator_->AllocatePlusAddress(
      origin, PlusAddressAllocator::AllocationMode::kNewPlusAddress,
      base::BindOnce(&PlusAddressServiceImpl::HandleCreateOrConfirmResponse,
                     base::Unretained(this))
          .Then(std::move(on_completed)));
}

bool PlusAddressServiceImpl::IsRefreshingSupported(const url::Origin& origin) {
  return plus_address_allocator_->IsRefreshingSupported(origin);
}

void PlusAddressServiceImpl::ConfirmPlusAddress(
    const url::Origin& origin,
    const PlusAddress& plus_address,
    PlusAddressRequestCallback on_completed) {
  if (!IsEnabled()) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kUserSignedOut)));
    return;
  }
  // Check the local mapping before attempting to confirm plus_address.
  if (std::optional<PlusProfile> stored_plus_profile =
          GetPlusProfile(OriginToFacet(origin));
      stored_plus_profile) {
    std::move(on_completed).Run(stored_plus_profile.value());
    return;
  }

  // We remove the allocated plus address here even though the creation call
  // may not go through. UI code may offer the user to re-attempt to confirm
  // a plus address, e.g. in the case of time out.
  plus_address_allocator_->RemoveAllocatedPlusAddress(plus_address);
  plus_address_http_client_->ConfirmPlusAddress(
      origin, plus_address,
      base::BindOnce(&PlusAddressServiceImpl::HandleCreateOrConfirmResponse,
                     base::Unretained(this))
          .Then(std::move(on_completed)));
}

const PlusProfileOrError& PlusAddressServiceImpl::HandleCreateOrConfirmResponse(
    const PlusProfileOrError& maybe_profile) {
  if (maybe_profile.has_value() && maybe_profile->is_confirmed) {
    SavePlusProfile(*maybe_profile);
  }
  return maybe_profile;
}

std::optional<std::string> PlusAddressServiceImpl::GetPrimaryEmail() {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return std::nullopt;
  }
  // TODO(crbug.com/40276862): This is fine for prototyping, but eventually we
  // must also take `AccountInfo::CanHaveEmailAddressDisplayed` into account
  // here and elsewhere in this file.
  return identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

bool PlusAddressServiceImpl::IsEnabled() const {
  if (!feature_enabled_for_profile_check_.Run(
          features::kPlusAddressesEnabled) ||
      features::kEnterprisePlusAddressServerUrl.Get().empty()) {
    return false;
  }

  const auto primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  return !primary_account_id.empty() &&
         identity_manager_
                 ->GetErrorStateOfRefreshTokenForAccount(primary_account_id)
                 .state() == GoogleServiceAuthError::State::NONE;
}

void PlusAddressServiceImpl::OnWebDataChangedBySync(
    const std::vector<PlusAddressDataChange>& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<PlusAddressDataChange> applied_changes;
  for (const PlusAddressDataChange& change : changes) {
    const PlusProfile& profile = change.profile();
    switch (change.type()) {
      // Sync updates affect the local cache only if they introduce changes
      // (e.g., an added plus address that wasn't previously confirmed via
      // ConfirmPlusAddress).
      case PlusAddressDataChange::Type::kAdd: {
        if (plus_address_cache_.InsertProfile(profile)) {
          applied_changes.emplace_back(PlusAddressDataChange::Type::kAdd,
                                       profile);
        }
        break;
      }
      case PlusAddressDataChange::Type::kRemove: {
        if (plus_address_cache_.EraseProfile(profile)) {
          applied_changes.emplace_back(PlusAddressDataChange::Type::kRemove,
                                       profile);
        }
        break;
      }
    }
  }

  for (PlusAddressService::Observer& o : observers_) {
    o.OnPlusAddressesChanged(applied_changes);
  }
}

void PlusAddressServiceImpl::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(result->GetType(), PLUS_ADDRESS_RESULT);
  CHECK(plus_address_cache_.IsEmpty());

  const std::vector<PlusProfile>& profiles =
      static_cast<WDResult<std::vector<PlusProfile>>*>(result.get())
          ->GetValue();

  std::vector<PlusAddressDataChange> applied_changes;
  applied_changes.reserve(profiles.size());
  for (const PlusProfile& plus_profile : profiles) {
    plus_address_cache_.InsertProfile(plus_profile);
    applied_changes.emplace_back(PlusAddressDataChange::Type::kAdd,
                                 plus_profile);
  }

  for (PlusAddressService::Observer& o : observers_) {
    o.OnPlusAddressesChanged(applied_changes);
  }
}

void PlusAddressServiceImpl::Shutdown() {
  identity_manager_observation_.Reset();
  PlusAddressService::Shutdown();
}

void PlusAddressServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  signin::PrimaryAccountChangeEvent::Type type =
      event.GetEventTypeFor(signin::ConsentLevel::kSignin);
  if (type == signin::PrimaryAccountChangeEvent::Type::kCleared) {
    HandleSignout();
  }
}

void PlusAddressServiceImpl::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (auto primary_account = identity_manager_->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
      primary_account.IsEmpty() ||
      primary_account.account_id != account_info.account_id) {
    return;
  }
  if (error.state() != GoogleServiceAuthError::NONE) {
    HandleSignout();
  }
}

void PlusAddressServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  // Needs to be shutdown before IdentityManager.
  NOTREACHED(base::NotFatalUntil::M142);
}

void PlusAddressServiceImpl::HandleSignout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  plus_address_http_client_->Reset();
}

bool PlusAddressServiceImpl::IsSupportedOrigin(
    const url::Origin& origin) const {
  if (origin.opaque() || IsSiteExcluded(origin)) {
    return false;
  }

  return origin.scheme() == url::kHttpsScheme ||
         origin.scheme() == url::kHttpScheme;
}

void PlusAddressServiceImpl::RecordAutofillSuggestionEvent(
    SuggestionEvent suggestion_event) {
  metrics::RecordAutofillSuggestionEvent(suggestion_event);

  using enum autofill::AutofillPlusAddressDelegate::SuggestionEvent;
  switch (suggestion_event) {
    case kRefreshPlusAddressInlineClicked:
      base::RecordAction(base::UserMetricsAction("PlusAddresses.Refreshed"));
      return;
    case kExistingPlusAddressSuggested:
      base::RecordAction(base::UserMetricsAction(
          "PlusAddresses.StandaloneFillSuggestionShown"));
      return;
    case kCreateNewPlusAddressSuggested: {
      if (setting_service_->GetHasAcceptedNotice()) {
        base::RecordAction(
            base::UserMetricsAction("PlusAddresses.CreateSuggestionShown"));
      } else {
        base::RecordAction(base::UserMetricsAction(
            "PlusAddresses.CreateSuggestionFirstTimeNoticeShown"));
      }
      return;
    }
    case kCreateNewPlusAddressInlineSuggested:
      base::RecordAction(
          base::UserMetricsAction("PlusAddresses.CreateSuggestionShown"));
      return;
    case kExistingPlusAddressChosen:
      base::RecordAction(base::UserMetricsAction(
          "PlusAddresses.FillStandaloneSuggestionAccepted"));
      return;
    case kCreateNewPlusAddressChosen:
      base::RecordAction(
          base::UserMetricsAction("PlusAddresses.CreateSuggestionAccepted"));
      return;
    case kCreateNewPlusAddressInlineChosen:
      base::RecordAction(
          base::UserMetricsAction("PlusAddresses.OfferedPlusAddressAccepted"));
      return;
    case kErrorDuringReserve:
    case kCreateNewPlusAddressInlineReserveLoadingStateShown:
      return;
  }
  NOTREACHED();
}

void PlusAddressServiceImpl::OnPlusAddressSuggestionShown(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field,
    SuggestionContext suggestion_context,
    autofill::PasswordFormClassification::Type form_type,
    autofill::SuggestionType suggestion_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  submission_logger_.OnPlusAddressSuggestionShown(
      manager, form, field, suggestion_context, form_type, suggestion_type,
      /*plus_address_count=*/plus_address_cache_.Size());
}

void PlusAddressServiceImpl::DidFillPlusAddress() {
  pref_service_->SetTime(prefs::kLastPlusAddressFillingTime, base::Time::Now());
}

size_t PlusAddressServiceImpl::GetPlusAddressesCount() {
  return GetPlusProfiles().size();
}

std::map<std::string, std::string>
PlusAddressServiceImpl::GetPlusAddressHatsData() const {
  auto time_pref_to_string = [&](std::string_view pref) {
    const base::Time time = pref_service_->GetTime(pref);
    if (time.is_null()) {
      return std::string("-1");
    }
    const base::TimeDelta delta = base::Time::Now() - time;
    return delta.is_positive() ? base::ToString(delta.InSeconds())
                               : std::string("-1");
  };

  return {{hats::kPlusAddressesCount, base::ToString(GetPlusProfiles().size())},
          {hats::kFirstPlusAddressCreationTime,
           time_pref_to_string(prefs::kFirstPlusAddressCreationTime)},
          {hats::kLastPlusAddressFillingTime,
           time_pref_to_string(prefs::kLastPlusAddressFillingTime)}};
}

}  // namespace plus_addresses
