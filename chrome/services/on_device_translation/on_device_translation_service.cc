// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/on_device_translation/on_device_translation_service.h"

#include <memory>

#include "chrome/services/on_device_translation/mock_translator.h"
#include "chrome/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace on_device_translation {

OnDeviceTranslationService::OnDeviceTranslationService(
    mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver)
    : receiver_(this, std::move(receiver)) {}

OnDeviceTranslationService::~OnDeviceTranslationService() = default;

void OnDeviceTranslationService::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    CreateTranslatorCallback callback) {
  if (!MockTranslator::CanTranslate(source_lang, target_lang)) {
    std::move(callback).Run(false);
    return;
  }
  mojo::MakeSelfOwnedReceiver(std::make_unique<MockTranslator>(),
                              std::move(receiver));
  std::move(callback).Run(true);
}

void OnDeviceTranslationService::CanTranslate(const std::string& source_lang,
                                              const std::string& target_lang,
                                              CanTranslateCallback callback) {
  std::move(callback).Run(
      MockTranslator::CanTranslate(source_lang, target_lang));
}

}  // namespace on_device_translation
