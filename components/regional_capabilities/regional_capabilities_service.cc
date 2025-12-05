// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_service.h"

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_internals_data_holder.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/strings/grit/components_strings.h"
#include "regional_capabilities_metrics.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/regional_capabilities/android/jni_headers/RegionalCapabilitiesService_jni.h"
#endif

using ::country_codes::CountryId;

namespace regional_capabilities {
namespace {

// LINT.IfChange(CountryIdStoreStatus)
enum class CountryIdStoreStatus {
  kValidStaticCountryId = 0,
  // kDontClearInvalidCountry = 1, // Deprecated.
  kInvalidStaticPref = 2,
  kValidDynamicCountryId = 3,
  kInvalidDynamicPref = 4,
  kNoPersistedValue = 5,
  kMaxValue = kNoPersistedValue,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/search/enums.xml:UnknownCountryIdStored)

// Reads the country from preferences.
// As we are going through a migration across prefs, the actual pref used will
// depend on the local state. This function returns signals about the pref used,
// and signals whether the checked prefs were invalid and might need to be
// cleared.
std::pair<CountryId, base::flat_set<CountryIdStoreStatus>>
GetPersistedCountryIdAndSource(const PrefService& profile_prefs) {
  // Prefer the dynamic `prefs::kCountryID` if available and valid, otherwise
  // fallback to the static `prefs::kCountryIDAtInstall`.

  base::flat_set<CountryIdStoreStatus> sources;

  if (base::FeatureList::IsEnabled(switches::kDynamicProfileCountry) &&
      profile_prefs.HasPrefPath(prefs::kCountryID)) {
    const CountryId persisted_dynamic_country_id =
        CountryId::Deserialize(profile_prefs.GetInteger(prefs::kCountryID));
    // Even though invalid country ID should not be stored in prefs, it's safer
    // to double check it.
    //
    // For example, there might be changes in country ID validator.
    if (persisted_dynamic_country_id.IsValid()) {
      sources.emplace(CountryIdStoreStatus::kValidDynamicCountryId);
      return {persisted_dynamic_country_id, sources};
    }

    // Clear dynamic pref CountryID as it is invalid.
    sources.emplace(CountryIdStoreStatus::kInvalidDynamicPref);
  }

  if (profile_prefs.HasPrefPath(prefs::kCountryIDAtInstall)) {
    CountryId persisted_country_id = CountryId::Deserialize(
        profile_prefs.GetInteger(prefs::kCountryIDAtInstall));

    // Check and report on the validity of the initially persisted value.
    if (persisted_country_id.IsValid()) {
      sources.emplace(CountryIdStoreStatus::kValidStaticCountryId);
      return {persisted_country_id, sources};
    }

    // Clear static pref CountryID as it is invalid.
    sources.emplace(CountryIdStoreStatus::kInvalidStaticPref);
  }

  if (sources.empty()) {
    sources.emplace(CountryIdStoreStatus::kNoPersistedValue);
  }

  return {CountryId(), sources};
}

// Helper to make it possible to check for the synchronous completion of the
// `RegionalCapabilitiesService::Client::FetchCountryId()` call.
class ScopedCountryIdReceiver {
 public:
  base::OnceCallback<void(CountryId)> GetCaptureCallback() {
    return base::BindOnce(
        [](base::WeakPtr<ScopedCountryIdReceiver> receiver,
           CountryId country_id) {
          if (receiver) {
            receiver->received_country_ = country_id;
          }
        },
        weak_ptr_factory_.GetWeakPtr());
  }

  std::optional<CountryId> received_country() { return received_country_; }

 private:
  std::optional<CountryId> received_country_;

