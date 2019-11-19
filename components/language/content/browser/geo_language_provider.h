// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_GEO_LANGUAGE_PROVIDER_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_GEO_LANGUAGE_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/language/content/browser/language_code_locator.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation.mojom.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace service_manager {
class Connector;
}

class PrefRegistrySimple;
class PrefService;

namespace language {
// GeoLanguageProvider is responsible for providing a "local" language derived
// from the approximate geolocation of the device based only on its public IP
// address.
// * Singleton class. Access through GetInstance().
// * Sequencing: Must be created and used on the same sequence.
class GeoLanguageProvider {
 public:
  static const char kCachedGeoLanguagesPref[];

  static GeoLanguageProvider* GetInstance();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Call this once near browser startup. Begins ongoing geo-language updates.
  // * Initializes location->language mapping in a low-priority background task.
  // * Until the first IP geolocation completes, CurrentGeoLanguages() will
  //   return an empty list.
  // |service_manager_connector| should not yet be bound to a sequence, e.g., it
  // should be the result of invoking ServiceManagerConnect::Clone() on another
  // connector.
  void StartUp(
      std::unique_ptr<service_manager::Connector> service_manager_connector,
      PrefService* prefs);

  // Returns the inferred ranked list of local languages based on the most
  // recently obtained approximate public-IP geolocation of the device.
  // * Returns a list of BCP-47 language codes.
  // * Returns an empty list in these cases:
  //   - StartUp() not yet called
  //   - Geolocation failed
  //   - Geolocation pending
  //   - Geolocation succeeded but no local language is mapped to that location
  std::vector<std::string> CurrentGeoLanguages() const;

 private:
  friend class GeoLanguageModelTest;
  friend class GeoLanguageProviderTest;

  GeoLanguageProvider();
  explicit GeoLanguageProvider(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~GeoLanguageProvider();
  friend struct base::DefaultSingletonTraits<GeoLanguageProvider>;

  // Performs actual work described in StartUp() above.
  void BackgroundStartUp();

  // Binds |ip_geolocation_service_| using a service_manager::Connector.
  void BindIpGeolocationService();

  // Requests the next available IP-based approximate geolocation from
  // |ip_geolocation_service_|, binding |ip_geolocation_service_| first if
  // necessary.
  void QueryNextPosition();

  // Lookup the languages from the lat/lon pair, and pass them to
  // SetGeoLanguages. Must be called on the UI thread.
  void LookupAndSetLanguages(double lat, double lon);

  // Updates the list of BCP-47 language codes that will be returned by calls to
  // CurrentGeoLanguages().
  // Must be called on the UI thread.
  void SetGeoLanguages(const std::vector<std::string>& languages);

  // Callback for updates from |ip_geolocation_service_|.
  void OnIpGeolocationResponse(device::mojom::GeopositionPtr geoposition);

  // List of BCP-47 language code inferred from public-IP geolocation.
  // May be empty. See comment on CurrentGeoLanguages() above.
  std::vector<std::string> languages_;

  // Service manager connector for use on background_task_runner_.
  std::unique_ptr<service_manager::Connector> service_manager_connector_;

  // Connection to the IP geolocation service.
  mojo::Remote<device::mojom::Geolocation> geolocation_provider_;

  // Location -> Language lookup library.
  std::unique_ptr<language::LanguageCodeLocator> language_code_locator_;

  // Runner for tasks that should run on the creation sequence.
  scoped_refptr<base::SequencedTaskRunner> creation_task_runner_;

  // Runner for low priority background tasks.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Sequence checker for methods that must run on the creation sequence.
  SEQUENCE_CHECKER(creation_sequence_checker_);

  // Sequence checker for background_task_runner_.
  SEQUENCE_CHECKER(background_sequence_checker_);

  // The pref service used to cached the latest latitude/longitude pair
  // obtained.
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(GeoLanguageProvider);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_GEO_LANGUAGE_PROVIDER_H_
