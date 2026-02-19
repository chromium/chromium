// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller_manager.h"

#include "base/memory/scoped_refptr.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/service_controller.h"

namespace on_device_translation {

ServiceControllerManager::ServiceControllerManager(
    PrefService* local_state,
    base::PassKey<ServiceControllerManagerFactory>)
    : local_state_(local_state) {}

ServiceControllerManager::ServiceControllerManager(PrefService* local_state)
    : local_state_(local_state) {}

ServiceControllerManager::~ServiceControllerManager() = default;

scoped_refptr<OnDeviceTranslationServiceController>
ServiceControllerManager::GetServiceControllerForOrigin(
    const url::Origin& origin) {
  auto it = service_controllers_.find(origin);
  if (it != service_controllers_.end()) {
    return it->second.get();
  }
  auto service_controller =
      base::MakeRefCounted<OnDeviceTranslationServiceController>(local_state_,
                                                                 this, origin);
  service_controllers_[origin] = service_controller.get();
  return service_controller;
}

bool ServiceControllerManager::CanStartNewService() const {
  size_t running_service_count = 0;
  const size_t service_count_limit = kTranslationAPIMaxServiceCount.Get();
  for (const auto& pair : service_controllers_) {
    if (pair.second->IsServiceRunning()) {
      ++running_service_count;
      // We can't start a new service if we've reached the limit.
      if (running_service_count == service_count_limit) {
        return false;
      }
    }
  }
  return true;
}

void ServiceControllerManager::OnServiceControllerDeleted(
    const url::Origin& origin,
    base::PassKey<OnDeviceTranslationServiceController>) {
  CHECK_EQ(service_controllers_.erase(origin), 1u);
  if (service_controller_deleted_observer_for_testing_) {
    std::move(service_controller_deleted_observer_for_testing_).Run();
  }
}

}  // namespace on_device_translation
