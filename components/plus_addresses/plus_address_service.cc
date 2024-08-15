// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include <memory>
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
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_blocklist_data.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_http_client_impl.h"
#include "components/plus_addresses/plus_address_jit_allocator.h"
#include "components/plus_addresses/plus_address_preallocator.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/prefs/pref_service.h"
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

using autofill::FormFieldData;
using autofill::Suggestion;
using autofill::SuggestionType;
using PasswordFormClassification =
    autofill::AutofillClient::PasswordFormClassification;

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
    std::string etld_plus_one = GetEtldPlusOne(origin);
    // TODO(crbug.com/327838014): Remove the fallback and use
    // `affiliations::FacetURI` in the tests.
    // For empty `etld_plus_one`, fallback to `origin`.
    facet = etld_plus_one.empty() ? origin.GetURL().spec() : etld_plus_one;
  }
  return facet;
}

// Returns `true` when we wish to offer plus address creation on a form with
// password manager classification `form_classification` and a focused field
// with id `focused_field_id`.
// If password manager did not recognize a username field or the username field
// is different from the focused field, this is always `true`. Otherwise,
// whether we offer plus address creation depends on the form type.
bool ShouldOfferPlusAddressCreationOnForm(
    const PasswordFormClassification& form_classification,
    autofill::FieldGlobalId focused_field_id) {
  if ((!form_classification.username_field ||
       *form_classification.username_field != focused_field_id) &&
      base::FeatureList::IsEnabled(
          features::kPlusAddressOfferCreationOnAllNonUsernameFields)) {
    return true;
  }
  switch (form_classification.type) {
    case PasswordFormClassification::Type::kNoPasswordForm:
    case PasswordFormClassification::Type::kSignupForm:
      return true;
    case PasswordFormClassification::Type::kLoginForm:
    case PasswordFormClassification::Type::kChangePasswordForm:
    case PasswordFormClassification::Type::kResetPasswordForm:
      return false;
    case PasswordFormClassification::Type::kSingleUsernameForm:
      return base::FeatureList::IsEnabled(
          features::kPlusAddressOfferCreationOnSingleUsernameForms);
  }
  NOTREACHED();
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
// `false` otherwise. If `kPlusAddressBlocklistEnabled` is enabled, this means
// that the domain's origin matches the `exclusion_pattern` regex and does not
// match the `exception_pattern` regex.
bool IsSiteExcluded(const base::flat_set<std::string>& excluded_sites,
                    const url::Origin& origin) {
  if (base::FeatureList::IsEnabled(features::kPlusAddressBlocklistEnabled)) {
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

  return excluded_sites.contains(GetEtldPlusOne(origin));
}

}  // namespace

PlusAddressService::PlusAddressService(
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
      submission_logger_(identity_manager,
                         base::BindRepeating(&PlusAddressService::IsPlusAddress,
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
                      base::BindRepeating(&PlusAddressService::IsEnabled,
                                          base::Unretained(this)));

  if (IsSyncingPlusAddresses() && webdata_service_) {
    webdata_service_observation_.Observe(webdata_service_.get());
    if (IsEnabled()) {
      webdata_service_->GetPlusProfiles(this);
    }
  }
  CreateAndStartTimer();
  identity_manager_observation_.Observe(identity_manager);

  if (!base::FeatureList::IsEnabled(features::kPlusAddressBlocklistEnabled)) {
    excluded_sites_ = GetAndParseExcludedSites();
  }
}

PlusAddressService::~PlusAddressService() {
  for (Observer& o : observers_) {
    o.OnPlusAddressServiceShutdown();
  }
}

bool PlusAddressService::IsPlusAddressFillingEnabled(
    const url::Origin& origin) const {
  // Check that the feature is enabled and the origin is supported (not opaque,
  // in the `excluded_sites_`, or is non http/https scheme)
  return IsEnabled() && IsSupportedOrigin(origin);
}

bool PlusAddressService::IsPlusAddressCreationEnabled(
    const url::Origin& origin,
    bool is_off_the_record) const {
  // Disabled plus address filling implies that plus address creation is
  // disabled.
  if (!IsPlusAddressFillingEnabled(origin)) {
    return false;
  }

  // Don't offer plus address creation for off-the-record sessions.
  if (is_off_the_record) {
    return false;
  }

  // We've met the prerequisites. If this isn't an OTR session and the global
  // settings toggle isn't off, plus address creation is supported.
  return !base::FeatureList::IsEnabled(features::kPlusAddressGlobalToggle) ||
         setting_service_->GetIsPlusAddressesEnabled();
}

bool PlusAddressService::ShouldShowManualFallback(
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
  return !base::FeatureList::IsEnabled(features::kPlusAddressGlobalToggle) ||
         setting_service_->GetIsPlusAddressesEnabled();
}

std::optional<PlusAddress> PlusAddressService::GetPlusAddress(
    const PlusProfile::facet_t& facet) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<PlusProfile> profile = GetPlusProfile(facet);
  return profile ? std::make_optional(std::move(profile->plus_address))
                 : std::nullopt;
}

void PlusAddressService::GetAffiliatedPlusProfiles(
    const url::Origin& origin,
    GetPlusProfilesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  plus_address_match_helper_.GetAffiliatedPlusProfiles(OriginToFacet(origin),
                                                       std::move(callback));
}

base::span<const PlusProfile> PlusAddressService::GetPlusProfiles() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_address_cache_.GetPlusProfiles();
}

