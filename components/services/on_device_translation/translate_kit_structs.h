// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_STRUCTS_H_
#define COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_STRUCTS_H_

#include <cstddef>
#include <cstdint>

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
  // The output text buffer.
  // Not owned.
  const char* buffer;
  // The size of the output text buffer.
  size_t buffer_size;
} TranslateKitOutputText;

typedef struct {
  // A chrome::on_device_translation::TranslateKitLanguagePackageConfig
  // serialized as a string.
  // Not owned.
  const char* package_config;
  // The size of `package_config`.
  size_t package_config_size;
} TranslateKitSetLanguagePackagesArgs;

typedef bool (*FileExistsFn)(const char* file_name,
                             size_t file_name_size,
                             bool* is_directory,
                             std::uintptr_t user_data);
typedef std::uintptr_t (*OpenForReadOnlyMemoryMapFn)(const char* file_name,
                                                     size_t file_name_size,
                                                     std::uintptr_t user_data);
typedef void (*DeleteReadOnlyMemoryRegionFn)(std::uintptr_t memory_map_ptr,
                                             std::uintptr_t user_data);
typedef const void* (*ReadOnlyMemoryRegionDataFn)(std::uintptr_t memory_map_ptr,
                                                  std::uintptr_t user_data);
typedef uint64_t (*ReadOnlyMemoryRegionLengthFn)(std::uintptr_t memory_map_ptr,
                                                 std::uintptr_t user_data);

}  // extern "C"

#endif  // COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_STRUCTS_H_
