// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/translate_kit_client.h"

#include <map>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/services/on_device_translation/proto/translate_kit_api.pb.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "components/services/on_device_translation/translate_kit_structs.h"

namespace on_device_translation {
namespace {

// Logs UMA after an attempt to load the TranslateKit binary.
void LogLoadTranslateKitResult(LoadTranslateKitResult result,
                               const base::NativeLibraryLoadError* error) {
  base::UmaHistogramEnumeration("AI.Translation.LoadTranslateKitResult",
                                result);
#if BUILDFLAG(IS_WIN)
  if (result == LoadTranslateKitResult::kInvalidBinary) {
    base::UmaHistogramSparse("AI.Translation.LoadTranslateKitErrorCode",
                             error->code);
  }
#endif  // BUILDFLAG(IS_WIN)
}

// This method is used to receive the result from TranslatorTranslate() method.
void TranslateCallback(TranslateKitOutputText result,
                       std::uintptr_t user_data) {
  std::string* output = reinterpret_cast<std::string*>(user_data);
  CHECK(output);
  CHECK(result.buffer);
  *output = std::string(result.buffer, result.buffer_size);
}

void DeleteReadOnlyMemoryRegion(std::uintptr_t memory_map_ptr,
                                std::uintptr_t user_data) {
  CHECK(memory_map_ptr);
  delete reinterpret_cast<base::MemoryMappedFile*>(memory_map_ptr);
}

const void* ReadOnlyMemoryRegionData(std::uintptr_t memory_map_ptr,
                                     std::uintptr_t user_data) {
  CHECK(memory_map_ptr);
  return reinterpret_cast<base::MemoryMappedFile*>(memory_map_ptr)->data();
}

uint64_t ReadOnlyMemoryRegionLength(std::uintptr_t memory_map_ptr,
                                    std::uintptr_t user_data) {
  CHECK(memory_map_ptr);
  return reinterpret_cast<base::MemoryMappedFile*>(memory_map_ptr)->length();
}
}  // namespace

// static
TranslateKitClient* TranslateKitClient::Get() {
  static base::NoDestructor<std::unique_ptr<TranslateKitClient>> client(
      std::make_unique<TranslateKitClient>(
          GetTranslateKitBinaryPathFromCommandLine(),
          base::PassKey<TranslateKitClient>()));
  return client->get();
}

TranslateKitClient::TranslateKitClient(const base::FilePath& library_path,
                                       base::PassKey<TranslateKitClient>)
    : lib_(library_path),
      initialize_storage_backend_fnc_(
          reinterpret_cast<InitializeStorageBackendFn>(
              lib_.GetFunctionPointer("InitializeStorageBackend"))),
      create_translate_kit_fnc_(reinterpret_cast<CreateTranslateKitFn>(
          lib_.GetFunctionPointer("CreateTranslateKit"))),
      delete_tanslate_kit_fnc_(reinterpret_cast<DeleteTranslateKitFn>(
          lib_.GetFunctionPointer("DeleteTranslateKit"))),
      set_language_packages_func_(
          reinterpret_cast<TranslateKitSetLanguagePackagesFn>(
              lib_.GetFunctionPointer("TranslateKitSetLanguagePackages"))),
      translate_kit_create_translator_func_(
          reinterpret_cast<TranslateKitCreateTranslatorFn>(
              lib_.GetFunctionPointer("TranslateKitCreateTranslator"))),
      delete_translator_fnc_(reinterpret_cast<DeleteTranslatorFn>(
          lib_.GetFunctionPointer("DeleteTranslator"))),
      translator_translate_func_(reinterpret_cast<TranslatorTranslateFn>(
          lib_.GetFunctionPointer("TranslatorTranslate"))) {
  if (!lib_.is_valid()) {
    load_lib_result_ = LoadTranslateKitResult::kInvalidBinary;
    LogLoadTranslateKitResult(load_lib_result_, lib_.GetError());
    return;
  }

  if (!initialize_storage_backend_fnc_ || !create_translate_kit_fnc_ ||
      !delete_tanslate_kit_fnc_ || !set_language_packages_func_ ||
      !translate_kit_create_translator_func_ || !delete_translator_fnc_ ||
      !translator_translate_func_) {
    load_lib_result_ = LoadTranslateKitResult::kInvalidFunctionPointer;
  } else {
    load_lib_result_ = LoadTranslateKitResult::kSuccess;
  }
  LogLoadTranslateKitResult(load_lib_result_, lib_.GetError());
}

bool TranslateKitClient::MaybeInitialize() {
  if (failed_to_initialize_ ||
      load_lib_result_ != LoadTranslateKitResult::kSuccess) {
    return false;
  }
  if (kit_ptr_) {
    return true;
  }
  initialize_storage_backend_fnc_(
      &TranslateKitClient::FileExists,
      &TranslateKitClient::OpenForReadOnlyMemoryMap,
      &DeleteReadOnlyMemoryRegion, &ReadOnlyMemoryRegionData,
      &ReadOnlyMemoryRegionLength, reinterpret_cast<std::uintptr_t>(this));

  kit_ptr_ = create_translate_kit_fnc_();
  if (!kit_ptr_) {
    failed_to_initialize_ = true;
  }
  return !!kit_ptr_;
}

void TranslateKitClient::SetConfig(
    mojom::OnDeviceTranslationServiceConfigPtr config) {
  if (!MaybeInitialize()) {
    return;
  }
  config_ = std::move(config);
  directories_.clear();
  files_.clear();

  chrome::on_device_translation::TranslateKitLanguagePackageConfig config_proto;
  size_t index = 0;
  for (const auto& package : config_->packages) {
    // Generate a virtual absolute file path for the package.
    // On Windows, set the package path to a fake drive letter 'X:' to avoid
    // the file path validation in the TranslateKit.
    const std::string package_path =
#if BUILDFLAG(IS_WIN)
        base::StrCat({"X:\\", base::NumberToString(index++)});
#else
        base::StrCat({"/", base::NumberToString(index++)});
#endif  // BUILDFLAG(IS_WIN)
    auto* new_package = config_proto.add_packages();
    new_package->set_language1(package->language1);
    new_package->set_language2(package->language2);
    new_package->set_package_path(package_path);

    for (const auto& file : package->files) {
      // Calling AsUTF8Unsafe() is safe here because we have already checked the
      // file name is ASCII in the browser process.
      // We intentionally use '/' for the directory separator even on Windows,
      // because TranslateKit uses '/' as the directory separator.
      const std::string file_path =
          base::StrCat({package_path, "/", file->relative_path.AsUTF8Unsafe()});
      const std::size_t found = file_path.rfind('/');
      CHECK(found != std::string::npos);
      directories_.insert(file_path.substr(0, found));
      files_[file_path] = std::move(file->file);
    }
  }

  const std::string packages_str = config_proto.SerializeAsString();
  CHECK(set_language_packages_func_(
      kit_ptr_, TranslateKitSetLanguagePackagesArgs{packages_str.c_str(),
                                                    packages_str.size()}))
      << "Failed to set config";
}

TranslateKitClient::~TranslateKitClient() {
  if (!kit_ptr_) {
    return;
  }
  delete_tanslate_kit_fnc_(kit_ptr_);
  kit_ptr_ = 0;
  translators_.clear();
}

bool TranslateKitClient::CanTranslate(const std::string& source_lang,
                                      const std::string& target_lang) {
  if (!MaybeInitialize()) {
    return false;
  }
  return !!TranslateKitClient::GetTranslator(source_lang, target_lang);
}

TranslateKitClient::Translator* TranslateKitClient::GetTranslator(
    const std::string& source_lang,
    const std::string& target_lang) {
  if (!MaybeInitialize()) {
    return nullptr;
  }

  TranslatorKey key(source_lang, target_lang);
  if (auto it = translators_.find(key); it != translators_.end()) {
    return it->second.get();
  }
  auto translator = TranslatorImpl::MaybeCreate(this, source_lang, target_lang);
  if (!translator) {
    return nullptr;
  }
  auto raw_translator_ptr = translator.get();
  translators_.emplace(std::move(key), std::move(translator));
  return raw_translator_ptr;
}

DISABLE_CFI_DLSYM
std::unique_ptr<TranslateKitClient::TranslatorImpl>
TranslateKitClient::TranslatorImpl::MaybeCreate(
    TranslateKitClient* client,
    const std::string& source_lang,
    const std::string& target_lang) {
  CHECK(client->translate_kit_create_translator_func_);
  std::uintptr_t translator_ptr = client->translate_kit_create_translator_func_(
      client->kit_ptr_,
      TranslateKitLanguage(source_lang.c_str(), source_lang.length()),
      TranslateKitLanguage(target_lang.c_str(), target_lang.length()));
  if (!translator_ptr) {
    return nullptr;
  }
  return std::make_unique<TranslatorImpl>(base::PassKey<TranslatorImpl>(),
                                          client, translator_ptr);
}

DISABLE_CFI_DLSYM
TranslateKitClient::TranslatorImpl::TranslatorImpl(
    base::PassKey<TranslatorImpl>,
    TranslateKitClient* client,
    std::uintptr_t translator_ptr)
    : client_(client), translator_ptr_(translator_ptr) {}

DISABLE_CFI_DLSYM
TranslateKitClient::TranslatorImpl::~TranslatorImpl() {
  CHECK(client_->delete_translator_fnc_);
  client_->delete_translator_fnc_(translator_ptr_);
}

DISABLE_CFI_DLSYM
std::optional<std::string> TranslateKitClient::TranslatorImpl::Translate(
    const std::string& text) {
  CHECK(client_->translator_translate_func_);
  std::string output;
  if (client_->translator_translate_func_(
          translator_ptr_, TranslateKitInputText(text.c_str(), text.length()),
          TranslateCallback, reinterpret_cast<uintptr_t>(&text))) {
    return text;
  }
  return std::nullopt;
}

// static
bool TranslateKitClient::FileExists(const char* file_name,
                                    size_t file_name_size,
                                    bool* is_directory,
                                    std::uintptr_t user_data) {
  CHECK(file_name);
  CHECK(is_directory);
  CHECK(user_data);
  return reinterpret_cast<TranslateKitClient*>(user_data)->FileExistsImpl(
      file_name, file_name_size, is_directory);
}
bool TranslateKitClient::FileExistsImpl(const char* file_name,
                                        size_t file_name_size,
                                        bool* is_directory) {
  std::string file_name_str(file_name, file_name_size);
  if (!config_) {
    return false;
  }
  if (directories_.contains(file_name_str)) {
    *is_directory = true;
    return true;
  }
  if (files_.find(file_name_str) != files_.end()) {
    return true;
  }
  return false;
}

// static
std::uintptr_t TranslateKitClient::OpenForReadOnlyMemoryMap(
    const char* file_name,
    size_t file_name_size,
    std::uintptr_t user_data) {
  CHECK(file_name);
  CHECK(user_data);
  return reinterpret_cast<TranslateKitClient*>(user_data)
      ->OpenForReadOnlyMemoryMapImpl(file_name, file_name_size);
}

std::uintptr_t TranslateKitClient::OpenForReadOnlyMemoryMapImpl(
    const char* file_name,
    size_t file_name_size) {
  std::string file_name_str(file_name, file_name_size);
  auto it = files_.find(file_name_str);
  if (it != files_.end()) {
    std::unique_ptr<base::MemoryMappedFile> mapped_file =
        std::make_unique<base::MemoryMappedFile>();
    if (mapped_file->Initialize(it->second.Duplicate())) {
      return reinterpret_cast<std::uintptr_t>(mapped_file.release());
    }
  }
  return 0;
}

}  // namespace on_device_translation
