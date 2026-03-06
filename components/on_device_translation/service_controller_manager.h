// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_

#include <map>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/on_device_translation/service_controller.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

class PrefService;

namespace on_device_translation {

class OnDeviceTranslationServiceController;
class ServiceControllerManagerFactory;

// Manages the OnDeviceTranslationServiceControllers for a BrowserContext.
// This class is responsible for creating the per origin
// OnDeviceTranslationServiceController.
class ServiceControllerManager : public KeyedService {
 public:
  explicit ServiceControllerManager(
      PrefService* local_state,
      base::PassKey<ServiceControllerManagerFactory>);
  ~ServiceControllerManager() override;

  // Constructor for testing.
  explicit ServiceControllerManager(PrefService* local_state);

  ServiceControllerManager(const ServiceControllerManager&) = delete;
  ServiceControllerManager& operator=(const ServiceControllerManager&) = delete;

  // Creates a translator class that implements `mojom::Translator` for the
  // given language pair.
  virtual void CreateTranslator(
      const url::Origin& origin,
      const std::string& source_lang,
      const std::string& target_lang,
      OnDeviceTranslationController::CreateTranslatorCallback callback);

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  virtual void CanTranslate(
      const url::Origin& origin,
      const std::string& source_lang,
      const std::string& target_lang,
      OnDeviceTranslationController::CanTranslateCallback callback);

  bool IsServiceRunning(const url::Origin& origin) const;

  // Sets the service idle timeout for testing. This must be called before the
  // service is started.
  void SetServiceIdleTimeoutForTesting(const url::Origin& origin,
                                       base::TimeDelta service_idle_timeout);

 private:
  // It can also return a nullptr in case we cannot add a new controller.
  OnDeviceTranslationController* GetOrCreateController(
      const url::Origin& origin);

  // This LRU cache maintains at most K service controllers in it. We assume
  // that all of them have services running. Whenever a new controller needs to
  // be created, the cache will remove the least used one. To be able to do
  // this, we do not expose the controller directly, rather we expose its
  // methods `CreateTranslator` and `CanTranslate` here.
  base::LRUCache<url::Origin, std::unique_ptr<OnDeviceTranslationController>>
      service_controllers_;
  // Safe because BrowserProcess::local_state() outlives the Profile.
  raw_ptr<PrefService> local_state_;
  base::WeakPtrFactory<ServiceControllerManager> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_
