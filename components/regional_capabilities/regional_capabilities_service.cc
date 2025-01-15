// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_service.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "components/regional_capabilities/android/jni_headers/RegionalCapabilitiesServiceClientAndroid_jni.h"
#endif

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

}  // namespace

// --- RegionalCapabilitiesService::Client ------------------------------------

RegionalCapabilitiesService::Client::~Client() = default;

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX)
void RegionalCapabilitiesService::Client::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
#if BUILDFLAG(IS_ANDROID)
  // On Android get it from a device API in Java.
  // Usage of `WeakPtr` is crucial here, as `RegionalCapabilitiesService` is
  // not guaranteed to be alive when the response from Java arrives.
  auto heap_callback =
      std::make_unique<CountryIdCallback>(std::move(on_country_id_fetched));
  // The ownership of the callback on the heap is passed to Java. It will be
  // deleted by JNI_RegionalCapabilitiesService_ProcessDeviceCountryResponse.
  Java_RegionalCapabilitiesServiceClientAndroid_requestDeviceCountry(
      base::android::AttachCurrentThread(),
      reinterpret_cast<intptr_t>(heap_callback.release()));
#else
  // On other platforms, `GetCurrentCountryID()` already returns a reliable
  // value.
  std::move(on_country_id_fetched).Run(country_codes::GetCurrentCountryID());
#endif
}
#endif

#if BUILDFLAG(IS_ANDROID)
void JNI_RegionalCapabilitiesServiceClientAndroid_ProcessDeviceCountryResponse(
    JNIEnv* env,
    jlong ptr_to_native_callback,
    const base::android::JavaParamRef<jstring>& j_device_country) {
  // Using base::WrapUnique ensures that the callback is deleted when this goes
  // out of scope.
  using CountryIdCallback =
      RegionalCapabilitiesService::Client::CountryIdCallback;
  std::unique_ptr<CountryIdCallback> heap_callback = base::WrapUnique(
      reinterpret_cast<CountryIdCallback*>(ptr_to_native_callback));
  CHECK(heap_callback);
  if (!j_device_country) {
    return;
  }
  std::string device_country =
      base::android::ConvertJavaStringToUTF8(env, j_device_country);
  int device_country_id =
      country_codes::CountryStringToCountryID(device_country);
  if (device_country_id == country_codes::kCountryIDUnknown) {
    return;
  }
  std::move(*heap_callback).Run(device_country_id);
}
#endif

// --- RegionalCapabilitiesService --------------------------------------------

RegionalCapabilitiesService::RegionalCapabilitiesService(
    PrefService& profile_prefs,
    std::unique_ptr<Client> regional_capabilities_client)
    : profile_prefs_(profile_prefs),
      client_(std::move(regional_capabilities_client)) {
  CHECK(client_);
}

RegionalCapabilitiesService::~RegionalCapabilitiesService() = default;

int RegionalCapabilitiesService::GetCountryId() {
  std::optional<SearchEngineCountryOverride> country_override =
      GetSearchEngineCountryOverride();
  if (country_override.has_value()) {
    if (std::holds_alternative<int>(country_override.value())) {
      return std::get<int>(country_override.value());
    }
    return country_codes::kCountryIDUnknown;
  }

  if (!country_id_cache_.has_value()) {
    InitializeCountryIdCache();
  }

  return country_id_cache_.value();
}

void RegionalCapabilitiesService::InitializeCountryIdCache() {
  // TODO(b:328040066): Move `kCountryIDAtInstall` pref declaration in this
  // class.
  std::optional<int> country_id;

  // Check the validity of the initially persisted value, if present.
  if (profile_prefs_->HasPrefPath(country_codes::kCountryIDAtInstall)) {
    country_id = profile_prefs_->GetInteger(country_codes::kCountryIDAtInstall);
    if (country_id.value() != country_codes::kCountryIDUnknown) {
      base::UmaHistogramEnumeration(kUnknownCountryIdStored,
                                    UnknownCountryIdStored::kValidCountryId);
    } else {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
      if (base::FeatureList::IsEnabled(switches::kClearPrefForUnknownCountry)) {
        profile_prefs_->ClearPref(country_codes::kCountryIDAtInstall);
        base::UmaHistogramEnumeration(kUnknownCountryIdStored,
                                      UnknownCountryIdStored::kClearedPref);
        country_id.reset();
      }
#endif
      base::UmaHistogramEnumeration(
          kUnknownCountryIdStored,
          UnknownCountryIdStored::kDontClearInvalidCountry);
    }
  }

  if (!country_id.has_value()) {
    client_->FetchCountryId(base::BindOnce(
        [](base::WeakPtr<RegionalCapabilitiesService> service, int country_id) {
          if (service && country_id != country_codes::kCountryIDUnknown) {
            service->profile_prefs_->SetInteger(
                country_codes::kCountryIDAtInstall, country_id);
          }
        },
        weak_ptr_factory_.GetWeakPtr()));
    if (profile_prefs_->HasPrefPath(country_codes::kCountryIDAtInstall)) {
      // The initialization above completed synchronously, return its outcome.
      country_id =
          profile_prefs_->GetInteger(country_codes::kCountryIDAtInstall);
    } else {
      // The initialization failed or did not complete synchronously. Fall
      // back to `country_codes::GetCurrentCountryID()` without persisting it.
      // If the fetch completes later, the country will be picked up at the
      // next startup.
      country_id = country_codes::GetCurrentCountryID();
    }
  }

  country_id_cache_ = country_id.value();
}

void RegionalCapabilitiesService::ClearCountryIdCacheForTesting() {
  CHECK_IS_TEST();
  country_id_cache_.reset();
}

}  // namespace regional_capabilities
