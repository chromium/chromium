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
  // The file path buffer.
  // Not owned.
  const char* buffer;
  // The size of the file path buffer.
  size_t buffer_size;
} TranslateKitPath;

}  // extern "C"

#endif  // CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_STRUCTS_H_
