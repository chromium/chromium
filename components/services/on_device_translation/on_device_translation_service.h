// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_
#define COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_

#include <memory>

#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace on_device_translation {

class OnDeviceTranslationService : public mojom::OnDeviceTranslationService {
 public:
  explicit OnDeviceTranslationService(
      mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver);

  ~OnDeviceTranslationService() override;

  OnDeviceTranslationService(const OnDeviceTranslationService&) = delete;
  OnDeviceTranslationService& operator=(const OnDeviceTranslationService&) =
      delete;

  // `mojom::OnDeviceTranslationService` overrides:
  void SetServiceConfig(
      mojom::OnDeviceTranslationServiceConfigPtr config) override;
  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
      CreateTranslatorCallback create_translator_callback) override;
  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    CanTranslateCallback can_translate_callback) override;

 private:
  mojo::Receiver<mojom::OnDeviceTranslationService> receiver_;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_
