// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/on_device_translation/on_device_translation_service.h"

#include <memory>

#include "base/command_line.h"
#include "chrome/services/on_device_translation/mock_translator.h"
#include "chrome/services/on_device_translation/public/cpp/features.h"
#include "chrome/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "chrome/services/on_device_translation/translate_kit_client.h"
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
    auto result = translator_->Translate(input);
    std::move(translate_callback).Run(result ? *result : std::string());
  }

 private:
  // Owned by the TranslateKitClient managed by OnDeviceTranslationService.
  // The lifetime must be longer than this instance.
  raw_ptr<TranslateKitClient::Translator> translator_;
};

OnDeviceTranslationService::OnDeviceTranslationService(
    mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver)
    : receiver_(this, std::move(receiver)) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTranslateKitPath)) {
    auto translate_kit_path =
        command_line->GetSwitchValuePath(kTranslateKitPath);
    translate_kit_ = std::make_unique<TranslateKitClient>(translate_kit_path);
  } else {
    LOG(ERROR)
        << "Failed to initialize TranslateKitClient due to missing path.";
  }
}

OnDeviceTranslationService::~OnDeviceTranslationService() = default;

void OnDeviceTranslationService::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    CreateTranslatorCallback create_translator_callback) {
  if (!base::FeatureList::IsEnabled(kEnableTranslateKitComponent)) {
    MockTranslator::Create(source_lang, target_lang, std::move(receiver),
                           std::move(create_translator_callback));
    return;
  }

  auto* translator = translate_kit_->GetTranslator(source_lang, target_lang);
  if (!translator) {
    std::move(create_translator_callback).Run(false);
    return;
  }
  auto translator_kit_translator =
      std::make_unique<TranslateKitTranslator>(translator);
  mojo::MakeSelfOwnedReceiver(std::move(translator_kit_translator),
                              std::move(receiver));
  std::move(create_translator_callback).Run(true);
}

void OnDeviceTranslationService::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    CanTranslateCallback can_translate_callback) {
  if (!base::FeatureList::IsEnabled(kEnableTranslateKitComponent)) {
    MockTranslator::CanTranslate(source_lang, target_lang,
                                 std::move(can_translate_callback));
    return;
  }

  bool can_translate = false;
  if (translate_kit_) {
    can_translate = translate_kit_->CanTranslate(source_lang, target_lang);
  }
  std::move(can_translate_callback).Run(can_translate);
}

}  // namespace on_device_translation
