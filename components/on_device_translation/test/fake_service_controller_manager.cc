// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/test/fake_service_controller_manager.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/service_controller.h"

namespace on_device_translation {

FakeServiceControllerManager::FakeServiceControllerManager(
    PrefService* local_state,
    LauncherFactory launcher_factory)
    : ServiceControllerManager(local_state, std::move(launcher_factory)),
      local_state_(local_state) {}

FakeServiceControllerManager::~FakeServiceControllerManager() = default;
void FakeServiceControllerManager::CreateTranslator(
    const url::Origin& origin,
    const std::string& source_lang,
    const std::string& target_lang,
    OnDeviceTranslationController::CreateTranslatorCallback callback) {
  service_controllers_.at(origin)->CreateTranslator(source_lang, target_lang,
                                                    std::move(callback));
}

void FakeServiceControllerManager::CanTranslate(
    const url::Origin& origin,
    const std::string& source_lang,
    const std::string& target_lang,
    OnDeviceTranslationController::CanTranslateCallback callback) {
  service_controllers_.at(origin)->CanTranslate(source_lang, target_lang,
                                                std::move(callback));
}

size_t FakeServiceControllerManager::GetControllerCount() const {
  return service_controllers_.size();
}

void FakeServiceControllerManager::SetServiceControllerForTest(
    const url::Origin& origin,
    std::unique_ptr<OnDeviceTranslationController> service_controller) {
  service_controllers_[origin] = std::move(service_controller);
}

}  // namespace on_device_translation
