// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_STRUCTS_H_
#define CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_STRUCTS_H_

#include <cstddef>

// WARNING:
// This below section contains the interface contract between Chrome and the
// TranslateKit binary. Any changes to these interfaces must be backwards
// compatible since the TranslateKit binary on the device may be older or newer
// than the Chrome version.

extern "C" {

typedef struct {
  // The language code.
  // Not owned.
  const char* language_code;
  // The size of the language code string.
  size_t language_code_size;
} TranslateKitLanguage;

typedef struct {
  // The input text buffer.
  // Not owned.
  const char* input_text;
  // The size of the input text buffer.
  size_t input_text_size;
} TranslateKitInputText;

typedef struct {
  // The modifiable output text buffer.
  // Not owned.
  char* buffer;
  // The size of the output text buffer.
  size_t buffer_size;
} TranslateKitOutputText;

typedef struct {
  // A chrome::on_device_translation::TranslateKitConfig serialized as a string.
  // Not owned.
  const char* translate_kit_config;
  // The size of `translate_kit_config`.
  size_t translate_kit_config_size;
  // The source language the initialized translator to translate from.
  TranslateKitLanguage source_lang;
  // The target language the initialized translator to translate to.
  TranslateKitLanguage target_lang;
} TranslateKitTranslatorConfig;

}  // extern "C"

#endif  // CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_STRUCTS_H_