  base::WeakPtrFactory<ScopedCountryIdReceiver> weak_ptr_factory_{this};
};

// Returns a callback that dispatches the incoming value to `callback1` and
// `callback2`. Always forwards the incoming value to each of them, provided
// they're not null.
RegionalCapabilitiesService::Client::CountryIdCallback DispatchCountryId(
    RegionalCapabilitiesService::Client::CountryIdCallback callback1,
    RegionalCapabilitiesService::Client::CountryIdCallback callback2) {
  return base::BindOnce(
      [](RegionalCapabilitiesService::Client::CountryIdCallback cb1,
         RegionalCapabilitiesService::Client::CountryIdCallback cb2,
         CountryId incoming_country_id) {
        if (cb1) {
          std::move(cb1).Run(incoming_country_id);
        }
        if (cb2) {
          std::move(cb2).Run(incoming_country_id);
        }
      },
      std::move(callback1), std::move(callback2));
}

// Selects CountryID and corresponding source based on the following rules:
//
// If kDynamicProfileCountry feature is disabled, then
//   1. return persisted CountryID if valid, otherwise
//   2. return fetched current CountryID if valid, otherwise
//   3. return fallback current CountryID if valid, otherwise
//   4. return invalid CountryID
// in other words, persisted > fetched > fallback.
//
// If kDynamicProfileCountry feature is enabled, then
//   1. return fetched current CountryID if valid, otherwise
//   2. return persisted CountryID if valid, otherwise
//   3. return fallback current CountryID if valid, otherwise
//   4. return invalid CountryID
// in other words, fetched > persisted > fallback.
std::pair<CountryId, LoadedCountrySource> SelectCountryId(
    CountryId persisted_country,
    CountryId current_country,
    bool is_current_country_from_fallback) {
  // Let's check first all possible combinations when `persisted_country`
  // and/or `current_country` might be invalid.

  if (!persisted_country.IsValid() && !current_country.IsValid()) {
    return {CountryId(), LoadedCountrySource::kNoneAvailable};
  }

  // At this point either `persisted_country` or `current_country` might be
  // invalid.
  if (!persisted_country.IsValid()) {
    DCHECK(current_country.IsValid());
    return {current_country, LoadedCountrySource::kCurrentOnly};
  }
  if (!current_country.IsValid()) {
    DCHECK(persisted_country.IsValid());
    return {persisted_country, LoadedCountrySource::kPersistedOnly};
  }

  // At this point both `persisted_country` and `current_country` should be
  // valid.
  DCHECK(persisted_country.IsValid());
  DCHECK(current_country.IsValid());

  if (persisted_country == current_country) {
    return {persisted_country, LoadedCountrySource::kBothMatch};
  }

  // If the dynamic profile country feature is disabled, it's preferred
  // to return persisted country ID first.
  if (!base::FeatureList::IsEnabled(switches::kDynamicProfileCountry)) {
    return {persisted_country, LoadedCountrySource::kPersistedPreferred};
  }

  // At this point the `kDynamicProfileCountry` feature is enabled.
  DCHECK(base::FeatureList::IsEnabled(switches::kDynamicProfileCountry));

  // Fetched current CountryID is preferred over persisted CountryID.
  if (!is_current_country_from_fallback) {
    return {current_country, LoadedCountrySource::kCurrentPreferred};
  }

  // Persisted CountryID is preferred over fallback current CountryID.
  return {persisted_country,
          LoadedCountrySource::kPersistedPreferredOverFallback};
}

// Pass `out_scope_check_outcome` to collect information about the outcome of
// the program checks.
Program CountryIdToProgram(const CountryId& country_id) {
  static constexpr Program kCountryDerivedPrograms[] = {
#if BUILDFLAG(IS_IOS)
      // Only iOS can derive Taiyaki scope directly from the country.
      Program::kTaiyaki,
#endif

      Program::kWaffle,
  };

  for (Program program : kCountryDerivedPrograms) {
    if (IsInProgramRegion(program, country_id) &&
        IsClientCompatibleWithProgram(program)) {
      return program;
    }
  }

  return Program::kDefault;
}

std::optional<ProgramAndLocationMatch> GetProgramAndLocationMatch(
    Program program,
    CountryId profile_country,
    CountryId variations_latest_country) {
  if (program == Program::kDefault || !variations_latest_country.IsValid()) {
    // Checking program and variations location against each other
    // is irrelevant for the default program or when we don't have the
    // variations country.
    return std::nullopt;
  }

  if (profile_country == variations_latest_country) {
    return ProgramAndLocationMatch::SameAsProfileCountry;
  }

  if (IsInProgramRegion(program, variations_latest_country)) {
    return ProgramAndLocationMatch::SameRegionAsProgram;
  }

  return ProgramAndLocationMatch::NoMatch;
}

Program CountryOverrideToProgram(SearchEngineCountryOverride country_override) {
  return std::visit(
      absl::Overload{
          [](CountryId country_id) { return CountryIdToProgram(country_id); },
          [](RegionalProgramOverride program_override) {
            switch (program_override) {
              case RegionalProgramOverride::kTaiyaki:
                CHECK(IsClientCompatibleWithProgram(Program::kTaiyaki));
                return Program::kTaiyaki;
            }
            NOTREACHED();
          },
          [](SearchEngineCountryListOverride list_override) {
            switch (list_override) {
              case SearchEngineCountryListOverride::kEeaAll:
              case SearchEngineCountryListOverride::kEeaDefault:
                return Program::kWaffle;
            }
            NOTREACHED();
          },
      },
      country_override);
}

CountryId CountryOverrideToCountryId(
    SearchEngineCountryOverride country_override) {
  return std::visit(absl::Overload{
                        [](CountryId country_id) { return country_id; },
                        [](RegionalProgramOverride program_override) {
                          const ProgramSettings& settings =
                              GetSettingsForProgram(
                                  CountryOverrideToProgram(program_override));
                          // For programs allowing to be overridden this way,
                          // they should be configured to be able to be resolved
                          // to a specific country.
                          CHECK(!settings.associated_countries.empty());
                          return settings.associated_countries.front();
                        },
                        [](SearchEngineCountryListOverride list_override) {
                          return CountryId();
                        },
                    },
                    country_override);
}

}  // namespace

RegionalCapabilitiesService::RegionalCapabilitiesService(
    PrefService& profile_prefs,
    std::unique_ptr<Client> regional_capabilities_client)
    : profile_prefs_(profile_prefs),
      client_(std::move(regional_capabilities_client)) {
  CHECK(client_);
}

RegionalCapabilitiesService::~RegionalCapabilitiesService() {
#if BUILDFLAG(IS_ANDROID)
  DestroyJavaObject();
#endif
}

std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>
RegionalCapabilitiesService::GetRegionalPrepopulatedEngines() {
  if (HasSearchEngineCountryListOverride()) {
    auto country_override = std::get<SearchEngineCountryListOverride>(
        GetSearchEngineCountryOverride().value());

    switch (country_override) {
      case SearchEngineCountryListOverride::kEeaAll:
        return GetAllEeaRegionPrepopulatedEngines();
      case SearchEngineCountryListOverride::kEeaDefault:
        return GetDefaultPrepopulatedEngines();
    }
    NOTREACHED();
  }

  return GetPrepopulatedEngines(
      GetCountryIdInternal(), profile_prefs_.get(),
      GetActiveProgramSettings().search_engine_list_type);
}

bool RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion() {
  return GetChoiceScreenEligibilityConfig().has_value();
}

// static
bool RegionalCapabilitiesService::IsInSearchEngineChoiceScreenRegion(
    const country_codes::CountryId& tested_country_id) {
  return GetSettingsForProgram(CountryIdToProgram(tested_country_id))
      .choice_screen_eligibility_config.has_value();
}

bool RegionalCapabilitiesService::
    IsChoiceScreenCompatibleWithCurrentLocation() {
  CHECK(GetChoiceScreenEligibilityConfig().has_value())
      << "No choice screen config is present so it won't be shown. "
         "Checking the compatibility with the current location in "
         "this context is irrelevant and should not have happened.";
  if (!GetChoiceScreenEligibilityConfig()->restrict_to_associated_countries) {
    return true;
  }

  if (const auto override = GetSearchEngineCountryOverride();
      override.has_value() &&
      (std::holds_alternative<SearchEngineCountryListOverride>(*override) ||
       std::holds_alternative<RegionalProgramOverride>(*override))) {
    // When overriding the list or the program directly, skip the region checks.
    // This is a testing situation where we are manually overriding regional
    // settings, the region checks are not relevant as the current country gets
    // overridden too.
    return true;
  }

  if (!base::Contains(GetActiveProgramSettings().associated_countries,
                      client_->GetVariationsLatestCountryId())) {
    return false;
  }

  if (base::FeatureList::IsEnabled(switches::kStrictAssociatedCountriesCheck) &&
      GetCountryIdInternal() != client_->GetVariationsLatestCountryId()) {
    return false;
  }

  return true;
}

bool RegionalCapabilitiesService::CanRecordDisplayStateForCountry(
    CountryId display_state_country_id) {
  if (!base::Contains(GetActiveProgramSettings().associated_countries,
                      display_state_country_id)) {
    // Choice screen completions happen in context of a given regional program.
    // Based on the client state, the active program might change across
    // sessions. Since the metrics upload get tagged with the active program
    // that will be reflected in UMA filters, we avoid recording the histograms
    // to make sure they don't get filed under the wrong program. This is only
    // relevant when attempting to upload cached display state from a previous
    // session, as the program can't change during a given session.
    return false;
  }

  if (display_state_country_id != client_->GetVariationsLatestCountryId()) {
    // As the display state might be a proxy to pinpoint to a specific profile
    // country, we only record it if this data would not add extra location info
    // compared to what would be already present in the logs session (the
    // metrics session's country is assume to be variations latest).
    return false;
  }

  return true;
}

bool RegionalCapabilitiesService::
    ShouldRecordSearchEngineChoicesMadeFromSettings() {
  return GetActiveProgramSettings()
      .selection_from_settings_counts_as_choice_screen_choice;
}

#if !BUILDFLAG(IS_ANDROID)
std::optional<RegionalCapabilitiesService::ChoiceScreenDesign>
RegionalCapabilitiesService::GetChoiceScreenDesign() {
  switch (GetActiveProgramSettings().program) {
    case Program::kDefault:
      return std::nullopt;
    case Program::kTaiyaki:
      return RegionalCapabilitiesService::ChoiceScreenDesign{
          .title_string_id = IDS_SEARCH_ENGINE_CHOICE_PAGE_TITLE,
          .subtitle_1_string_id =
              IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_WITH_DEFINITION1,
          .subtitle_1_learn_more_suffix_string_id =
              IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK,
          .subtitle_1_learn_more_a11y_string_id =
              IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK_A11Y_LABEL,
          .subtitle_2_string_id =
              IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_WITH_DEFINITION2,
      };
    case Program::kWaffle:
      return RegionalCapabilitiesService::ChoiceScreenDesign{
          .title_string_id = IDS_SEARCH_ENGINE_CHOICE_PAGE_TITLE,
          .subtitle_1_string_id = IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE,
          .subtitle_1_learn_more_suffix_string_id =
              IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK,
          .subtitle_1_learn_more_a11y_string_id =
              IDS_SEARCH_ENGINE_CHOICE_PAGE_SUBTITLE_INFO_LINK_A11Y_LABEL,
      };
  }
  NOTREACHED();
}
#endif  // !BUILDFLAG(IS_ANDROID)

const std::optional<ChoiceScreenEligibilityConfig>&
RegionalCapabilitiesService::GetChoiceScreenEligibilityConfig() {
  return GetActiveProgramSettings().choice_screen_eligibility_config;
}

bool RegionalCapabilitiesService::IsInEeaCountry() {
  // Feature behaviour was directly based on the current country, as a
  // decentralised way to express a concept we are now framing as "program
  // settings". Here we check for the program reference directly as command line
  // overrides may be setting a program with a separate country engine list
  // override.
  // TODO(crbug.com/328040066): Introduce granular program settings APIs and
  // deprecate `IsInEeaCountry()` in favour of these.
  return GetActiveProgramSettings().program == Program::kWaffle;
}

RegionalCapabilitiesService::Client&
RegionalCapabilitiesService::GetClientForTesting() {
  return *client_;
}

CountryIdHolder RegionalCapabilitiesService::GetCountryId() {
  return CountryIdHolder(GetCountryIdInternal());
}

InternalsDataHolder RegionalCapabilitiesService::GetInternalsData() {
  return InternalsDataHolder(*this);
}

const ProgramSettings& RegionalCapabilitiesService::GetActiveProgramSettings() {
  if (std::optional<SearchEngineCountryOverride> country_override =
          GetSearchEngineCountryOverride();
      country_override.has_value()) {
    return GetSettingsForProgram(
        CountryOverrideToProgram(country_override.value()));
  }

  EnsureRegionalScopeCacheInitialized();

  return program_settings_cache_->get();
}

CountryId RegionalCapabilitiesService::GetCountryIdInternal() {
  std::optional<SearchEngineCountryOverride> country_override =
      GetSearchEngineCountryOverride();
  if (country_override.has_value()) {
    return CountryOverrideToCountryId(country_override.value());
  }

  EnsureRegionalScopeCacheInitialized();
  return country_id_cache_.value();
}

void RegionalCapabilitiesService::EnsureRegionalScopeCacheInitialized() {
  // The regional scope cache is made of these 2 values, their presence has to
  // be consistent.
  CHECK_EQ(country_id_cache_.has_value(), program_settings_cache_.has_value());
  if (country_id_cache_.has_value() && program_settings_cache_.has_value()) {
    return;
  }

  auto [persisted_country_id, sources] =
      GetPersistedCountryIdAndSource(profile_prefs_.get());
  for (const CountryIdStoreStatus& source : sources) {
    base::UmaHistogramEnumeration("Search.ChoiceDebug.UnknownCountryIdStored",
                                  source);
    if (source == CountryIdStoreStatus::kInvalidStaticPref) {
      profile_prefs_->ClearPref(prefs::kCountryIDAtInstall);
    } else if (source == CountryIdStoreStatus::kInvalidDynamicPref) {
      profile_prefs_->ClearPref(prefs::kCountryID);
    }
  }

  // Fetches the device country using `Client::FetchCountryId()`. Upon
  // completion, makes it available through `country_id_receiver` and also
  // forwards it to `completion_callback`.
  ScopedCountryIdReceiver country_id_receiver;
  client_->FetchCountryId(DispatchCountryId(
      // Callback to a weak_ptr, and like `country_id_receiver`, scoped to the
      // function only.
      country_id_receiver.GetCaptureCallback(),
      // Callback scoped to the lifetime of the service.
      base::BindOnce(&RegionalCapabilitiesService::TrySetPersistedCountryId,
                     weak_ptr_factory_.GetWeakPtr())));

  CountryId current_country =
      country_id_receiver.received_country().value_or(CountryId());
  bool is_current_country_from_fallback = false;
  if (!current_country.IsValid()) {
    // The initialization failed or did not complete synchronously. Use the
    // fallback value and don't persist it. If the fetch completes later, the
    // persisted country will be picked up at the next startup.
    current_country = client_->GetFallbackCountryId();
    is_current_country_from_fallback = true;
  }

  RecordVariationsCountryMatching(client_->GetVariationsLatestCountryId(),
                                  persisted_country_id, current_country,
                                  is_current_country_from_fallback);

  const std::pair<CountryId, LoadedCountrySource> selected_country_and_source =
      SelectCountryId(persisted_country_id, current_country,
                      is_current_country_from_fallback);

  country_id_cache_ = selected_country_and_source.first;

  Program program;

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          switches::kResolveRegionalCapabilitiesFromDevice)) {
    program = client_->GetDeviceProgram();

    if (!IsInProgramRegion(program, country_id_cache_.value())) {
      // Interim program inconsistencies originate from asynchronous nature of
      // their resolution. For the time being, use a reasonable default.
      program = Program::kDefault;
    }
  } else
