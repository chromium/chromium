// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_SERVICE_LAUNCHER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_SERVICE_LAUNCHER_H_

#include <memory>
#include <string_view>

#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace on_device_translation {

// Interface for launching the on-device translation service.
class OnDeviceTranslationServiceLauncher {
 public:
  virtual ~OnDeviceTranslationServiceLauncher() = default;

  // Launches the on-device translation service.
  virtual mojo::PendingRemote<mojom::OnDeviceTranslationService> Launch(
      std::string_view service_display_name_suffix) = 0;
};

// Creates a new instance of OnDeviceTranslationServiceLauncher.
std::unique_ptr<OnDeviceTranslationServiceLauncher>
CreateOnDeviceTranslationServiceLauncher();

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_SERVICE_LAUNCHER_H_
