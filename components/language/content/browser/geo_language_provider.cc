// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/geo_language_provider.h"

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/language/content/browser/language_code_locator_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"

namespace language {
namespace {

// Don't start requesting updates to IP-based approximation geolocation until
// this long after receiving the last one.
constexpr base::TimeDelta kMinUpdatePeriod = base::Days(1);

GeoLanguageProvider::Binder& GetBinderOverride() {
  static base::NoDestructor<GeoLanguageProvider::Binder> binder;
  return *binder;
}

}  // namespace

const char GeoLanguageProvider::kCachedGeoLanguagesPref[] =
    "language.geo_language_provider.cached_geo_languages";

const char GeoLanguageProvider::kTimeOfLastGeoLanguagesUpdatePref[] =
    "language.geo_language_provider.time_of_last_geo_languages_update";

GeoLanguageProvider::GeoLanguageProvider()
    : creation_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      prefs_(nullptr) {
  // Constructor is not required to run on |background_task_runner_|:
  DETACH_FROM_SEQUENCE(background_sequence_checker_);
}

GeoLanguageProvider::GeoLanguageProvider(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : creation_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      background_task_runner_(background_task_runner),
      prefs_(nullptr) {
  // Constructor is not required to run on |background_task_runner_|:
  DETACH_FROM_SEQUENCE(background_sequence_checker_);
}

GeoLanguageProvider::~GeoLanguageProvider() = default;

// static
GeoLanguageProvider* GeoLanguageProvider::GetInstance() {
  return base::Singleton<GeoLanguageProvider, base::LeakySingletonTraits<
                                                  GeoLanguageProvider>>::get();
}

// static
void GeoLanguageProvider::RegisterLocalStatePrefs(
    PrefRegistrySimple* const registry) {
  registry->RegisterListPref(kCachedGeoLanguagesPref);
  registry->RegisterDoublePref(kTimeOfLastGeoLanguagesUpdatePref, 0);
}

void GeoLanguageProvider::StartUp(PrefService* const prefs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);

  prefs_ = prefs;

  const base::Value::List& cached_languages_list =
      prefs_->GetList(kCachedGeoLanguagesPref);
  for (const auto& language_value : cached_languages_list) {
    languages_.push_back(language_value.GetString());
  }

  const double last_update =
      prefs_->GetDouble(kTimeOfLastGeoLanguagesUpdatePref);

  base::TimeDelta time_passed_since_update =
      base::Time::Now() - base::Time::FromTimeT(last_update);

  // Delay startup if languages have been updated within |kMinUpdatePeriod|.
  if (time_passed_since_update < kMinUpdatePeriod) {
    base::TimeDelta time_till_next_update =
        kMinUpdatePeriod - time_passed_since_update;
    background_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GeoLanguageProvider::BackgroundStartUp,
                       base::Unretained(this)),
        time_till_next_update);
  } else {
    // Continue startup in the background.
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GeoLanguageProvider::BackgroundStartUp,
                                  base::Unretained(this)));
  }
}

std::vector<std::string> GeoLanguageProvider::CurrentGeoLanguages() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  return languages_;
}

// static
void GeoLanguageProvider::OverrideBinderForTesting(Binder binder) {
  GetBinderOverride() = std::move(binder);
}

void GeoLanguageProvider::BackgroundStartUp() {
  // This binds background_sequence_checker_.
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);

  // Initialize location->language lookup library.
  language_code_locator_ = GetLanguageCodeLocator(prefs_);

  // Make initial query.
  QueryNextPosition();
}

void GeoLanguageProvider::BindIpGeolocationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  DCHECK(!geolocation_provider_.is_bound());

  mojo::Remote<device::mojom::PublicIpAddressGeolocationProvider>
      ip_geolocation_provider;
  auto receiver = ip_geolocation_provider.BindNewPipeAndPassReceiver();
  const auto& binder = GetBinderOverride();
  if (binder) {
    binder.Run(std::move(receiver));
  } else {
    content::GetDeviceService().BindPublicIpAddressGeolocationProvider(
        std::move(receiver));
  }

  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("geo_language_provider",
                                                 "network_location_request",
                                                 R"(
          semantics {
            sender: "GeoLanguage Provider"
          }
          policy {
            setting:
              "Users can disable this feature for translation requests in "
              "settings 'Languages', 'Language', 'Offer to translate'. Note "
              "that users can still manually trigger this feature via the "
              "right-click menu."
            chrome_policy {
              DefaultGeolocationSetting {
                DefaultGeolocationSetting: 2
              }
            }
          })");

  // Use the PublicIpAddressGeolocationProvider to bind ip_geolocation_service_.
  ip_geolocation_provider->CreateGeolocation(
      static_cast<net::MutablePartialNetworkTrafficAnnotationTag>(
          partial_traffic_annotation),
      geolocation_provider_.BindNewPipeAndPassReceiver(),
      device::mojom::GeolocationClientId::kGeoLanguageProvider);
  // No error handler required: If the connection is broken, QueryNextPosition
  // will bind it again.
}

void GeoLanguageProvider::QueryNextPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);

  if (geolocation_provider_.is_bound() && !geolocation_provider_.is_connected())
    geolocation_provider_.reset();
  if (!geolocation_provider_.is_bound())
    BindIpGeolocationService();

  geolocation_provider_->QueryNextPosition(base::BindOnce(
      &GeoLanguageProvider::OnIpGeolocationResponse, base::Unretained(this)));
}

void GeoLanguageProvider::OnIpGeolocationResponse(
    device::mojom::GeopositionResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);

  if (result->is_position() &&
      device::ValidateGeoposition(*result->get_position())) {
    // Update current languages on UI thread. We pass the lat/long pair so that
    // SetGeoLanguages can do the lookup on the UI thread. This is because the
    // language provider could decide to cache the values, requiring interaction
    // with the pref service.
    const auto& position = *result->get_position();
    creation_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GeoLanguageProvider::LookupAndSetLanguages,
                                  base::Unretained(this), position.latitude,
                                  position.longitude));
  }

  // Post a task to request a fresh lookup after |kMinUpdatePeriod|.
  background_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GeoLanguageProvider::QueryNextPosition,
                     base::Unretained(this)),
      kMinUpdatePeriod);
}

void GeoLanguageProvider::LookupAndSetLanguages(double lat, double lon) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  // Perform the lookup here (as opposed to the geolocation callback), as the
  // locator could cache the value in a pref, which must happen on the UI thread
  // This behavior is factored out in this function in order for tests to be
  // able to call SetGeoLanguages directly.
  SetGeoLanguages(language_code_locator_->GetLanguageCodes(lat, lon));
}

void GeoLanguageProvider::SetGeoLanguages(
    const std::vector<std::string>& languages) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  languages_ = languages;

  base::Value::List cache_list;
  for (const std::string& language : languages_) {
    cache_list.Append(language);
  }
  prefs_->SetList(kCachedGeoLanguagesPref, std::move(cache_list));
  prefs_->SetDouble(kTimeOfLastGeoLanguagesUpdatePref,
                    base::Time::Now().InSecondsFSinceUnixEpoch());
}

}  // namespace language
