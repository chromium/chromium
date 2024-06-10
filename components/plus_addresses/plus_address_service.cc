// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_http_client_impl.h"
#include "components/plus_addresses/plus_address_jit_allocator.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/webdata/common/web_data_results.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {

namespace {

using autofill::Suggestion;
using autofill::SuggestionType;
using PasswordFormType = autofill::AutofillClient::PasswordFormType;

// Get the ETLD+1 of `origin`, which means any subdomain is treated
// equivalently. See `GetDomainAndRegistry` for concrete examples.
std::string GetEtldPlusOne(const url::Origin origin) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Get and parse the excluded sites.
base::flat_set<std::string> GetAndParseExcludedSites() {
  return base::MakeFlatSet<std::string>(
      base::SplitString(features::kPlusAddressExcludedSites.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
}

PlusProfile::facet_t OriginToFacet(const url::Origin& origin) {
  PlusProfile::facet_t facet;
  if (IsSyncingPlusAddresses()) {
    // For a valid `origin`, `origin.GetURL().spec()` is always a valid spec.
    // However, using `FacetURI::FromCanonicalSpec(spec)` can lead to mismatches
    // in the underlying representation, since it uses the spec verbatim. E.g.,
    // a trailing "/" is removed by `FacetURI::FromPotentiallyInvalidSpec()`,
    // but kept by `FacetURI::FromCanonicalSpec(spec)`.
    // TODO(b/338342346): Revise `FacetURI::FromCanonicalSpec()`.
    facet = affiliations::FacetURI::FromPotentiallyInvalidSpec(
        origin.GetURL().spec());
  } else {
    facet = GetEtldPlusOne(origin);
  }
  return facet;
}

bool ShouldOfferPlusAddressCreation(PasswordFormType form_type) {
  switch (form_type) {
    case PasswordFormType::kNoPasswordForm:
    case PasswordFormType::kSignupForm:
      return true;
    case PasswordFormType::kLoginForm:
    case PasswordFormType::kChangePasswordForm:
    case PasswordFormType::kResetPasswordForm:
    case PasswordFormType::kSingleUsernameForm:
      return false;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

PlusAddressService::PlusAddressService(
    signin::IdentityManager* identity_manager,
    std::unique_ptr<PlusAddressHttpClient> plus_address_http_client,
    scoped_refptr<PlusAddressWebDataService> webdata_service,
    affiliations::AffiliationService* affiliation_service)
    : identity_manager_(CHECK_DEREF(identity_manager)),
      submission_logger_(identity_manager,
                         base::BindRepeating(&PlusAddressService::IsPlusAddress,
                                             base::Unretained(this))),
      plus_address_http_client_(std::move(plus_address_http_client)),
      webdata_service_(std::move(webdata_service)),
      plus_address_allocator_(std::make_unique<PlusAddressJitAllocator>(
          plus_address_http_client_.get())),
      plus_address_match_helper_(this, affiliation_service),
      excluded_sites_(GetAndParseExcludedSites()) {
  if (IsSyncingPlusAddresses()) {
    if (webdata_service_) {
      webdata_service_observation_.Observe(webdata_service_.get());
      if (is_enabled()) {
        webdata_service_->GetPlusProfiles(this);
      }
    }
  } else {
    CreateAndStartTimer();
    // Observing the identity manager is only necessary to clear data on
    // sign-out and start polling plus addresses for newly signed in accounts.
    // When plus addresses arrive via sync, this becomes unnecessary.
    identity_manager_observation_.Observe(identity_manager);
  }
}

PlusAddressService::~PlusAddressService() {
  for (Observer& o : observers_) {
    o.OnPlusAddressServiceShutdown();
  }
}

bool PlusAddressService::SupportsPlusAddresses(const url::Origin& origin,
                                               bool is_off_the_record) const {
  // First, check prerequisites (the feature enabled, etc.).
  if (!is_enabled()) {
    return false;
  }

  // Check if origin is supported (Not opaque, in the `excluded_sites_`, or is
  // non http/https scheme).
  if (!IsSupportedOrigin(origin)) {
    return false;
  }
  // We've met the prerequisites. If this isn't an OTR session, plus_addresses
  // are supported.
  if (!is_off_the_record) {
    return true;
  }
  // Prerequisites are met, but it's an off-the-record session. If there's an
  // existing plus_address with a facet equal to `origin` (i.e. no affiliations
  // considered), it's supported, otherwise it is not.
  return GetPlusProfile(OriginToFacet(origin)).has_value();
}

std::optional<std::string> PlusAddressService::GetPlusAddress(
    const PlusProfile::facet_t& facet) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<PlusProfile> profile = GetPlusProfile(facet);
  return profile ? std::make_optional(profile->plus_address) : std::nullopt;
}

std::vector<PlusProfile> PlusAddressService::GetPlusProfiles() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::vector<PlusProfile>(plus_profiles_.begin(), plus_profiles_.end());
}

std::optional<PlusProfile> PlusAddressService::GetPlusProfile(
    const PlusProfile::facet_t& facet) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* facet_uri = absl::get_if<affiliations::FacetURI>(&facet)) {
    if (!facet_uri->is_valid()) {
      return std::nullopt;
    }
  }
  // `facet` is used as the comparator, so the other fields don't matter.
  auto it = plus_profiles_.find(PlusProfile("", facet, "", false));
  if (it == plus_profiles_.end()) {
    return std::nullopt;
  }
  return *it;
}

void PlusAddressService::SavePlusProfile(const PlusProfile& profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile.is_confirmed);
  // New plus addresses are requested directly from the PlusAddress backend. If
  // `IsSyncingPlusAddresses()`, these addresses become later available through
  // sync. Until the address shows up in sync, it should still be available
  // through `PlusAddressService`, even after reloading the data. This requires
  // adding the address to the database.
  if (webdata_service_ && IsSyncingPlusAddresses()) {
    webdata_service_->AddOrUpdatePlusProfile(profile);
  }
  // Update the in-memory `plus_profiles_` cache.
  plus_profiles_.insert(profile);
  plus_addresses_.insert(profile.plus_address);
  for (Observer& o : observers_) {
    o.OnPlusAddressesChanged(
        {PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile)});
  }
}

