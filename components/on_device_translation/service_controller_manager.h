// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
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

  virtual scoped_refptr<OnDeviceTranslationServiceController>
  GetServiceControllerForOrigin(const url::Origin& origin);

  // Returns true if a new service can be started.
  virtual bool CanStartNewService() const;

  // Called when a service controller is deleted.
  virtual void OnServiceControllerDeleted(
      const url::Origin& origin,
      base::PassKey<OnDeviceTranslationServiceController>);

  // Sets the service controller deleted observer for testing.
  void set_service_controller_deleted_observer_for_testing(
      base::OnceClosure observer) {
    service_controller_deleted_observer_for_testing_ = std::move(observer);
  }

 private:
  std::map<url::Origin, raw_ptr<OnDeviceTranslationServiceController>>
      service_controllers_;
  base::OnceClosure service_controller_deleted_observer_for_testing_;
  // Safe because BrowserProcess::local_state() outlives the Profile.
  raw_ptr<PrefService> local_state_;
};

}  // namespace on_device_translation
#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_H_
