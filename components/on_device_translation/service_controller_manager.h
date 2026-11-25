// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_

#include <cstddef>
#include <map>
#include <memory>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/on_device_translation/service_controller.h"
#include "url/origin.h"

namespace url {
class Origin;
}  // namespace url

class PrefService;

namespace on_device_translation {

class InputBudget;
class OnDeviceTranslationController;
class OnDeviceTranslationServiceLauncher;

// Manages the OnDeviceTranslationServiceControllers for a BrowserContext.
// This class is responsible for creating the per origin
// OnDeviceTranslationServiceController.
class ServiceControllerManager : public KeyedService {
 public:
  using LauncherFactory = base::RepeatingCallback<
      std::unique_ptr<OnDeviceTranslationServiceLauncher>()>;

  ServiceControllerManager(PrefService* local_state,
                           LauncherFactory launcher_factory);
  ~ServiceControllerManager() override;

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

  // Returns the input budget that the translators of `origin` share, or null
  // if `kTranslationAPIMaxPendingInputBytesPerOrigin` turns it off. The budget
  // lives while a translator holds it.
  scoped_refptr<InputBudget> GetInputBudget(const url::Origin& origin);

  // Sets the service idle timeout for testing. This must be called before the
  // service is started.
  void SetServiceIdleTimeoutForTesting(const url::Origin& origin,
                                       base::TimeDelta service_idle_timeout);

  // Sets the installer for testing.
  void SetInstallerForTesting(OnDeviceTranslationInstaller* installer);

 private:
  // It can also return a nullptr in case we cannot add a new controller.
  OnDeviceTranslationController* GetOrCreateController(
      const url::Origin& origin);

  using ServiceControllerMap =
      base::LRUCache<url::Origin,
                     std::unique_ptr<OnDeviceTranslationController>>;

  // Holds at most `max_service_count_` controllers. Uses `NO_AUTO_EVICT` so
  // `LRUCacheMemoryConsumer` cannot shrink the cache and destroy controllers
  // that still have a running service or pending download.
  const size_t max_service_count_;
  ServiceControllerMap service_controllers_;
  LauncherFactory launcher_factory_;
  // Safe because BrowserProcess::local_state() outlives the Profile.
  raw_ptr<PrefService> local_state_;
  raw_ptr<OnDeviceTranslationInstaller> installer_for_test_ = nullptr;
  // The input budget of each origin that has a translator.
  std::map<url::Origin, base::WeakPtr<InputBudget>> input_budgets_;
  base::WeakPtrFactory<ServiceControllerManager> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_
