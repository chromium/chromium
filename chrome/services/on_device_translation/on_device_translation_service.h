// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_
#define CHROME_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_

#include "chrome/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
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

  // mojom::OnDeviceTranslationService overrides.
  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
      CreateTranslatorCallback callback) override;

  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    CanTranslateCallback callback) override;

 private:
  mojo::Receiver<mojom::OnDeviceTranslationService> receiver_;
};

}  // namespace on_device_translation

#endif  // CHROME_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_
