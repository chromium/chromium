// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/on_device_translation_service.h"

#include <memory>

#include "base/types/pass_key.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/public/mojom/translator.mojom.h"
#include "components/services/on_device_translation/translate_kit_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace on_device_translation {

// TranslateKitTranslator provides translation functionalities based on the
// Translator from TranslateKitClient.
class TranslateKitTranslator : public mojom::Translator {
 public:
  explicit TranslateKitTranslator(TranslateKitClient::Translator* translator)
      : translator_(translator) {
    CHECK(translator_);
  }
  ~TranslateKitTranslator() override = default;
  // Not copyable.
  TranslateKitTranslator(const TranslateKitTranslator&) = delete;
  TranslateKitTranslator& operator=(const TranslateKitTranslator&) = delete;

  // `mojom::Translator` overrides:
  void Translate(const std::string& input,
                 TranslateCallback translate_callback) override {
    CHECK(translator_);
    std::move(translate_callback).Run(translator_->Translate(input));
  }

 private:
  // Owned by the TranslateKitClient managed by OnDeviceTranslationService.
  // The lifetime must be longer than this instance.
  raw_ptr<TranslateKitClient::Translator> translator_;
};

// static
std::unique_ptr<OnDeviceTranslationService>
OnDeviceTranslationService::CreateForTesting(
    mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver,
    std::unique_ptr<TranslateKitClient> client) {
  return std::make_unique<OnDeviceTranslationService>(
      std::move(receiver), std::move(client),
      base::PassKey<OnDeviceTranslationService>());
}

OnDeviceTranslationService::OnDeviceTranslationService(
    mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver)
    : receiver_(this, std::move(receiver)),
      client_(TranslateKitClient::Get()) {}

OnDeviceTranslationService::OnDeviceTranslationService(
    mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver,
    std::unique_ptr<TranslateKitClient> client,
    base::PassKey<OnDeviceTranslationService>)
    : receiver_(this, std::move(receiver)),
      owning_client_for_testing_(std::move(client)),
      client_(owning_client_for_testing_.get()) {}

OnDeviceTranslationService::~OnDeviceTranslationService() = default;

void OnDeviceTranslationService::SetServiceConfig(
    mojom::OnDeviceTranslationServiceConfigPtr config) {
  client_->SetConfig(std::move(config));
}

void OnDeviceTranslationService::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    CreateTranslatorCallback create_translator_callback) {
  auto* translator = client_->GetTranslator(source_lang, target_lang);
  if (!translator) {
    std::move(create_translator_callback).Run(mojo::NullRemote());
    return;
  }
  mojo::PendingRemote<mojom::Translator> pending_remote;
  translators_.Add(std::make_unique<TranslateKitTranslator>(translator),
                   pending_remote.InitWithNewPipeAndPassReceiver());
  std::move(create_translator_callback).Run(std::move(pending_remote));
}

void OnDeviceTranslationService::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    CanTranslateCallback can_translate_callback) {
  std::move(can_translate_callback)
      .Run(client_->CanTranslate(source_lang, target_lang));
}

}  // namespace on_device_translation