bool PlusAddressService::IsPlusAddress(
    const std::string& potential_plus_address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_addresses_.contains(potential_plus_address);
}

void PlusAddressService::GetSuggestions(
    const url::Origin& last_committed_primary_main_frame_origin,
    bool is_off_the_record,
    PasswordFormType focused_form_type,
    std::u16string_view focused_field_value,
    autofill::AutofillSuggestionTriggerSource trigger_source,
    GetSuggestionsCallback callback) {
  if (!SupportsPlusAddresses(last_committed_primary_main_frame_origin,
                             is_off_the_record)) {
    std::move(callback).Run({});
    return;
  }

  plus_address_match_helper_.GetAffiliatedPlusProfiles(
      OriginToFacet(last_committed_primary_main_frame_origin),
      base::BindOnce(&PlusAddressService::OnGetAffiliatedPlusProfiles,
                     weak_factory_.GetWeakPtr(), focused_form_type,
                     std::u16string(focused_field_value), trigger_source,
                     std::move(callback)));
}

std::optional<Suggestion> PlusAddressService::GetManagePlusAddressSuggestion()
    const {
  if (!base::FeatureList::IsEnabled(features::kPlusAddressUIRedesign)) {
    return std::nullopt;
  }
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_TEXT),
      SuggestionType::kManagePlusAddress);
  suggestion.icon = Suggestion::Icon::kGoogleMonochrome;
  return suggestion;
}

