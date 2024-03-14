// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/rand_util.h"
#include "base/scoped_observation.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_http_client_impl.h"
#include "components/plus_addresses/plus_address_jit_allocator.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
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

using autofill::PopupItemId;
using autofill::Suggestion;

// Get the ETLD+1 of `origin`, which means any subdomain is treated
// equivalently. See `GetDomainAndRegistry` for concrete examples.
std::string GetEtldPlusOne(const url::Origin origin) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

PlusAddressService::PlusAddressService(
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    std::unique_ptr<PlusAddressHttpClient> plus_address_http_client,
    scoped_refptr<PlusAddressWebDataService> webdata_service)
    : identity_manager_(identity_manager),
      pref_service_(pref_service),
      plus_address_http_client_(std::move(plus_address_http_client)),
      webdata_service_(webdata_service),
      plus_address_allocator_(std::make_unique<PlusAddressJitAllocator>(
          plus_address_http_client_.get())),
      excluded_sites_(GetAndParseExcludedSites()) {
  CreateAndStartTimer();
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
  if (webdata_service_ && is_enabled()) {
    webdata_service_->GetPlusProfiles(this);
  }
}

PlusAddressService::~PlusAddressService() = default;

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
  // existing plus_address, it's supported, otherwise it is not.
  return GetPlusProfile(origin).has_value();
}

std::optional<std::string> PlusAddressService::GetPlusAddress(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<PlusProfile> profile = GetPlusProfile(origin);
  return profile ? std::make_optional(profile->plus_address) : std::nullopt;
}

std::vector<PlusProfile> PlusAddressService::GetPlusProfiles() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<PlusProfile> profiles;
  profiles.reserve(plus_address_by_site_.size());
  for (const auto& [facet, plus_address] : plus_address_by_site_) {
    profiles.push_back(PlusProfile(
        {.facet = facet, .plus_address = plus_address, .is_confirmed = true}));
  }
  return profiles;
}

std::optional<PlusProfile> PlusAddressService::GetPlusProfile(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string etld_plus_one = GetEtldPlusOne(origin);
  auto it = plus_address_by_site_.find(etld_plus_one);
  if (it == plus_address_by_site_.end()) {
    return std::nullopt;
  }
  // Assume that 'is_confirmed` = TRUE since this service only has a saved plus
  // address if it was successfully confirmed via the dialog or retrieved via
  // polling (which only returns confirmed plus addresses).
  return PlusProfile({.facet = etld_plus_one,
                      .plus_address = it->second,
                      .is_confirmed = true});
}

void PlusAddressService::SavePlusAddress(url::Origin origin,
                                         std::string plus_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string etld_plus_one = GetEtldPlusOne(origin);
  plus_address_by_site_[etld_plus_one] = plus_address;
  plus_addresses_.insert(plus_address);
}

bool PlusAddressService::IsPlusAddress(
    const std::string& potential_plus_address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_addresses_.contains(potential_plus_address);
}

