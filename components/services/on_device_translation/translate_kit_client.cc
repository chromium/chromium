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
#include "base/strings/string_util.h"
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

bool ParseFilePath(const char* file_name,
                   size_t file_name_size,
                   uint32_t& package_index,
                   base::FilePath& relative_path) {
  std::string path(file_name, file_name_size);
  // The TranslateKit only use ASCII paths.
  CHECK(base::IsStringASCII(path));
#if BUILDFLAG(IS_WIN)
  base::ReplaceChars(path, "/", "\\", &path);
#endif  // BUILDFLAG(IS_WIN)
  base::FilePath virtual_path = base::FilePath::FromASCII(path);
  // The TranslateKit doesn't use '..'.
  CHECK(!virtual_path.ReferencesParent());
  // The TranslateKit must use an absolute path.
  CHECK(virtual_path.IsAbsolute());
  const std::vector<base::FilePath::StringType> components =
      virtual_path.GetComponents();
#if BUILDFLAG(IS_WIN)
  // Windows:  "X:\0\bar"  ->  [ "X:", "\\", "0", "bar" ]
  //                                         ^^^ : component_idx = 2
  size_t component_idx = 2;
#else
  // Posix:  "/0/bar"  ->  [ "/", "0", "bar" ]
  //                              ^^^ : component_idx = 1
  size_t component_idx = 1;
#endif  // BUILDFLAG(IS_WIN)
  CHECK_GT(components.size(), component_idx + 1);
  CHECK(base::StringToUint(components[component_idx], &package_index));
  ++component_idx;
  base::FilePath result;
  for (; component_idx < components.size(); ++component_idx) {
    result = result.Append(components[component_idx]);
  }
  relative_path = result;
  return true;
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

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
void TranslateKitClient::SetConfig(
    mojom::OnDeviceTranslationServiceConfigPtr config) {
  if (!MaybeInitialize()) {
    return;
  }

  // When `file_operation_proxy_` is set, need to reset `file_operation_proxy_`
  // before binding the new one. This happens when SetConfig() is called again
  // for the new config.
  if (file_operation_proxy_) {
    file_operation_proxy_.reset();
  }
  file_operation_proxy_.Bind(std::move(config->file_operation_proxy));
  chrome::on_device_translation::TranslateKitLanguagePackageConfig config_proto;
  size_t index = 0;
  for (const auto& package : config->packages) {
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
  }

  const std::string packages_str = config_proto.SerializeAsString();
  CHECK(set_language_packages_func_(
      kit_ptr_, TranslateKitSetLanguagePackagesArgs{packages_str.c_str(),
                                                    packages_str.size()}))
      << "Failed to set config";
}

DISABLE_CFI_DLSYM
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
  if (!file_operation_proxy_) {
    return false;
  }
  uint32_t package_index = 0;
  base::FilePath relative_path;
  if (!ParseFilePath(file_name, file_name_size, package_index, relative_path)) {
    return false;
  }
  bool exists = false;
  file_operation_proxy_->FileExists(package_index, relative_path, &exists,
                                    is_directory);
  return exists;
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
  if (!file_operation_proxy_) {
    return 0;
  }
  uint32_t package_index = 0;
  base::FilePath relative_path;
  if (!ParseFilePath(file_name, file_name_size, package_index, relative_path)) {
    return 0;
  }
  base::File file;
  file_operation_proxy_->Open(package_index, relative_path, &file);
  if (!file.IsValid()) {
    return 0;
  }
  std::unique_ptr<base::MemoryMappedFile> mapped_file =
      std::make_unique<base::MemoryMappedFile>();
  if (mapped_file->Initialize(std::move(file))) {
    return reinterpret_cast<std::uintptr_t>(mapped_file.release());
  }
  return 0;
}

}  // namespace on_device_translation