std::optional<PlusProfile> PlusAddressService::GetPlusProfile(
    const PlusProfile::facet_t& facet) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* facet_uri = absl::get_if<affiliations::FacetURI>(&facet)) {
    if (!facet_uri->is_valid()) {
      return std::nullopt;
    }
  }

  return plus_address_cache_.FindByFacet(facet);
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
  // Update the in-memory plus profiles cache.
  plus_address_cache_.InsertProfile(profile);
  for (Observer& o : observers_) {
    o.OnPlusAddressesChanged(
        {PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile)});
  }
}

bool PlusAddressService::IsPlusAddress(
    const std::string& potential_plus_address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return plus_address_cache_.IsPlusAddress(potential_plus_address);
}

void PlusAddressService::GetSuggestions(
    const url::Origin& last_committed_primary_main_frame_origin,
    bool is_off_the_record,
    const PasswordFormClassification& focused_form_classification,
    const FormFieldData& focused_field,
    autofill::AutofillSuggestionTriggerSource trigger_source,
    GetSuggestionsCallback callback) {
  if (!IsPlusAddressFillingEnabled(last_committed_primary_main_frame_origin)) {
    std::move(callback).Run({});
    return;
  }

  plus_address_match_helper_.GetAffiliatedPlusProfiles(
      OriginToFacet(last_committed_primary_main_frame_origin),
      base::BindOnce(&PlusAddressService::OnGetAffiliatedPlusProfiles,
                     weak_factory_.GetWeakPtr(), focused_form_classification,
                     focused_field, trigger_source, is_off_the_record,
                     std::move(callback)));
}

Suggestion PlusAddressService::GetManagePlusAddressSuggestion() const {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_TEXT),
      SuggestionType::kManagePlusAddress);
  suggestion.icon = Suggestion::Icon::kGoogleMonochrome;
  return suggestion;
}

bool PlusAddressService::ShouldMixWithSingleFieldFormFillSuggestions() const {
  return base::FeatureList::IsEnabled(
      features::kPlusAddressAndSingleFieldFormFill);
}

