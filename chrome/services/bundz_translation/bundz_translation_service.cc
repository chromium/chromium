// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/bundz_translation/bundz_translation_service.h"

#include <memory>

#include "chrome/services/bundz_translation/mock_translator.h"
#include "chrome/services/bundz_translation/public/mojom/bundz_translation_service.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace bundz_translation {

BundzTranslationService::BundzTranslationService(
    mojo::PendingReceiver<mojom::BundzTranslationService> receiver)
    : receiver_(this, std::move(receiver)) {}

BundzTranslationService::~BundzTranslationService() = default;

void BundzTranslationService::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<bundz_translation::mojom::Translator> receiver,
    CreateTranslatorCallback callback) {
  if (!MockTranslator::CanTranslate(source_lang, target_lang)) {
    std::move(callback).Run(false);
    return;
  }
  mojo::MakeSelfOwnedReceiver(std::make_unique<MockTranslator>(),
                              std::move(receiver));
  std::move(callback).Run(true);
}

void BundzTranslationService::CanTranslate(const std::string& source_lang,
                                           const std::string& target_lang,
                                           CanTranslateCallback callback) {
  std::move(callback).Run(
      MockTranslator::CanTranslate(source_lang, target_lang));
}

}  // namespace bundz_translation
