// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/translate_kit_client.h"

#include <map>
#include <optional>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
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

chrome::on_device_translation::TranslateKitLanguagePackageConfig ToProto(
    const mojom::OnDeviceTranslationServiceConfig& config) {
  chrome::on_device_translation::TranslateKitLanguagePackageConfig result;
  for (const auto& package : config.packages) {
    auto* new_package = result.add_packages();
    new_package->set_language1(package->language1);
    new_package->set_language2(package->language2);
#if BUILDFLAG(IS_WIN)
    new_package->set_package_path(package->package_path.AsUTF8Unsafe());
#else
    new_package->set_package_path(package->package_path.value());
#endif  // BUILDFLAG(IS_WIN)
  }
  return result;
}

// This method is used to receive the result from TranslatorTranslate() method.
void TranslateCallback(TranslateKitOutputText result,
                       std::uintptr_t user_data) {
  std::string* output = reinterpret_cast<std::string*>(user_data);
  CHECK(output);
  CHECK(result.buffer);
  *output = std::string(result.buffer, result.buffer_size);
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

  if (!create_translate_kit_fnc_ || !delete_tanslate_kit_fnc_ ||
      !set_language_packages_func_ || !translate_kit_create_translator_func_ ||
      !delete_translator_fnc_ || !translator_translate_func_) {
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
  const std::string config_str = ToProto(*config).SerializeAsString();
  CHECK(set_language_packages_func_(
      kit_ptr_, TranslateKitSetLanguagePackagesArgs{config_str.c_str(),
                                                    config_str.size()}))
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

}  // namespace on_device_translation
