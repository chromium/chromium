// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/geo_language_provider.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/language/content/browser/language_code_locator_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/device/public/mojom/public_ip_address_geolocation_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace language {
namespace {

// Don't start requesting updates to IP-based approximation geolocation until
// this long after receiving the last one.
constexpr base::TimeDelta kMinUpdatePeriod = base::TimeDelta::FromDays(1);

}  // namespace

const char GeoLanguageProvider::kCachedGeoLanguagesPref[] =
    "language.geo_language_provider.cached_geo_languages";

GeoLanguageProvider::GeoLanguageProvider()
    : creation_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      background_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      prefs_(nullptr) {
  // Constructor is not required to run on |background_task_runner_|:
  DETACH_FROM_SEQUENCE(background_sequence_checker_);
}

GeoLanguageProvider::GeoLanguageProvider(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : creation_task_runner_(base::SequencedTaskRunnerHandle::Get()),
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
}

void GeoLanguageProvider::StartUp(
    std::unique_ptr<service_manager::Connector> service_manager_connector,
    PrefService* const prefs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);

  prefs_ = prefs;

  const base::ListValue* const cached_languages_list =
      prefs_->GetList(kCachedGeoLanguagesPref);
  for (const auto& language_value : *cached_languages_list) {
    languages_.push_back(language_value.GetString());
  }

  service_manager_connector_ = std::move(service_manager_connector);
  // Continue startup in the background.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GeoLanguageProvider::BackgroundStartUp,
                                base::Unretained(this)));
}

std::vector<std::string> GeoLanguageProvider::CurrentGeoLanguages() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(creation_sequence_checker_);
  return languages_;
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

  // Bind a PublicIpAddressGeolocationProvider.
  mojo::Remote<device::mojom::PublicIpAddressGeolocationProvider>
      ip_geolocation_provider;
  service_manager_connector_->Connect(
      device::mojom::kServiceName,
      ip_geolocation_provider.BindNewPipeAndPassReceiver());

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
      geolocation_provider_.BindNewPipeAndPassReceiver());
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
    device::mojom::GeopositionPtr geoposition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);

  // Update current languages on UI thread. We pass the lat/long pair so that
  // SetGeoLanguages can do the lookup on the UI thread. This is because the
  // language provider could decide to cache the values, requiring interaction
  // with the pref service.
  creation_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GeoLanguageProvider::LookupAndSetLanguages,
                                base::Unretained(this), geoposition->latitude,
                                geoposition->longitude));

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

  base::ListValue cache_list;
  for (size_t i = 0; i < languages_.size(); ++i) {
    cache_list.Set(i, std::make_unique<base::Value>(languages_[i]));
  }
  prefs_->Set(kCachedGeoLanguagesPref, cache_list);
}

}  // namespace language
