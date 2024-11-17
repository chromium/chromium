// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the ABI of libtranslatekit only for testing. It is not
// used in production.
// This file's CreateTranslateKit() method returns 0 to test the behavior of the
// TranslateKit creation failure on the client side.

#include <cstddef>
#include <cstdint>

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/services/on_device_translation/translate_kit_structs.h"

#if BUILDFLAG(IS_WIN)
#define TRANSLATE_KIT_EXPORT __declspec(dllexport)
#else
#define TRANSLATE_KIT_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

TRANSLATE_KIT_EXPORT void InitializeStorageBackend(
    FileExistsFn file_exists,
    OpenForReadOnlyMemoryMapFn open_for_read_only_memory_map,
    DeleteReadOnlyMemoryRegionFn delete_read_only_memory_region,
    ReadOnlyMemoryRegionDataFn read_only_memory_region_data,
    ReadOnlyMemoryRegionLengthFn read_only_memory_region_length,
    std::uintptr_t user_data) {}

TRANSLATE_KIT_EXPORT uintptr_t CreateTranslateKit() {
  return 0;
}

TRANSLATE_KIT_EXPORT void DeleteTranslateKit(uintptr_t kit_ptr) {
  NOTREACHED();
}

TRANSLATE_KIT_EXPORT bool TranslateKitSetLanguagePackages(
    uintptr_t kit_ptr,
    TranslateKitSetLanguagePackagesArgs args) {
  NOTREACHED();
}

TRANSLATE_KIT_EXPORT uintptr_t
TranslateKitCreateTranslator(uintptr_t kit_ptr,
                             TranslateKitLanguage source_lang,
                             TranslateKitLanguage target_lang) {
  NOTREACHED();
}

TRANSLATE_KIT_EXPORT void DeleteTranslator(uintptr_t translator_ptr) {
  NOTREACHED();
}

typedef void (*TranslateCallbackFn)(TranslateKitOutputText, std::uintptr_t);
DISABLE_CFI_DLSYM TRANSLATE_KIT_EXPORT bool TranslatorTranslate(
    uintptr_t translator_ptr,
    TranslateKitInputText input,
    TranslateCallbackFn callback,
    std::uintptr_t user_data) {
  NOTREACHED();
}

}  // extern C