#endif  // BUILDFLAG(IS_ANDROID)
  {
    program = CountryIdToProgram(country_id_cache_.value());
  }

  program_settings_cache_ = GetSettingsForProgram(program);

  RecordLoadedCountrySource(selected_country_and_source.second);
  if (auto program_and_location_match =
          GetProgramAndLocationMatch(program, country_id_cache_.value(),
                                     client_->GetVariationsLatestCountryId());
      program_and_location_match.has_value()) {
    RecordProgramAndLocationMatch(*program_and_location_match);
  }
}

ActiveRegionalProgram
RegionalCapabilitiesService::GetActiveProgramForMetrics() {
  switch (GetActiveProgramSettings().program) {
    case Program::kDefault:
      return ActiveRegionalProgram::kDefault;
    case Program::kTaiyaki:
      return ActiveRegionalProgram::kTaiyaki;
    case Program::kWaffle:
      return ActiveRegionalProgram::kWaffle;
  }
  NOTREACHED();
}

int RegionalCapabilitiesService::GetSerializedActiveProgram() {
  return SerializeProgram(GetActiveProgramSettings().program);
}

void RegionalCapabilitiesService::ClearCacheForTesting() {
  CHECK_IS_TEST();
  country_id_cache_.reset();
  program_settings_cache_.reset();
}

