// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_service.h"

#include <optional>
#include <variant>
#include <vector>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
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

constexpr char kUnknownCountryIdStored[] =
    "Search.ChoiceDebug.UnknownCountryIdStored";

// LINT.IfChange(UnknownCountryIdStored)
enum class UnknownCountryIdStored {
  kValidCountryId = 0,
  kDontClearInvalidCountry = 1,
  kClearedPref = 2,
  kMaxValue = kClearedPref,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/search/enums.xml:UnknownCountryIdStored)

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

CountryId SelectCountryId(std::optional<CountryId> persisted_country,
                          CountryId current_country) {
  if (!persisted_country.has_value() && !current_country.IsValid()) {
    RecordLoadedCountrySource(LoadedCountrySource::kNoneAvailable);
    return CountryId();
  }

  if (!persisted_country.has_value()) {
    // We deliberately don't check `persisted_country` validity here. This
    // means it's still possible below that we might end up returning an
    // invalid country in the case where we obtained that invalid value
    // from the preferences. crbug.com/399878483 and crbug.com/399879272
    // should contribute to getting rid of that issue.
    CHECK(current_country.IsValid());
    RecordLoadedCountrySource(LoadedCountrySource::kCurrentOnly);
    return current_country;
  }

  LoadedCountrySource loaded_country_source;
  if (!current_country.IsValid()) {
    CHECK(persisted_country.has_value());
    loaded_country_source = LoadedCountrySource::kPersistedOnly;
  } else if (current_country == persisted_country.value()) {
    loaded_country_source = LoadedCountrySource::kBothMatch;
  } else {
    loaded_country_source = LoadedCountrySource::kPersistedPreferred;
  }

  RecordLoadedCountrySource(loaded_country_source);
  return persisted_country.value();
}

const ProgramSettings* CountryIdToProgram(CountryId country_id) {
  if (regional_capabilities::IsEeaCountry(country_id)) {
    return &kWaffleSettings;
  }

  return &kDefaultSettings;
}

const ProgramSettings* CountryOverrideToProgram(
    SearchEngineCountryOverride country_override) {
  return std::visit(
      absl::Overload{
          [](CountryId country_id) { return CountryIdToProgram(country_id); },
          [](SearchEngineCountryListOverride list_override) {
            switch (list_override) {
              case SearchEngineCountryListOverride::kEeaAll:
              case SearchEngineCountryListOverride::kEeaDefault:
                return &kWaffleSettings;
            }
          },
      },
      country_override);
}

CountryId CountryOverrideToCountryId(
    SearchEngineCountryOverride country_override) {
  return std::visit(absl::Overload{
                        [](CountryId country_id) { return country_id; },
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

CountryIdHolder RegionalCapabilitiesService::GetCountryId() {
  return CountryIdHolder(GetCountryIdInternal());
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
  }

  return GetPrepopulatedEngines(
      GetCountryIdInternal(), profile_prefs_.get(),
      GetActiveProgramSettings().search_engine_list_type);
}

bool RegionalCapabilitiesService::IsInEeaCountry() {
  // Feature behaviour was directly based on the current country, as a
  // decentralised way to express a concept we are now framing as "program
  // settings". Here we check for the program reference directly as command line
  // overrides may be setting a program with a separate country engine list
  // override.
  // TODO(crbug.com/328040066): Introduce granular program settings APIs and
  // deprecate `IsInEeaCountry()` in favour of these.
  return &GetActiveProgramSettings() == &kWaffleSettings;
}

const ProgramSettings& RegionalCapabilitiesService::GetActiveProgramSettings() {
  if (std::optional<SearchEngineCountryOverride> country_override =
          GetSearchEngineCountryOverride();
      country_override.has_value()) {
    return CHECK_DEREF(CountryOverrideToProgram(country_override.value()));
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

  std::optional<CountryId> persisted_country_id = GetPersistedCountryId();

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

  CountryId current_device_country =
      country_id_receiver.received_country().value_or(CountryId());
  bool is_device_country_from_fallback = false;
  if (!current_device_country.IsValid()) {
    // The initialization failed or did not complete synchronously. Use the
    // fallback value and don't persist it. If the fetch completes later, the
    // persisted country will be picked up at the next startup.
    current_device_country = client_->GetFallbackCountryId();
    is_device_country_from_fallback = true;
  }

  RecordVariationsCountryMatching(client_->GetVariationsLatestCountryId(),
                                  persisted_country_id.value_or(CountryId()),
                                  current_device_country,
                                  is_device_country_from_fallback);

  country_id_cache_ =
      SelectCountryId(persisted_country_id, current_device_country);
  program_settings_cache_ =
      CHECK_DEREF(CountryIdToProgram(country_id_cache_.value()));
}

void RegionalCapabilitiesService::ClearCountryIdCacheForTesting() {
  CHECK_IS_TEST();
  country_id_cache_.reset();
}

std::optional<CountryId> RegionalCapabilitiesService::GetPersistedCountryId() {
  if (!profile_prefs_->HasPrefPath(prefs::kCountryIDAtInstall)) {
    return std::nullopt;
  }

  CountryId persisted_country_id = CountryId::Deserialize(
      profile_prefs_->GetInteger(prefs::kCountryIDAtInstall));

  // Check and report on the validity of the initially persisted value.
  if (persisted_country_id.IsValid()) {
    base::UmaHistogramEnumeration(kUnknownCountryIdStored,
                                  UnknownCountryIdStored::kValidCountryId);
    return persisted_country_id;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(switches::kClearPrefForUnknownCountry)) {
    profile_prefs_->ClearPref(prefs::kCountryIDAtInstall);
    base::UmaHistogramEnumeration(kUnknownCountryIdStored,
                                  UnknownCountryIdStored::kClearedPref);
    return std::nullopt;
  }
#endif

  base::UmaHistogramEnumeration(
      kUnknownCountryIdStored,
      UnknownCountryIdStored::kDontClearInvalidCountry);
  return persisted_country_id;
}

void RegionalCapabilitiesService::TrySetPersistedCountryId(
    CountryId country_id) {
  if (!country_id.IsValid()) {
    return;
  }
  if (profile_prefs_->HasPrefPath(prefs::kCountryIDAtInstall)) {
    // Deliberately do not override the current value. This would be a
    // dedicated feature like `kDynamicProfileCountryMetrics` for example.
    return;
  }

  profile_prefs_->SetInteger(prefs::kCountryIDAtInstall,
                             country_id.Serialize());
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
