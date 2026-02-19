// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_SERVICE_CONTROLLER_MANAGER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_SERVICE_CONTROLLER_MANAGER_H_

#include <map>

#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "components/on_device_translation/service_controller_manager.h"
#include "url/origin.h"

class PrefService;

namespace on_device_translation {

class OnDeviceTranslationServiceController;

class FakeServiceControllerManager : public ServiceControllerManager {
 public:
  explicit FakeServiceControllerManager(PrefService* local_state);
  ~FakeServiceControllerManager() override;

  FakeServiceControllerManager(const FakeServiceControllerManager&) = delete;
  FakeServiceControllerManager& operator=(const FakeServiceControllerManager&) =
      delete;

  // ServiceControllerManager:
  scoped_refptr<OnDeviceTranslationServiceController>
  GetServiceControllerForOrigin(const url::Origin& origin) override;
  bool CanStartNewService() const override;
  void OnServiceControllerDeleted(
      const url::Origin& origin,
      base::PassKey<OnDeviceTranslationServiceController>) override;

  // Test-only methods:
  void SetCanStartNewService(bool can_start);
  size_t GetControllerCount() const;
  void SetServiceControllerForTest(
      const url::Origin& origin,
      scoped_refptr<OnDeviceTranslationServiceController> service_controller);

 private:
  raw_ptr<PrefService> local_state_;
  bool can_start_new_service_ = true;
  std::map<url::Origin, scoped_refptr<OnDeviceTranslationServiceController>>
      service_controllers_;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_SERVICE_CONTROLLER_MANAGER_H_