void RegionalCapabilitiesService::SetCacheForTesting(
    country_codes::CountryId country_id,
    const ProgramSettings& program_settings) {
  CHECK(!GetSearchEngineCountryOverride().has_value())
      << "Override either country or program settings, not both.";
  ClearCacheForTesting();
  country_id_cache_ = country_id;
  program_settings_cache_ = program_settings;
}

void RegionalCapabilitiesService::SetCacheForTesting(
    const ProgramSettings& program_settings) {
  SetCacheForTesting(program_settings.associated_countries.empty()
                         ? country_codes::CountryId()
                         : program_settings.associated_countries.front(),
                     program_settings);
}

const ProgramSettings&
RegionalCapabilitiesService::GetActiveProgramSettingsForTesting() {
  return GetActiveProgramSettings();
}

CountryId RegionalCapabilitiesService::GetPersistedCountryId() const {
  return GetPersistedCountryIdAndSource(profile_prefs_.get()).first;
}

void RegionalCapabilitiesService::TrySetPersistedCountryId(
    CountryId country_id) {
  if (!country_id.IsValid()) {
    return;
  }

  if (base::FeatureList::IsEnabled(switches::kDynamicProfileCountry)) {
    profile_prefs_->SetInteger(prefs::kCountryID, country_id.Serialize());
  }

  if (!profile_prefs_->HasPrefPath(prefs::kCountryIDAtInstall)) {
    // Deliberately do not override the current value if it has already been
    // set. Note that if we end up having to fall back to it and if the value
    // turns out to be invalid, at that time it will be cleared. It might then
    // be updated next time we try to persist the country.
    profile_prefs_->SetInteger(prefs::kCountryIDAtInstall,
                               country_id.Serialize());
  }
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
RegionalCapabilitiesService::GetJavaObject() {
  if (!java_ref_) {
    java_ref_.Reset(Java_RegionalCapabilitiesService_Constructor(
        jni_zero::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

void RegionalCapabilitiesService::DestroyJavaObject() {
  if (java_ref_) {
    Java_RegionalCapabilitiesService_destroy(jni_zero::AttachCurrentThread(),
                                             java_ref_);
    java_ref_.Reset();
  }
}

jboolean RegionalCapabilitiesService::IsInEeaCountry(JNIEnv* env) {
  return IsInEeaCountry();
}
#endif

}  // namespace regional_capabilities

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(RegionalCapabilitiesService)
#endif