void PlusAddressService::OnGetAffiliatedPlusProfiles(
    PasswordFormType focused_form_type,
    std::u16string_view focused_field_value,
    autofill::AutofillSuggestionTriggerSource trigger_source,
    GetSuggestionsCallback callback,
    std::vector<PlusProfile> affiliated_profiles) {
  using enum autofill::AutofillSuggestionTriggerSource;
  const std::u16string normalized_field_value =
      autofill::RemoveDiacriticsAndConvertToLowerCase(focused_field_value);

  if (affiliated_profiles.empty()) {
    // Do not offer creation on non-empty fields and certain form types (e.g.
    // login forms).
    if (trigger_source != kManualFallbackPlusAddresses &&
        (!normalized_field_value.empty() ||
         !ShouldOfferPlusAddressCreation(focused_form_type))) {
      std::move(callback).Run({});
      return;
    }
    Suggestion create_plus_address_suggestion(
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
        SuggestionType::kCreateNewPlusAddress);
    RecordAutofillSuggestionEvent(AutofillPlusAddressDelegate::SuggestionEvent::
                                      kCreateNewPlusAddressSuggested);
    if constexpr (!BUILDFLAG(IS_ANDROID)) {
      if (base::FeatureList::IsEnabled(features::kPlusAddressUIRedesign)) {
        create_plus_address_suggestion.labels = {
            {Suggestion::Text(l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
      }
    }
    create_plus_address_suggestion.icon =
        base::FeatureList::IsEnabled(features::kPlusAddressUIRedesign)
            ? Suggestion::Icon::kPlusAddressSmall
            : Suggestion::Icon::kPlusAddress;
    std::move(callback).Run({std::move(create_plus_address_suggestion)});
    return;
  }

  std::vector<Suggestion> suggestions;
  suggestions.reserve(affiliated_profiles.size());
  for (const PlusProfile& profile : affiliated_profiles) {
    Suggestion suggestion =
        Suggestion(base::UTF8ToUTF16(profile.plus_address),
                   SuggestionType::kFillExistingPlusAddress);
    if constexpr (!BUILDFLAG(IS_ANDROID)) {
      if (base::FeatureList::IsEnabled(features::kPlusAddressUIRedesign)) {
        suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
      }
    }
    suggestion.icon =
        base::FeatureList::IsEnabled(features::kPlusAddressUIRedesign)
            ? Suggestion::Icon::kPlusAddressSmall
            : Suggestion::Icon::kPlusAddress;

    // Only suggest filling a plus address whose prefix matches the field's
    // value.
    if (trigger_source == kManualFallbackPlusAddresses ||
        suggestion.main_text.value.starts_with(normalized_field_value)) {
      suggestions.push_back(std::move(suggestion));
    }
  }

  if (!suggestions.empty()) {
    RecordAutofillSuggestionEvent(AutofillPlusAddressDelegate::SuggestionEvent::
                                      kExistingPlusAddressSuggested);
  }

  std::move(callback).Run({std::move(suggestions)});
}

void PlusAddressService::ReservePlusAddress(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed) {
  if (!is_enabled()) {
    return;
  }
  plus_address_allocator_->AllocatePlusAddress(
      origin, PlusAddressAllocator::AllocationMode::kAny,
      base::BindOnce(&PlusAddressService::HandleCreateOrConfirmResponse,
                     base::Unretained(this), origin, std::move(on_completed)));
}

void PlusAddressService::RefreshPlusAddress(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed) {
  if (!is_enabled()) {
    return;
  }
  plus_address_allocator_->AllocatePlusAddress(
      origin, PlusAddressAllocator::AllocationMode::kNewPlusAddress,
      base::BindOnce(&PlusAddressService::HandleCreateOrConfirmResponse,
                     base::Unretained(this), origin, std::move(on_completed)));
}

bool PlusAddressService::IsRefreshingSupported(const url::Origin& origin) {
  return plus_address_allocator_->IsRefreshingSupported(origin);
}

void PlusAddressService::ConfirmPlusAddress(
    const url::Origin& origin,
    const std::string& plus_address,
    PlusAddressRequestCallback on_completed) {
  if (!is_enabled()) {
    return;
  }
  // Check the local mapping before attempting to confirm plus_address.
  if (std::optional<PlusProfile> stored_plus_profile =
          GetPlusProfile(OriginToFacet(origin));
      stored_plus_profile) {
    std::move(on_completed).Run(stored_plus_profile.value());
    return;
  }
  plus_address_http_client_->ConfirmPlusAddress(
      origin, plus_address,
      base::BindOnce(&PlusAddressService::HandleCreateOrConfirmResponse,
                     base::Unretained(this), origin, std::move(on_completed)));
}

void PlusAddressService::HandleCreateOrConfirmResponse(
    const url::Origin& origin,
    PlusAddressRequestCallback callback,
    const PlusProfileOrError& maybe_profile) {
  if (maybe_profile.has_value()) {
    account_is_forbidden_ = false;
    if (maybe_profile->is_confirmed) {
      if (IsSyncingPlusAddresses()) {
        SavePlusProfile(*maybe_profile);
      } else {
        PlusProfile profile_to_save = *maybe_profile;
        profile_to_save.facet = GetEtldPlusOne(origin);
        SavePlusProfile(profile_to_save);
      }
    }
  } else {
    HandlePlusAddressRequestError(maybe_profile.error());
  }
  // Run callback last in case it's dependent on above changes.
  std::move(callback).Run(maybe_profile);
}

std::optional<std::string> PlusAddressService::GetPrimaryEmail() {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return std::nullopt;
  }
  // TODO(crbug.com/40276862): This is fine for prototyping, but eventually we
  // must also take `AccountInfo::CanHaveEmailAddressDisplayed` into account
  // here and elsewhere in this file.
  return identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

bool PlusAddressService::is_enabled() const {
  if (features::kDisableForForbiddenUsers.Get() &&
      account_is_forbidden_.has_value() && account_is_forbidden_.value()) {
    return false;
  }
  return base::FeatureList::IsEnabled(features::kPlusAddressesEnabled) &&
         (features::kEnterprisePlusAddressServerUrl.Get() != "") &&
         // Note that having a primary account implies that account's email will
         // be populated.
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
         primary_account_auth_error_.state() ==
             GoogleServiceAuthError::State::NONE;
}

void PlusAddressService::CreateAndStartTimer() {
  if (!is_enabled() || !features::kSyncWithEnterprisePlusAddressServer.Get() ||
      polling_timer_.IsRunning()) {
    return;
  }
  SyncPlusAddressMapping();
  polling_timer_.Start(
      FROM_HERE, features::kEnterprisePlusAddressTimerDelay.Get(),
      base::BindRepeating(&PlusAddressService::SyncPlusAddressMapping,
                          // base::Unretained(this) is safe here since the timer
                          // that is created has same lifetime as this service.
                          base::Unretained(this)));
}

void PlusAddressService::SyncPlusAddressMapping() {
  if (!is_enabled()) {
    return;
  }
  plus_address_http_client_->GetAllPlusAddresses(base::BindOnce(
      [](PlusAddressService* service,
         const PlusAddressMapOrError& maybe_mapping) {
        if (maybe_mapping.has_value()) {
          if (service->is_enabled()) {
            service->UpdatePlusAddressMap(maybe_mapping.value());
          }
          service->account_is_forbidden_ = false;
        } else {
          service->HandlePlusAddressRequestError(maybe_mapping.error());
          // If `kDisableForForbiddenUsers` is on, we retry 403 responses.
          if (features::kDisableForForbiddenUsers.Get() &&
              maybe_mapping.error() == PlusAddressRequestError::AsNetworkError(
                                           net::HTTP_FORBIDDEN) &&
              !service->account_is_forbidden_.value_or(false)) {
            service->SyncPlusAddressMapping();
          }
        }
      },
      // base::Unretained is safe here since PlusAddressService owns
      // the PlusAddressHttpClient and they have the same lifetime.
      base::Unretained(this)));
}

void PlusAddressService::UpdatePlusAddressMap(const PlusAddressMap& map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  plus_profiles_.clear();
  plus_addresses_.clear();
  for (const auto& [facet, address] : map) {
    // `UpdatePlusAddressMap()` is only called when sync support is disabled.
    // In this case, profile_ids don't matter.
    plus_profiles_.insert(
        PlusProfile(/*profile_id=*/"", facet, address, /*is_confirmed=*/true));
    plus_addresses_.insert(address);
  }
}

void PlusAddressService::OnWebDataChangedBySync(
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
        const auto [it, inserted] = plus_profiles_.insert(profile);
        if (inserted) {
          plus_addresses_.insert(profile.plus_address);
          applied_changes.emplace_back(PlusAddressDataChange::Type::kAdd,
                                       profile);
        }
        break;
      }
      case PlusAddressDataChange::Type::kRemove: {
        if (plus_profiles_.erase(profile)) {
          plus_addresses_.erase(profile.plus_address);
          applied_changes.emplace_back(PlusAddressDataChange::Type::kRemove,
                                       profile);
        }
        break;
      }
    }
  }

  for (Observer& o : observers_) {
    o.OnPlusAddressesChanged(applied_changes);
  }
}

