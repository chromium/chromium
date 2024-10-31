// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the ABI of libtranslatekit only for testing. It is not
// used in production.
// This mock implementation runs as follows:
// TranslateKitCreateTranslator()
//   - If the source language is "cause_crash", crash.
//   - Check the file "dict.dat" in the language package.
//   - If the file exists, create and returns a FakeTranslator.
//   - Otherwise, return a null pointer.
// TranslatorTranslate()
//   - If the input text is "SIMULATE_ERROR", return false.
//   - If the input text is "CAUSE_CRASH", crash.
//   - If the input text is "CHECK_NOT_EXIST_FILE", check if the file
//     "not_exist_file" exists in the language package. And pass the result of
//     Open() and FileExists() to the callback, and return true.
//   - Otherwise, pass the concatenation of the content of "dict.dat" and the
//     input text to the callback, and return true.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/services/on_device_translation/proto/translate_kit_api.pb.h"
#include "components/services/on_device_translation/translate_kit_structs.h"

#if BUILDFLAG(IS_WIN)
#define TRANSLATE_KIT_EXPORT __declspec(dllexport)
#else
#define TRANSLATE_KIT_EXPORT __attribute__((visibility("default")))
#endif

using chrome::on_device_translation::TranslateKitLanguagePackageConfig;

namespace {

// The storage backend is a global variable, set by InitializeStorageBackend().
struct StorageBackend {
  FileExistsFn file_exists;
  OpenForReadOnlyMemoryMapFn open_for_read_only_memory_map;
  DeleteReadOnlyMemoryRegionFn delete_read_only_memory_region;
  ReadOnlyMemoryRegionDataFn read_only_memory_region_data;
  ReadOnlyMemoryRegionLengthFn read_only_memory_region_length;
  std::uintptr_t user_data;
} g_storage_backend;

// FileExists() is a wrapper around the FileExistsFn.
DISABLE_CFI_DLSYM bool FileExists(const std::string_view file_name,
                                  bool* is_directory) {
  CHECK(g_storage_backend.file_exists);
  return g_storage_backend.file_exists(file_name.data(), file_name.size(),
                                       is_directory,
                                       g_storage_backend.user_data);
}

// FakeReadOnlyMemoryMap is a wrapper around the OpenForReadOnlyMemoryMapFn,
// DeleteReadOnlyMemoryRegionFn, ReadOnlyMemoryRegionDataFn, and
// ReadOnlyMemoryRegionLengthFn.
class FakeReadOnlyMemoryMap {
 public:
  DISABLE_CFI_DLSYM static std::unique_ptr<FakeReadOnlyMemoryMap> MaybeCreate(
      std::string_view path) {
    CHECK(g_storage_backend.open_for_read_only_memory_map);
    std::uintptr_t data_ptr = g_storage_backend.open_for_read_only_memory_map(
        path.data(), path.size(), g_storage_backend.user_data);
    if (data_ptr == 0) {
      return nullptr;
    }
    return std::make_unique<FakeReadOnlyMemoryMap>(
        data_ptr, base::PassKey<FakeReadOnlyMemoryMap>());
  }

  FakeReadOnlyMemoryMap(std::uintptr_t ptr,
                        base::PassKey<FakeReadOnlyMemoryMap>)
      : ptr_(ptr) {}
  DISABLE_CFI_DLSYM ~FakeReadOnlyMemoryMap() {
    CHECK(g_storage_backend.delete_read_only_memory_region);
    g_storage_backend.delete_read_only_memory_region(
        ptr_, g_storage_backend.user_data);
  }
  FakeReadOnlyMemoryMap(const FakeReadOnlyMemoryMap&) = delete;
  FakeReadOnlyMemoryMap& operator=(const FakeReadOnlyMemoryMap&) = delete;

  DISABLE_CFI_DLSYM const void* data() {
    CHECK(g_storage_backend.read_only_memory_region_data);
    return g_storage_backend.read_only_memory_region_data(
        ptr_, g_storage_backend.user_data);
  }
  DISABLE_CFI_DLSYM uint64_t size() {
    CHECK(g_storage_backend.read_only_memory_region_length);
    return g_storage_backend.read_only_memory_region_length(
        ptr_, g_storage_backend.user_data);
  }

