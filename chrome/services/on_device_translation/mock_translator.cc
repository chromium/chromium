// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/on_device_translation/mock_translator.h"

#include <memory>

#include "chrome/services/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace on_device_translation {

MockTranslator::MockTranslator() = default;

MockTranslator::~MockTranslator() = default;

// static
void MockTranslator::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    on_device_translation::mojom::OnDeviceTranslationService::
        CanTranslateCallback can_translate_callback) {
  std::move(can_translate_callback).Run(source_lang == target_lang);
}

// static
void MockTranslator::Create(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    on_device_translation::mojom::OnDeviceTranslationService::
        CreateTranslatorCallback create_translator_callback) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<MockTranslator>(),
                              std::move(receiver));
  std::move(create_translator_callback).Run(true);
}

void MockTranslator::Translate(const std::string& input,
                               TranslateCallback translate_callback) {
  std::move(translate_callback).Run(input);
}

}  // namespace on_device_translation
