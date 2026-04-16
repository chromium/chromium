// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller_manager.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/service_controller.h"

namespace on_device_translation {

ServiceControllerManager::ServiceControllerManager(
    PrefService* local_state,
    LauncherFactory launcher_factory,
    base::PassKey<ServiceControllerManagerFactory>)
    : service_controllers_(kTranslationAPIMaxServiceCount.Get()),
      launcher_factory_(std::move(launcher_factory)),
      local_state_(local_state) {
  CHECK_GT(service_controllers_.max_size(), 0u);
}

ServiceControllerManager::ServiceControllerManager(
    PrefService* local_state,
    LauncherFactory launcher_factory)
    : service_controllers_(kTranslationAPIMaxServiceCount.Get()),
      launcher_factory_(std::move(launcher_factory)),
      local_state_(local_state) {
  CHECK_GT(service_controllers_.max_size(), 0u);
}

ServiceControllerManager::~ServiceControllerManager() = default;

OnDeviceTranslationController* ServiceControllerManager::GetOrCreateController(
    const url::Origin& origin) {
  auto it = service_controllers_.Get(origin);
  if (it != service_controllers_.end()) {
    return it->second.get();
  }

  // If we are at maximum capacity and the least-used item is running, we cannot
  // add a new service.
  if (service_controllers_.size() == service_controllers_.max_size() &&
      service_controllers_.rbegin()->second->IsServiceRunning()) {
    return nullptr;
  }

  auto service_controller =
      std::make_unique<OnDeviceTranslationServiceController>(
          launcher_factory_.Run(), origin.Serialize());
  auto it_inserted =
      service_controllers_.Put(origin, std::move(service_controller));
  return it_inserted->second.get();
}

// Creates a translator class that implements `mojom::Translator` for the
// given language pair.
void ServiceControllerManager::CreateTranslator(
    const url::Origin& origin,
    const std::string& source_lang,
    const std::string& target_lang,
    OnDeviceTranslationController::CreateTranslatorCallback callback) {
  OnDeviceTranslationController* controller = GetOrCreateController(origin);
  if (controller == nullptr) {
    std::move(callback).Run(
        base::unexpected(OnDeviceTranslationController::CreateTranslatorError::
                             kExceedsServiceCountLimitation));
    return;
  }
  controller->CreateTranslator(source_lang, target_lang, std::move(callback));
}

// Checks if the translate service can do translation from `source_lang` to
// `target_lang`.
void ServiceControllerManager::CanTranslate(
    const url::Origin& origin,
    const std::string& source_lang,
    const std::string& target_lang,
    OnDeviceTranslationController::CanTranslateCallback callback) {
  auto* controller = GetOrCreateController(origin);
  if (controller == nullptr) {
    std::move(callback).Run(OnDeviceTranslationController::CanTranslateResult::
                                kNoExceedsServiceCountLimitation);
    return;
  }

  controller->CanTranslate(source_lang, target_lang, std::move(callback));
}

bool ServiceControllerManager::IsServiceRunning(
    const url::Origin& origin) const {
  auto it = service_controllers_.Peek(origin);
  if (it == service_controllers_.end()) {
    return false;
  }

  return it->second->IsServiceRunning();
}

// Sets the service idle timeout for testing. This must be called before the
// service is started.
void ServiceControllerManager::SetServiceIdleTimeoutForTesting(
    const url::Origin& origin,
    base::TimeDelta service_idle_timeout) {
  CHECK_IS_TEST();

  static_cast<OnDeviceTranslationServiceController*>(
      GetOrCreateController(origin))
      ->SetServiceIdleTimeoutForTesting(service_idle_timeout);
}

}  // namespace on_device_translation
