// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller_manager.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/service_controller.h"

namespace on_device_translation {

ServiceControllerManager::ServiceControllerManager(
    PrefService* local_state,
    LauncherFactory launcher_factory)
    : max_service_count_(
          std::max<size_t>(1, kTranslationAPIMaxServiceCount.Get())),
      service_controllers_(ServiceControllerMap::NO_AUTO_EVICT),
      launcher_factory_(std::move(launcher_factory)),
      local_state_(local_state) {}

ServiceControllerManager::~ServiceControllerManager() = default;

OnDeviceTranslationController* ServiceControllerManager::GetOrCreateController(
    const url::Origin& origin) {
  auto it = service_controllers_.Get(origin);
  if (it != service_controllers_.end()) {
    return it->second.get();
  }

  OnDeviceTranslationInstaller* installer =
      installer_for_test_ ? installer_for_test_.get()
                          : OnDeviceTranslationInstaller::GetInstance();
  if (installer == nullptr) {
    return nullptr;
  }

  // At maximum capacity, make room by removing the least recently used
  // controller that is idle: no request waits in it, and its service isn't
  // running. If no controller is idle, we cannot add a new service. Checking
  // only the least recently used controller would refuse a new origin while
  // another controller sits idle.
  CHECK_LE(service_controllers_.size(), max_service_count_);
  if (service_controllers_.size() == max_service_count_) {
    auto idle_it = std::ranges::find_if(
        service_controllers_.rbegin(), service_controllers_.rend(),
        [](const auto& entry) { return entry.second->IsIdle(); });
    if (idle_it == service_controllers_.rend()) {
      return nullptr;
    }
    service_controllers_.Erase(idle_it);
  }

  auto service_controller =
      std::make_unique<OnDeviceTranslationServiceController>(
          launcher_factory_.Run(), origin.Serialize(), installer);
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

  auto* controller = GetOrCreateController(origin);
  if (controller) {
    static_cast<OnDeviceTranslationServiceController*>(controller)
        ->SetServiceIdleTimeoutForTesting(service_idle_timeout);  // IN-TEST
  }
}

void ServiceControllerManager::SetInstallerForTesting(  // IN-TEST
    OnDeviceTranslationInstaller* installer) {
  CHECK_IS_TEST();
  installer_for_test_ = installer;
}

}  // namespace on_device_translation
