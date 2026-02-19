// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/test/fake_service_controller_manager.h"

#include "base/memory/scoped_refptr.h"
#include "components/on_device_translation/service_controller.h"

namespace on_device_translation {

FakeServiceControllerManager::FakeServiceControllerManager(
    PrefService* local_state)
    : ServiceControllerManager(local_state), local_state_(local_state) {}

FakeServiceControllerManager::~FakeServiceControllerManager() = default;

scoped_refptr<OnDeviceTranslationServiceController>
FakeServiceControllerManager::GetServiceControllerForOrigin(
    const url::Origin& origin) {
  auto it = service_controllers_.find(origin);
  if (it != service_controllers_.end()) {
    return it->second;
  }

  if (!service_controllers_.empty()) {
    return service_controllers_.begin()->second;
  }

  auto service_controller =
      base::MakeRefCounted<OnDeviceTranslationServiceController>(local_state_,
                                                                 this, origin);
  service_controllers_[origin] = service_controller;
  return service_controller;
}

bool FakeServiceControllerManager::CanStartNewService() const {
  return can_start_new_service_;
}

void FakeServiceControllerManager::OnServiceControllerDeleted(
    const url::Origin& origin,
    base::PassKey<OnDeviceTranslationServiceController>) {
  service_controllers_.erase(origin);
}

void FakeServiceControllerManager::SetCanStartNewService(bool can_start) {
  can_start_new_service_ = can_start;
}

size_t FakeServiceControllerManager::GetControllerCount() const {
  return service_controllers_.size();
}

void FakeServiceControllerManager::SetServiceControllerForTest(
    const url::Origin& origin,
    scoped_refptr<OnDeviceTranslationServiceController> service_controller) {
  service_controllers_[origin] = service_controller;
}

}  // namespace on_device_translation
