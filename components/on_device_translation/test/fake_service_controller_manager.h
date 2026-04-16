// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_SERVICE_CONTROLLER_MANAGER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_SERVICE_CONTROLLER_MANAGER_H_

#include <map>

#include "base/memory/scoped_refptr.h"
#include "components/on_device_translation/service_controller_manager.h"
#include "url/origin.h"

class PrefService;

namespace on_device_translation {

class FakeServiceControllerManager : public ServiceControllerManager {
 public:
  explicit FakeServiceControllerManager(PrefService* local_state,
                                        LauncherFactory launcher_factory = {});
  ~FakeServiceControllerManager() override;

  FakeServiceControllerManager(const FakeServiceControllerManager&) = delete;
  FakeServiceControllerManager& operator=(const FakeServiceControllerManager&) =
      delete;
  // Creates a translator class that implements `mojom::Translator` for the
  // given language pair.
  void CreateTranslator(const url::Origin& origin,
                        const std::string& source_lang,
                        const std::string& target_lang,
                        OnDeviceTranslationController::CreateTranslatorCallback
                            callback) override;

  // Checks if the translate service can do translation from `source_lang` to
  // `target_lang`.
  void CanTranslate(
      const url::Origin& origin,
      const std::string& source_lang,
      const std::string& target_lang,
      OnDeviceTranslationController::CanTranslateCallback callback) override;

  // Test-only methods:
  size_t GetControllerCount() const;
  void SetServiceControllerForTest(
      const url::Origin& origin,
      std::unique_ptr<OnDeviceTranslationController> service_controller);

 private:
  void OnServiceControllerDeleted(const url::Origin& origin);

  raw_ptr<PrefService> local_state_;
  std::map<url::Origin, std::unique_ptr<OnDeviceTranslationController>>
      service_controllers_;
  base::WeakPtrFactory<FakeServiceControllerManager> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_SERVICE_CONTROLLER_MANAGER_H_