void PlusAddressService::OnGetAffiliatedPlusProfiles(
    const PasswordFormClassification& focused_form_classification,
    const FormFieldData& focused_field,
    autofill::AutofillSuggestionTriggerSource trigger_source,
    bool is_off_the_record,
    GetSuggestionsCallback callback,
    std::vector<PlusProfile> affiliated_profiles) {
  using enum autofill::AutofillSuggestionTriggerSource;
  const std::u16string normalized_field_value =
      autofill::RemoveDiacriticsAndConvertToLowerCase(focused_field.value());

  if (affiliated_profiles.empty()) {
    // Do not offer creation in incognito mode.
    if (is_off_the_record) {
      std::move(callback).Run({});
      return;
    }

    // Do not offer creation if the setting is off.
    if (base::FeatureList::IsEnabled(features::kPlusAddressGlobalToggle) &&
        !setting_service_->GetIsPlusAddressesEnabled()) {
      std::move(callback).Run({});
      return;
    }

    // Do not offer creation on non-empty fields and certain form types (e.g.
    // login forms).
    if (trigger_source != kManualFallbackPlusAddresses &&
        (!normalized_field_value.empty() ||
         !ShouldOfferPlusAddressCreationOnForm(focused_form_classification,
                                               focused_field.global_id()))) {
      std::move(callback).Run({});
      return;
    }
    Suggestion create_plus_address_suggestion(
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
        SuggestionType::kCreateNewPlusAddress);
    RecordAutofillSuggestionEvent(AutofillPlusAddressDelegate::SuggestionEvent::
                                      kCreateNewPlusAddressSuggested);
    if constexpr (!BUILDFLAG(IS_ANDROID)) {
      create_plus_address_suggestion.labels = {
          {Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
    }
    create_plus_address_suggestion.icon = Suggestion::Icon::kPlusAddress;
    create_plus_address_suggestion.feature_for_new_badge =
        &features::kPlusAddressesEnabled;
    create_plus_address_suggestion.feature_for_iph =
        &feature_engagement::kIPHPlusAddressCreateSuggestionFeature;
#if BUILDFLAG(IS_ANDROID)
    create_plus_address_suggestion.iph_description_text =
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH_ANDROID);
#endif  // BUILDFLAG(IS_ANDROID)
    std::move(callback).Run({std::move(create_plus_address_suggestion)});
    return;
  }

  std::vector<Suggestion> suggestions;
  suggestions.reserve(affiliated_profiles.size());
  for (const PlusProfile& profile : affiliated_profiles) {
    Suggestion suggestion =
        Suggestion(base::UTF8ToUTF16(*profile.plus_address),
                   SuggestionType::kFillExistingPlusAddress);
    if constexpr (!BUILDFLAG(IS_ANDROID)) {
      suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
    }
    suggestion.icon = Suggestion::Icon::kPlusAddress;

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
  if (!IsEnabled()) {
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
  if (!IsEnabled()) {
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
    const PlusAddress& plus_address,
    PlusAddressRequestCallback on_completed) {
  if (!IsEnabled()) {
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

bool PlusAddressService::IsEnabled() const {
  if (features::kDisableForForbiddenUsers.Get() &&
      account_is_forbidden_.has_value() && account_is_forbidden_.value()) {
    return false;
  }
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

void PlusAddressService::CreateAndStartTimer() {
  if (!IsEnabled() || !features::kSyncWithEnterprisePlusAddressServer.Get() ||
      polling_timer_.IsRunning()) {
    return;
  }
  if (IsSyncingPlusAddresses()) {
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
  if (!IsEnabled()) {
    return;
  }
  plus_address_http_client_->GetAllPlusAddresses(base::BindOnce(
      [](PlusAddressService* service,
         const PlusAddressMapOrError& maybe_mapping) {
        if (maybe_mapping.has_value()) {
          if (service->IsEnabled()) {
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
  plus_address_cache_.Clear();
  for (const auto& [facet, address] : map) {
    // `UpdatePlusAddressMap()` is only called when sync support is disabled.
    // In this case, profile_ids don't matter.
    plus_address_cache_.InsertProfile(
        PlusProfile(/*profile_id=*/"", facet, address, /*is_confirmed=*/true));
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

  for (Observer& o : observers_) {
    o.OnPlusAddressesChanged(applied_changes);
  }
}

void PlusAddressService::OnWebDataServiceRequestDone(
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
  if (error.state() != GoogleServiceAuthError::NONE) {
    HandleSignout();
  } else {
    CreateAndStartTimer();
  }
}

void PlusAddressService::HandleSignout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsSyncingPlusAddresses()) {
    plus_address_cache_.Clear();
    polling_timer_.Stop();
  }
  plus_address_http_client_->Reset();
}


bool PlusAddressService::IsSupportedOrigin(const url::Origin& origin) const {
  if (origin.opaque() || IsSiteExcluded(excluded_sites_, origin)) {
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
    autofill::AutofillClient::PasswordFormClassification::Type form_type,
    autofill::SuggestionType suggestion_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  submission_logger_.OnPlusAddressSuggestionShown(
      manager, form, field, suggestion_context, form_type, suggestion_type,
      /*plus_address_count=*/plus_address_cache_.Size());
}

}  // namespace plus_addresses
