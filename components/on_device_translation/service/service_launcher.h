// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_SERVICE_LAUNCHER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_SERVICE_LAUNCHER_H_

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace on_device_translation {

class OnDeviceTranslationInstaller;
enum class LanguagePackKey;

// A language pack and the directory of the files that a service reads.
using LanguagePackPath = std::pair<LanguagePackKey, base::FilePath>;

// Interface for launching the on-device translation service.
class OnDeviceTranslationServiceLauncher {
 public:
  virtual ~OnDeviceTranslationServiceLauncher() = default;

  // Launches the on-device translation service, and sends it the installed
  // `language_packs` in that order. The paths must not be empty.
  virtual mojo::PendingRemote<mojom::OnDeviceTranslationService> Launch(
      std::string_view service_display_name_suffix,
      OnDeviceTranslationInstaller* installer,
      const std::vector<LanguagePackPath>& language_packs) = 0;

  // Sends the installed `language_packs` to a `service` that `Launch()`
  // started. The packs that the service already has must keep their position
  // and their path at the start of `language_packs`, because the service
  // refers to the files of a pack by its position, and may have read some of
  // them already. The translators that the service created keep working.
  virtual void UpdateLanguagePacks(
      mojom::OnDeviceTranslationService& service,
      const std::vector<LanguagePackPath>& language_packs) = 0;
};

// Creates a new instance of OnDeviceTranslationServiceLauncher.
std::unique_ptr<OnDeviceTranslationServiceLauncher>
CreateOnDeviceTranslationServiceLauncher();

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_SERVICE_LAUNCHER_H_
