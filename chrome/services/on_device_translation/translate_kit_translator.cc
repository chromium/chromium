// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/on_device_translation/translate_kit_translator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/services/on_device_translation/public/mojom/translator.mojom.h"
#include "chrome/services/on_device_translation/translate_kit_wrapper.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace on_device_translation {

TranslateKitTranslator::TranslateKitTranslator(const std::string& source_lang,
                                               const std::string& target_lang)
    : source_lang_(source_lang), target_lang_(target_lang) {}

TranslateKitTranslator::~TranslateKitTranslator() = default;

// static
void TranslateKitTranslator::CanTranslateAfterGettingTranslateKitWrapper(
    const std::string& source_lang,
    const std::string& target_lang,
    base::OnceCallback<void(bool)> can_create_callback,
    TranslateKitWrapper* wrapper) {
  if (!wrapper) {
    std::move(can_create_callback).Run(false);
    return;
  }
  std::move(can_create_callback)
      .Run(wrapper->CanTranslate(source_lang, target_lang));
}

// static
void TranslateKitTranslator::CanTranslate(
    const std::string& source_lang,
    const std::string& target_lang,
    on_device_translation::mojom::OnDeviceTranslationService::
        CanTranslateCallback can_create_callback) {
  TranslateKitWrapper::GetInstance(base::BindOnce(
      TranslateKitTranslator::CanTranslateAfterGettingTranslateKitWrapper,
      source_lang, target_lang, std::move(can_create_callback)));
}

// static
void TranslateKitTranslator::CreateTranslatorAfterGettingTranslateKitWrapper(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    base::OnceCallback<void(bool)> create_translator_callback,
    TranslateKitWrapper* wrapper) {
  if (!wrapper) {
    std::move(create_translator_callback).Run(false);
    return;
  }

  std::optional<TranslateFunc> translate_func =
      wrapper->GetTranslateFunc(source_lang, target_lang);
  if (!translate_func) {
    std::move(create_translator_callback).Run(false);
    return;
  }

  auto translator =
      std::make_unique<TranslateKitTranslator>(source_lang, target_lang);
  translator->translate_func_ = translate_func.value();
  mojo::MakeSelfOwnedReceiver(std::move(translator), std::move(receiver));
  std::move(create_translator_callback).Run(true);
}

// static
void TranslateKitTranslator::Create(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
    on_device_translation::mojom::OnDeviceTranslationService::
        CreateTranslatorCallback create_translator_callback) {
  TranslateKitWrapper::GetInstance(base::BindOnce(
      TranslateKitTranslator::CreateTranslatorAfterGettingTranslateKitWrapper,
      source_lang, target_lang, std::move(receiver),
      std::move(create_translator_callback)));
}

void TranslateKitTranslator::Translate(const std::string& input,
                                       TranslateCallback translate_callback) {
  std::move(translate_callback).Run(translate_func_.Run(input));
}

}  // namespace on_device_translation
