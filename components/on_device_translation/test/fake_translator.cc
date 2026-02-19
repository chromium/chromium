// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/test/fake_translator.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/strings/string_split.h"

namespace on_device_translation {

FakeTranslator::FakeTranslator(
    mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeTranslator::~FakeTranslator() = default;

void FakeTranslator::Translate(const std::string& input,
                               TranslateCallback callback) {
  // Simple fake translation logic for testing.
  if (input == "Hello world") {
    std::move(callback).Run("Hola mundo");
    return;
  }
  std::move(callback).Run("Translated: " + input);
}

void FakeTranslator::SplitSentences(const std::string& input,
                                    SplitSentencesCallback callback) {
  // Simple fake sentence splitting for testing.
  std::vector<std::string> sentences = base::SplitString(
      input, ".?!", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::move(callback).Run(sentences);
}

}  // namespace on_device_translation