std::vector<Suggestion> PlusAddressService::GetSuggestions(
    const url::Origin& last_committed_primary_main_frame_origin,
    bool is_off_the_record,
    std::u16string_view focused_field_value,
    autofill::AutofillSuggestionTriggerSource trigger_source) {
  using enum autofill::AutofillSuggestionTriggerSource;
  if (!SupportsPlusAddresses(last_committed_primary_main_frame_origin,
                             is_off_the_record)) {
    return {};
  }

  const std::u16string normalized_field_value =
      autofill::RemoveDiacriticsAndConvertToLowerCase(focused_field_value);
  std::optional<std::string> maybe_address =
      GetPlusAddress(last_committed_primary_main_frame_origin);
  if (maybe_address == std::nullopt) {
    if (trigger_source != kManualFallbackPlusAddresses &&
        !normalized_field_value.empty()) {
      return {};
    }
    Suggestion create_plus_address_suggestion(
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
        PopupItemId::kCreateNewPlusAddress);
    RecordAutofillSuggestionEvent(AutofillPlusAddressDelegate::SuggestionEvent::
                                      kCreateNewPlusAddressSuggested);
    create_plus_address_suggestion.icon = Suggestion::Icon::kPlusAddress;
    return {std::move(create_plus_address_suggestion)};
  }

  // Only suggest filling a plus address whose prefix matches the field's value.
  std::u16string address = base::UTF8ToUTF16(*maybe_address);
  if (trigger_source != kManualFallbackPlusAddresses &&
      !address.starts_with(normalized_field_value)) {
    return {};
  }
  Suggestion existing_plus_address_suggestion(
      std::move(address), PopupItemId::kFillExistingPlusAddress);
  RecordAutofillSuggestionEvent(AutofillPlusAddressDelegate::SuggestionEvent::
                                    kExistingPlusAddressSuggested);
  existing_plus_address_suggestion.icon = Suggestion::Icon::kPlusAddress;
  return {std::move(existing_plus_address_suggestion)};
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

void PlusAddressService::ConfirmPlusAddress(
    const url::Origin& origin,
    const std::string& plus_address,
    PlusAddressRequestCallback on_completed) {
  if (!is_enabled()) {
    return;
  }
  // Check the local mapping before attempting to confirm plus_address.
  if (std::optional<PlusProfile> stored_plus_profile = GetPlusProfile(origin);
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
      SavePlusAddress(origin, maybe_profile->plus_address);
    }
  } else {
    HandlePlusAddressRequestError(maybe_profile.error());
  }
  // Run callback last in case it's dependent on above changes.
  std::move(callback).Run(maybe_profile);
}

std::optional<std::string> PlusAddressService::GetPrimaryEmail() {
  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return std::nullopt;
  }
  // TODO(crbug.com/1467623): This is fine for prototyping, but eventually we
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
  return base::FeatureList::IsEnabled(features::kFeature) &&
         (features::kEnterprisePlusAddressServerUrl.Get() != "") &&
         identity_manager_ != nullptr &&
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
  if (!webdata_service_) {
    // Tests might not have a database. In this case, update the local cache
    // directly.
    plus_address_by_site_ = map;
    plus_addresses_.clear();
    for (const auto& [_, value] : map) {
      plus_addresses_.insert(value);
    }
    return;
  }
  // Update the database.
  webdata_service_->ClearPlusProfiles();
  for (const auto& [facet, plus_address] : map) {
    // TODO(b/322147254): Receive profile_ids from the PlusAddress backend. For
    // now, just assign any random identifier.
    webdata_service_->AddPlusProfile(
        {.profile_id = static_cast<int64_t>(base::RandUint64() >> 1),
         .facet = facet,
         .plus_address = plus_address,
         .is_confirmed = true});
  }
  // TODO(b/322147254): Re-reading the data we just wrote is unnecessary and the
  // local cache could be updated right away. This simply exists as a "sanity
  // check" that the round trip to the database works. Once the sync integration
  // has finished, `UpdatePlusAddressMap()` and the surrounding polling logic
  // will be removed.
  webdata_service_->GetPlusProfiles(this);
}

void PlusAddressService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(result->GetType(), PLUS_ADDRESS_RESULT);
  std::vector<PlusProfile> plus_profiles =
      static_cast<WDResult<std::vector<PlusProfile>>*>(result.get())
          ->GetValue();
  plus_address_by_site_.clear();
  plus_addresses_.clear();
  for (const PlusProfile& plus_profile : plus_profiles) {
    plus_address_by_site_.insert(
        {plus_profile.facet, plus_profile.plus_address});
    plus_addresses_.insert(plus_profile.plus_address);
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
    const GoogleServiceAuthError& error) {
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
  plus_address_by_site_.clear();
  plus_addresses_.clear();
  polling_timer_.Stop();
  plus_address_http_client_->Reset();
  if (webdata_service_) {
    webdata_service_->ClearPlusProfiles();
  }
}

std::set<std::string> PlusAddressService::GetAndParseExcludedSites() {
  std::set<std::string> parsed_excluded_sites;
  for (const std::string& site :
       base::SplitString(features::kPlusAddressExcludedSites.Get(), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    parsed_excluded_sites.insert(site);
  }
  return parsed_excluded_sites;
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
  PlusAddressMetrics::RecordAutofillSuggestionEvent(suggestion_event);
}

}  // namespace plus_addresses
