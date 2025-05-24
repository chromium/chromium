// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/country_codes/country_codes.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class PrefService;

namespace TemplateURLPrepopulateData {
struct PrepopulatedEngine;
}

namespace regional_capabilities {

class CountryIdHolder;

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
  };

  RegionalCapabilitiesService(
      PrefService& profile_prefs,
      std::unique_ptr<Client> regional_capabilities_client);
  ~RegionalCapabilitiesService() override;

  // Returns the country ID to use in the context of regional checks.
  // Can be overridden using `switches::kSearchEngineChoiceCountry`.
  // Note: Access to the raw value is restricted, see `CountryIdHolder` for
  // more details.
  CountryIdHolder GetCountryId();

  std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>
  GetRegionalPrepopulatedEngines();

  // Returns whether the profile country is a EEA member.
  //
  // Testing note: To control the value this returns in manual or automated
  // tests, see `switches::kSearchEngineChoiceCountry`.
  bool IsInEeaCountry();

  // Clears the country id cache to be able to change countries multiple times
  // in tests.
  void ClearCountryIdCacheForTesting();

#if BUILDFLAG(IS_ANDROID)
  // -- JNI Interface ---------------------------------------------------------

  // Returns a reference to the Java-side `RegionalCapabilitiesService`, lazily
  // creating it if needed.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // If the Java-side service has been created, commands it to destroy itself.
  void DestroyJavaObject();

  // See `IsInEeaCountry()`.
  jboolean IsInEeaCountry(JNIEnv* env);

  // -- JNI Interface End -----------------------------------------------------
#endif

 private:
  country_codes::CountryId GetCountryIdInternal();

  void InitializeCountryIdCache();
  std::optional<country_codes::CountryId> GetPersistedCountryId();
  void TrySetPersistedCountryId(country_codes::CountryId country_id);

  const raw_ref<PrefService> profile_prefs_;
  const std::unique_ptr<Client> client_;

  // Used to ensure that the value returned from `GetCountryId` never changes
  // in runtime (different runs can still return different values, though).
  std::optional<country_codes::CountryId> country_id_cache_;

#if BUILDFLAG(IS_ANDROID)
  // Corresponding Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif

  base::WeakPtrFactory<RegionalCapabilitiesService> weak_ptr_factory_{this};
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_