void PlusAddressService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(result->GetType(), PLUS_ADDRESS_RESULT);
  CHECK(plus_profiles_.empty());
  CHECK(plus_addresses_.empty());

  const std::vector<PlusProfile>& profiles =
      static_cast<WDResult<std::vector<PlusProfile>>*>(result.get())
          ->GetValue();

  std::vector<PlusAddressDataChange> applied_changes;
  applied_changes.reserve(profiles.size());
  for (const PlusProfile& plus_profile : profiles) {
    plus_profiles_.insert(plus_profile);
    plus_addresses_.insert(plus_profile.plus_address);
    applied_changes.emplace_back(PlusAddressDataChange::Type::kAdd,
                                 plus_profile);
  }

  for (Observer& o : observers_) {
    o.OnPlusAddressesChanged(applied_changes);
  }
}

void PlusAddressService::HandlePlusAddressRequestError(
    const PlusAddressRequestError& error) {
  if (!features::kDisableForForbiddenUsers.Get() ||
      error.type() != PlusAddressRequestErrorType::kNetworkError) {
    return;
  }
  if (account_is_forbidden_ || !error.http_response_code() ||
      *error.http_response_code() != net::HTTP_FORBIDDEN) {
    return;
  }
  if (++http_forbidden_responses_ > kMaxHttpForbiddenResponses) {
    account_is_forbidden_ = true;
  }
}
void PlusAddressService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  signin::PrimaryAccountChangeEvent::Type type =
      event.GetEventTypeFor(signin::ConsentLevel::kSignin);
  if (type == signin::PrimaryAccountChangeEvent::Type::kCleared) {
    HandleSignout();
  } else if (type == signin::PrimaryAccountChangeEvent::Type::kSet) {
    CreateAndStartTimer();
  }
}