 private:
  std::uintptr_t ptr_;
};

// This class implements a fake translator.
// Its Translate() method concatenates the content of "dict.dat" in the
// language package and the input text, and returns it as the translation
// result.
class FakeTranslator {
 public:
  static std::unique_ptr<FakeTranslator> MaybeCreate(
      size_t index,
      const std::string_view package_path) {
    std::string dict_path = base::StrCat({package_path, "/dict.dat"});
    auto dict_data = FakeReadOnlyMemoryMap::MaybeCreate(dict_path);
    bool is_directory = false;
    CHECK_EQ(!!dict_data, FileExists(dict_path, &is_directory));
    if (!dict_data) {
      return nullptr;
    }
    return std::make_unique<FakeTranslator>(package_path, std::move(dict_data),
                                            base::PassKey<FakeTranslator>());
  }

  FakeTranslator(const std::string_view package_path,
                 std::unique_ptr<FakeReadOnlyMemoryMap> dict,
                 base::PassKey<FakeTranslator>)
      : package_path_(package_path), dict_(std::move(dict)) {}
  ~FakeTranslator() = default;
  FakeTranslator(const FakeTranslator&) = delete;
  FakeTranslator& operator=(const FakeTranslator&) = delete;

  std::string Translate(const std::string_view input) {
    return base::StrCat(
        {std::string_view(reinterpret_cast<const char*>(dict_->data()),
                          dict_->size()),
         input});
  }

  const std::string& package_path() const { return package_path_; }

 private:
  const std::string package_path_;
  std::unique_ptr<FakeReadOnlyMemoryMap> dict_;
};

// This class implements a fake TranslateKit.
// It creates a FakeTranslator for each language pair if the language package
// contains "dict.dat".
class FakeTranslateKit {
 public:
  FakeTranslateKit() = default;
  ~FakeTranslateKit() = default;
  FakeTranslateKit(const FakeTranslateKit&) = delete;
  FakeTranslateKit& operator=(const FakeTranslateKit&) = delete;

  void SetConfig(TranslateKitLanguagePackageConfig config) {
    config_ = std::move(config);
  }
  std::unique_ptr<FakeTranslator> MaybeCreateTranslator(
      const std::string_view source_lang,
      const std::string_view target_lang) {
    for (int index = 0; index < config_.packages().size(); ++index) {
      const auto& package = config_.packages().at(index);
      if (package.language1() == source_lang &&
          package.language2() == target_lang) {
        return FakeTranslator::MaybeCreate(index, package.package_path());
      }
    }
    return nullptr;
  }

 private:
  TranslateKitLanguagePackageConfig config_;
};

}  // namespace

