// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_TRANSLATOR_H_
#define CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_TRANSLATOR_H_

#include "chrome/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "chrome/services/on_device_translation/public/mojom/translator.mojom.h"
#include "chrome/services/on_device_translation/translate_kit_wrapper.h"

namespace on_device_translation {

using TranslateFunc = base::RepeatingCallback<std::string(const std::string&)>;

// The class provides translation functionalities based on TranslateKit.
class TranslateKitTranslator : public mojom::Translator {
 public:
  TranslateKitTranslator(const std::string& source_lang,
                         const std::string& target_lang);
  ~TranslateKitTranslator() override;

  TranslateKitTranslator(const TranslateKitTranslator&) = delete;
  TranslateKitTranslator& operator=(const TranslateKitTranslator&) = delete;

  // Checks if the translator that translates text from `source_lang` to
  // `target_lang` can be created.
  static void CanTranslate(
      const std::string& source_lang,
      const std::string& target_lang,
      on_device_translation::mojom::OnDeviceTranslationService::
          CanTranslateCallback can_create_callback);

  // Creates a new Translator instance and binds it with the `receiver`. The
  // boolean result indicating if the creation succeeds is passed back through
  // the `callback`.
  static void Create(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
      on_device_translation::mojom::OnDeviceTranslationService::
          CreateTranslatorCallback create_translator_callback);

  // mojom::Translator overrides.
  void Translate(const std::string& input,
                 TranslateCallback translate_callback) override;

 private:
  static void CanTranslateAfterGettingTranslateKitWrapper(
      const std::string& source_lang,
      const std::string& target_lang,
      base::OnceCallback<void(bool)> can_create_callback,
      TranslateKitWrapper* wrapper);

  static void CreateTranslatorAfterGettingTranslateKitWrapper(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<on_device_translation::mojom::Translator> receiver,
      base::OnceCallback<void(bool)> create_translator_callback,
      TranslateKitWrapper* wrapper);

  std::string source_lang_;
  std::string target_lang_;
  TranslateFunc translate_func_;
};

}  // namespace on_device_translation

#endif  // CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_TRANSLATOR_H_
