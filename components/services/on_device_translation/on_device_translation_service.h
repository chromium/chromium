// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_
#define COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace on_device_translation {

class TranslateKitClient;

class OnDeviceTranslationService : public mojom::OnDeviceTranslationService {
 public:
  static std::unique_ptr<OnDeviceTranslationService> CreateForTesting(
      mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver,
      std::unique_ptr<TranslateKitClient> client);

  explicit OnDeviceTranslationService(
      mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver);
  OnDeviceTranslationService(
      mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver,
      std::unique_ptr<TranslateKitClient> client,
      base::PassKey<OnDeviceTranslationService>);

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
      CreateTranslatorCallback create_translator_callback) override;
  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    CanTranslateCallback can_translate_callback) override;

 private:
  mojo::Receiver<mojom::OnDeviceTranslationService> receiver_;
  std::unique_ptr<TranslateKitClient> owning_client_for_testing_;
  raw_ptr<TranslateKitClient> client_;

  mojo::UniqueReceiverSet<on_device_translation::mojom::Translator>
      translators_;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_ON_DEVICE_TRANSLATION_SERVICE_H_