extern "C" {

TRANSLATE_KIT_EXPORT void InitializeStorageBackend(
    FileExistsFn file_exists,
    OpenForReadOnlyMemoryMapFn open_for_read_only_memory_map,
    DeleteReadOnlyMemoryRegionFn delete_read_only_memory_region,
    ReadOnlyMemoryRegionDataFn read_only_memory_region_data,
    ReadOnlyMemoryRegionLengthFn read_only_memory_region_length,
    std::uintptr_t user_data) {
  g_storage_backend.file_exists = file_exists;
  g_storage_backend.open_for_read_only_memory_map =
      open_for_read_only_memory_map;
  g_storage_backend.delete_read_only_memory_region =
      delete_read_only_memory_region;
  g_storage_backend.read_only_memory_region_data = read_only_memory_region_data;
  g_storage_backend.read_only_memory_region_length =
      read_only_memory_region_length;
  g_storage_backend.user_data = user_data;
}

TRANSLATE_KIT_EXPORT uintptr_t CreateTranslateKit() {
  return reinterpret_cast<uintptr_t>(new FakeTranslateKit());
}

TRANSLATE_KIT_EXPORT void DeleteTranslateKit(uintptr_t kit_ptr) {
  CHECK(kit_ptr);
  delete reinterpret_cast<FakeTranslateKit*>(kit_ptr);
}

TRANSLATE_KIT_EXPORT bool TranslateKitSetLanguagePackages(
    uintptr_t kit_ptr,
    TranslateKitSetLanguagePackagesArgs args) {
  CHECK(kit_ptr);

  TranslateKitLanguagePackageConfig config;
  if (!config.ParseFromArray(args.package_config, args.package_config_size)) {
    return false;
  }
  reinterpret_cast<FakeTranslateKit*>(kit_ptr)->SetConfig(std::move(config));
  return true;
}

TRANSLATE_KIT_EXPORT uintptr_t
TranslateKitCreateTranslator(uintptr_t kit_ptr,
                             TranslateKitLanguage source_lang,
                             TranslateKitLanguage target_lang) {
  CHECK(kit_ptr);
  if (std::string_view(source_lang.language_code,
                       source_lang.language_code_size) == "cause_crash") {
    LOG(ERROR) << "Intentionally terminating current process to simulate"
                  " the on device translation service crash for testing.";
    // Use `TerminateCurrentProcessImmediately()` instead of `CHECK()` to avoid
    // 'Fatal error' dialog on Windows debug.
    base::Process::TerminateCurrentProcessImmediately(1);
  }
  auto translator =
      reinterpret_cast<FakeTranslateKit*>(kit_ptr)->MaybeCreateTranslator(
          std::string_view(source_lang.language_code,
                           source_lang.language_code_size),
          std::string_view(target_lang.language_code,
                           target_lang.language_code_size));
  if (translator) {
    return reinterpret_cast<uintptr_t>(translator.release());
  }
  return 0;
}

TRANSLATE_KIT_EXPORT void DeleteTranslator(uintptr_t translator_ptr) {
  CHECK(translator_ptr);
  delete reinterpret_cast<FakeTranslator*>(translator_ptr);
}

typedef void (*TranslateCallbackFn)(TranslateKitOutputText, std::uintptr_t);
DISABLE_CFI_DLSYM TRANSLATE_KIT_EXPORT bool TranslatorTranslate(
    uintptr_t translator_ptr,
    TranslateKitInputText input,
    TranslateCallbackFn callback,
    std::uintptr_t user_data) {
  CHECK(translator_ptr);

  std::string_view input_string(input.input_text, input.input_text_size);
  if (input_string == "SIMULATE_ERROR") {
    return false;
  }

  if (input_string == "CAUSE_CRASH") {
    LOG(ERROR) << "Intentionally terminating current process to simulate"
                  " the on device translation service crash for testing.";
    // Use `TerminateCurrentProcessImmediately()` instead of `CHECK()` to avoid
    // 'Fatal error' dialog on Windows debug.
    base::Process::TerminateCurrentProcessImmediately(1);
  }

  if (input_string == "CHECK_NOT_EXIST_FILE") {
    const std::string not_exsit_file_name = base::StrCat(
        {reinterpret_cast<FakeTranslator*>(translator_ptr)->package_path(),
         "/not_exist_file"});
    std::unique_ptr<FakeReadOnlyMemoryMap> data =
        FakeReadOnlyMemoryMap::MaybeCreate(not_exsit_file_name);
    bool is_directory = false;
    bool file_exists = FileExists(not_exsit_file_name, &is_directory);
    const std::string result = base::StrCat({
        "Result of Open(): ",
        data ? "OK" : "Failed",
        ", result of FileExists(): ",
        file_exists ? "true" : "false",
        ", is_directory: ",
        is_directory ? "true" : "false",
    });
    callback(TranslateKitOutputText(result.data(), result.size()), user_data);
    return true;
  }
  const auto output = reinterpret_cast<FakeTranslator*>(translator_ptr)
                          ->Translate(input_string);
  // Just pass the input text.
  callback(TranslateKitOutputText(output.data(), output.size()), user_data);
  return true;
}

}  // extern C
