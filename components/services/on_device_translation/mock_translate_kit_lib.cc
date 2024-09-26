// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "components/services/on_device_translation/translate_kit_structs.h"

// This file implements the ABI of libtranslatekit only for testing.

#if BUILDFLAG(IS_WIN)
#define TRANSLATE_KIT_EXPORT __declspec(dllexport)
#else
#define TRANSLATE_KIT_EXPORT __attribute__((visibility("default")))
#endif

namespace {

constexpr uintptr_t kMockTranslateKitPtr = 0xDEADBEAF;
constexpr uintptr_t kMockTranslatorPtr = 0xDEADBEEF;

bool IsIsSameLanguage(TranslateKitLanguage source_lang,
                      TranslateKitLanguage target_lang) {
  return std::string_view(source_lang.language_code,
                          source_lang.language_code_size) ==
         std::string_view(target_lang.language_code,
                          target_lang.language_code_size);
}
}  // namespace

extern "C" {

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

TRANSLATE_KIT_EXPORT void InitializeStorageBackend(
    FileExistsFn file_exists,
    OpenForReadOnlyMemoryMapFn open_for_read_only_memory_map,
    DeleteReadOnlyMemoryRegionFn delete_read_only_memory_region,
    ReadOnlyMemoryRegionDataFn read_only_memory_region_data,
    ReadOnlyMemoryRegionLengthFn read_only_memory_region_length,
    std::uintptr_t user_data) {}
TRANSLATE_KIT_EXPORT uintptr_t CreateTranslateKit() {
  return static_cast<uintptr_t>(kMockTranslateKitPtr);
}
TRANSLATE_KIT_EXPORT void DeleteTranslateKit(uintptr_t kit_ptr) {}
TRANSLATE_KIT_EXPORT bool TranslateKitSetLanguagePackages(
    uintptr_t kit_ptr,
    TranslateKitSetLanguagePackagesArgs args) {
  return true;
}
TRANSLATE_KIT_EXPORT uintptr_t
TranslateKitCreateTranslator(uintptr_t kit_ptr,
                             TranslateKitLanguage source_lang,
                             TranslateKitLanguage target_lang) {
  // Only supports the same language translation (?).
  if (IsIsSameLanguage(source_lang, target_lang)) {
    return static_cast<uintptr_t>(kMockTranslatorPtr);
  } else {
    return static_cast<uintptr_t>(0);
  }
}
TRANSLATE_KIT_EXPORT void DeleteTranslator(uintptr_t translator_ptr) {}
typedef void (*TranslateCallbackFn)(TranslateKitOutputText, std::uintptr_t);
DISABLE_CFI_DLSYM TRANSLATE_KIT_EXPORT bool TranslatorTranslate(
    uintptr_t translator_ptr,
    TranslateKitInputText input,
    TranslateCallbackFn callback,
    std::uintptr_t user_data) {
  // Just pass the input text.
  callback(TranslateKitOutputText(input.input_text, input.input_text_size),
           user_data);
  return true;
}
}
