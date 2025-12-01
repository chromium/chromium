// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/country_codes/country_codes.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/regional_capabilities/program_settings.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class PrefService;

namespace TemplateURLPrepopulateData {
struct PrepopulatedEngine;
}

namespace regional_capabilities {

enum class ActiveRegionalProgram;
class CountryIdHolder;
class InternalsDataHolder;
enum class Program;

// Service for managing the state related to Search Engine Choice (mostly
// for the country information).
//
// Various kinds of countries:
// - Variations country: Obtained from the variations service, this is the one
//   we get through the experiment framework. Exists in 2 variants: "Latest"
//   (changes once per run) and "Permanent" (changes at each milestone). See
//   `variations::VariationsService` for more details.
// - Device country: Represents the country with which the device is associated
//   and is provided by OS-level APIs. See `country_codes::GetCurrentCountryID`
//   for the common way we access it. Some platforms (Android or ChromeOS,
//   notably) need some alternative ways to obtain a more accurate values.
// - Profile country: The country that the `RegionalCapabilitiesService`
//   considers to apply to the current profile, and that is used to compute the
//   capabilities. It is persisted to profile prefs, and how it gets updated
//   varies by platform.
class RegionalCapabilitiesService : public KeyedService {
 public:
  // Helper that is responsible for providing the service with country data,
  // that could be coming from platform-specific or //chrome layer sources.
  class Client {
   public:
    using CountryIdCallback =
        base::OnceCallback<void(country_codes::CountryId)>;

    virtual ~Client() = default;

    // See `VariationsService::GetLatestCountry()`. Exposed through the
    // `Client` interface to abstract away platform-specific ways to access
    // the service.
    virtual country_codes::CountryId GetVariationsLatestCountryId() = 0;

    // Synchronously returns a country that could be used as the device country
    // for the current run.
    // Is called by the service when `FetchCountryId` does not complete
    // synchronously.
    virtual country_codes::CountryId GetFallbackCountryId() = 0;

    // Fetches country associated with the device via OS-level APIs, and
    // when/if successfully obtained, returns it by running
    // `country_id_fetched_callback`.
    // If it is not run synchronously, `GetFallbackCountryId()` will be used by
    // the service for the current run.
    virtual void FetchCountryId(
        CountryIdCallback country_id_fetched_callback) = 0;

#if BUILDFLAG(IS_ANDROID)
    // Synchronously reads the device's regional capabilities program
    // configuration.
    virtual Program GetDeviceProgram() = 0;
#endif
  };

  // Contains the string IDs for the UI elements on the search engine choice
  // screen. This is returned for regions that require a choice screen.
  struct ChoiceScreenDesign {
    int title_string_id;
    int subtitle_1_string_id;
    // String id for learn more with a link. This learn more needs to be
    // appended to `subtitle_1_string_id`.
    int subtitle_1_learn_more_suffix_string_id;
    // String id for the learn more accessibility.
    int subtitle_1_learn_more_a11y_string_id;
    std::optional<int> subtitle_2_string_id;
  };

  RegionalCapabilitiesService(
      PrefService& profile_prefs,
      std::unique_ptr<Client> regional_capabilities_client);
  ~RegionalCapabilitiesService() override;

  // -- Getters for regional capabilities -------------------------------------

  std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>
  GetRegionalPrepopulatedEngines();

  // Returns whether the profile is associated with a region in which we can
  // show a search engine choice screen.
  bool IsInSearchEngineChoiceScreenRegion();

  // Returns whether the tested country ID is associated with a region in which
  // we can show a search engine choice screen.
  static bool IsInSearchEngineChoiceScreenRegion(
      const country_codes::CountryId& tested_country_id);

  // Returns true when the choice screen eligibility check against country
  // association is not required, or if the current location is compatible with
  // the regional scope.
  bool IsChoiceScreenCompatibleWithCurrentLocation();

  // Returns whether display state metrics can be recorded.
  // `display_state_country_id` is passed by the caller as this may be used to
  // check the compatibility of display states cached from previous sessions.
  bool CanRecordDisplayStateForCountry(
      country_codes::CountryId display_state_country_id);

