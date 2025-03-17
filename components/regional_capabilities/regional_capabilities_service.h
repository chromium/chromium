// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class PrefService;

namespace regional_capabilities {

class CountryIdHolder;

// Service for managing the state related to Search Engine Choice (mostly
// for the country information).
class RegionalCapabilitiesService : public KeyedService {
 public:
  // Helper that is responsible for providing the service with country data,
  // that could be coming from platform-specific or //chrome layer sources.
  class Client {
   public:
    using CountryIdCallback = base::OnceCallback<void(int)>;

    virtual ~Client() = default;

    // Synchronously returns a country to use in current run for this profile.
    //
    // The default implementation uses `country_codes::GetCurrentCountryID()`.
    virtual int GetFallbackCountryId() = 0;

    // Computes a country to associate with this profile, returning it by
    // running `country_id_fetched_callback`. If it is not run synchronously,
    // `GetFallbackCountryId()` should be used by the service for the current
    // run. `country_id_fetched_callback` should be called only if a country was
    // successfully obtained.
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
  int GetCountryIdInternal();

  // Checks whether the persisted
  void InitializeCountryIdCache();

  const raw_ref<PrefService> profile_prefs_;
  const std::unique_ptr<Client> client_;

  // Used to ensure that the value returned from `GetCountryId` never changes
  // in runtime (different runs can still return different values, though).
  std::optional<int> country_id_cache_;

#if BUILDFLAG(IS_ANDROID)
  // Corresponding Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif

  base::WeakPtrFactory<RegionalCapabilitiesService> weak_ptr_factory_{this};
};

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SERVICE_H_
