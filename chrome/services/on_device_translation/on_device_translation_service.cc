// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/on_device_translation/on_device_translation_service.h"

#include <memory>

#include "chrome/services/on_device_translation/mock_translator.h"
#include "chrome/services/on_device_translation/public/cpp/features.h"
#include "chrome/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "chrome/services/on_device_translation/translate_kit_translator.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace on_device_translation {

OnDeviceTranslationService::OnDeviceTranslationService(
    mojo::PendingReceiver<mojom::OnDeviceTranslationService> receiver)
    : receiver_(this, std::move(receiver)) {}

OnDeviceTranslationService::~OnDeviceTranslationService() = default;

void CreateTranslatorAfterCheckingCanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    OnDeviceTranslationService::CreateTranslatorCallback
        create_translator_callback,
    bool can_translate) {
  if (!can_translate) {
    std::move(create_translator_callback).Run(false);
    return;
  }

#if BUILDFLAG(IS_WIN)
  MockTranslator::Create(source_lang, target_lang, std::move(receiver),
                         std::move(create_translator_callback));
#else
  if (base::FeatureList::IsEnabled(kUseTranslateKitForTranslationAPI)) {
    TranslateKitTranslator::Create(source_lang, target_lang,
                                   std::move(receiver),
                                   std::move(create_translator_callback));
  } else {
    MockTranslator::Create(source_lang, target_lang, std::move(receiver),
                           std::move(create_translator_callback));
  }
#endif  // BUILDFLAG(IS_WIN)
}

void OnDeviceTranslationService::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    CreateTranslatorCallback create_translator_callback) {
  CanTranslate(source_lang, target_lang,
               base::BindOnce(CreateTranslatorAfterCheckingCanTranslate,
                              source_lang, target_lang, std::move(receiver),
                              std::move(create_translator_callback)));
}

void OnDeviceTranslationService::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    CanTranslateCallback can_translate_callback) {
#if BUILDFLAG(IS_WIN)
  MockTranslator::CanTranslate(source_lang, target_lang,
                               std::move(can_translate_callback));
#else
  if (base::FeatureList::IsEnabled(kUseTranslateKitForTranslationAPI)) {
    TranslateKitTranslator::CanTranslate(source_lang, target_lang,
                                         std::move(can_translate_callback));
  } else {
    MockTranslator::CanTranslate(source_lang, target_lang,
                                 std::move(can_translate_callback));
  }
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace on_device_translation