  bool ShouldRecordSearchEngineChoicesMadeFromSettings();

#if !BUILDFLAG(IS_ANDROID)
  // Returns the appropriate choice screen design strings for the active
  // program, if one is required. Returns `std::nullopt` if the region does not
  // require a search engine choice screen.
  std::optional<ChoiceScreenDesign> GetChoiceScreenDesign();
#endif  // !BUILDFLAG(IS_ANDROID)

  const std::optional<ChoiceScreenEligibilityConfig>&
  GetChoiceScreenEligibilityConfig();

  // -- Internal utils & deprecated getters -----------------------------------

  // Returns whether the profile country is a EEA member.
  //
  // Testing note: To control the value this returns in manual or automated
  // tests, see `switches::kSearchEngineChoiceCountry`.
  // DEPRECATED: Prefer using getters for regional capabilities above.
  // TODO(crbug.com/394235956): Migrate callsites and remove this.
  bool IsInEeaCountry();

  // Returns the country ID to use in the context of regional checks.
  // Can be overridden using `switches::kSearchEngineChoiceCountry`.
  // Note: Access to the raw value is restricted, see `CountryIdHolder` for
  // more details.
  CountryIdHolder GetCountryId();

  InternalsDataHolder GetInternalsData();

  // Returns the metrics enum for the active regional program. This is used for
  // logging only.
  ActiveRegionalProgram GetActiveProgramForMetrics();

  // Returns an opaque `int` value representing the program.
  int GetSerializedActiveProgram();

  // -- Test Utils & Accessors ------------------------------------------------

  // Clears the caches to be able to change countries multiple times in tests.
  void ClearCacheForTesting();

  // Tests can control ProgramSettings in one of two ways:
  // 1. Specifying a value via the `switches::kSearchEngineChoiceCountry`
  //    command line argument, in which case the program settings are not cached
  //    and entirely depend how this command line is parsed. It could be
  //    directly setting a program, or rely on the full country / platform /
  //    form factor combination. See the argument's documentation for more info.
  // 2. Defining the exact country and program settings via this setter. This
  // allows tests more fine-grained control over
  //    the particular attributes they are testing, and means they are not
  //    constrained by dependencies or the particular platforms a particular
  //    program is supported on.
  //
  // Overriding program settings will prevent using the command line country
  // override, and vice versa. This is enforced by a CHECK in
  // `SetCacheForTesting()`.
  void SetCacheForTesting(country_codes::CountryId, const ProgramSettings&);

  // Forwards to `SetCacheForTesting(CountryId, const ProgramSettings&)`,
  // deriving from the `ProgramSettings`'s associated countries. If no country
  // is available, the invalid `CountryId()` will be used.
  void SetCacheForTesting(const ProgramSettings&);

  const ProgramSettings& GetActiveProgramSettingsForTesting();

  // Returns a reference to the client.
  Client& GetClientForTesting();

  // -- JNI Interface ---------------------------------------------------------
#if BUILDFLAG(IS_ANDROID)
  // Returns a reference to the Java-side `RegionalCapabilitiesService`, lazily
  // creating it if needed.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // If the Java-side service has been created, commands it to destroy itself.
  void DestroyJavaObject();

  // See `IsInEeaCountry()`.
  jboolean IsInEeaCountry(JNIEnv* env);
#endif
  // -- JNI Interface End ----------------------------------------------------

 private:
  friend class InternalsDataHolder;

  // Returns how features should adjust themselves based on the active country
  // or program.
  const ProgramSettings& GetActiveProgramSettings();

  country_codes::CountryId GetCountryIdInternal();

  void EnsureRegionalScopeCacheInitialized();
  country_codes::CountryId GetPersistedCountryId() const;
  void TrySetPersistedCountryId(country_codes::CountryId country_id);

  const raw_ref<PrefService> profile_prefs_;
  const std::unique_ptr<Client> client_;

  // -- Regional scope cache --
  // Used to ensure that the value returned from associated getters doesn't
  // change at runtime (different runs can still return different values,
  // though).
  std::optional<country_codes::CountryId> country_id_cache_;
  std::optional<raw_ref<const ProgramSettings>> program_settings_cache_;
  // -- cache end --

#if BUILDFLAG(IS_ANDROID)
  // Corresponding Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif

  base::WeakPtrFactory<RegionalCapabilitiesService> weak_ptr_factory_{this};
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_