void PlusAddressService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (auto primary_account = identity_manager_->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
      primary_account.IsEmpty() ||
      primary_account.account_id != account_info.account_id) {
    return;
  }
  primary_account_auth_error_ = error;
  if (error.state() != GoogleServiceAuthError::NONE) {
    HandleSignout();
  } else {
    CreateAndStartTimer();
  }
}

void PlusAddressService::HandleSignout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  plus_profiles_.clear();
  plus_addresses_.clear();
  polling_timer_.Stop();
  plus_address_http_client_->Reset();
}


bool PlusAddressService::IsSupportedOrigin(const url::Origin& origin) const {
  if (origin.opaque() || excluded_sites_.contains(GetEtldPlusOne(origin))) {
    return false;
  }

  return origin.scheme() == url::kHttpsScheme ||
         origin.scheme() == url::kHttpScheme;
}

void PlusAddressService::RecordAutofillSuggestionEvent(
    SuggestionEvent suggestion_event) {
  metrics::RecordAutofillSuggestionEvent(suggestion_event);
}

void PlusAddressService::OnPlusAddressSuggestionShown(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field,
    SuggestionContext suggestion_context,
    autofill::AutofillClient::PasswordFormType form_type,
    autofill::SuggestionType suggestion_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  submission_logger_.OnPlusAddressSuggestionShown(
      manager, form, field, suggestion_context, form_type, suggestion_type,
      /*plus_address_count=*/plus_addresses_.size());
}

}  // namespace plus_addresses
