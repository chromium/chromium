// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/on_device_translation/translate_kit_client.h"

#include <map>
#include <optional>

#include "base/compiler_specific.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/services/on_device_translation/translate_kit_structs.h"

namespace on_device_translation {
namespace {

// TODO(crbug.com/364773827): Reflect this size in the API.
// The maximum buffer size to put the translation result into.
constexpr size_t kTranslationResultBufferSize = 10000;

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

}  // namespace

TranslateKitClient::TranslateKitClient(base::FilePath library_path)
    : lib_(library_path) {
  if (!lib_.is_valid()) {
    load_lib_result_ = LoadTranslateKitResult::kInvalidBinary;
    LogLoadTranslateKitResult(load_lib_result_, lib_.GetError());
    return;
  }

  // TODO(crbug.com/364773827): Update the following ABIs.
  // Call `InitGoogle()` before everything.
  reinterpret_cast<void (*)()>(lib_.GetFunctionPointer("InitGoogle"))();

  is_language_supported_func_ =
      reinterpret_cast<bool (*)(TranslateKitLanguage)>(
          lib_.GetFunctionPointer("IsLanguageSupported"));
  create_translator_func_ = reinterpret_cast<std::uintptr_t (*)(
      TranslateKitPath, TranslateKitLanguage, TranslateKitLanguage)>(
      lib_.GetFunctionPointer("CreateTranslator"));
  delete_translator_func_ = reinterpret_cast<void (*)(std::uintptr_t)>(
      lib_.GetFunctionPointer("DeleteTranslator"));
  do_translation_func_ = reinterpret_cast<bool (*)(
      std::uintptr_t, TranslateKitInputText, TranslateKitOutputText)>(
      lib_.GetFunctionPointer("DoTranslation"));

  if (!is_language_supported_func_ || !create_translator_func_ ||
      !delete_translator_func_ || !do_translation_func_) {
    load_lib_result_ = LoadTranslateKitResult::kInvalidFunctionPointer;
  } else {
    load_lib_result_ = LoadTranslateKitResult::kSuccess;
  }
  LogLoadTranslateKitResult(load_lib_result_, lib_.GetError());
}

TranslateKitClient::~TranslateKitClient() {
  translators_.clear();
}

DISABLE_CFI_DLSYM
bool TranslateKitClient::CanTranslate(const std::string& source_lang,
                                      const std::string& target_lang) {
  if (!IsInitialized()) {
    return false;
  }

  return is_language_supported_func_(
      TranslateKitLanguage(source_lang.c_str(), source_lang.size()));
}

TranslateKitClient::Translator* TranslateKitClient::GetTranslator(
    const std::string& source_lang,
    const std::string& target_lang) {
  if (!CanTranslate(source_lang, target_lang)) {
    return nullptr;
  }

  TranslatorKey key(source_lang, target_lang);
  if (auto it = translators_.find(key); it != translators_.end()) {
    return it->second.get();
  }
  auto translator = std::make_unique<TranslateKitClient::TranslatorImpl>(
      this, source_lang, target_lang);
  auto raw_translator_ptr = translator.get();
  translators_.emplace(std::move(key), std::move(translator));
  return raw_translator_ptr;
}

DISABLE_CFI_DLSYM
TranslateKitClient::TranslatorImpl::TranslatorImpl(
    TranslateKitClient* client,
    const std::string& source_lang,
    const std::string& target_lang)
    : client_(client) {
  CHECK(client_->IsInitialized());
  // TODO(crbug.com/362123222): Implement a model loader and remove this.
  // For now pass in an empty path which will fail subsequent calls.
  std::string runfiles_path;

  translator_ptr_ = client_->create_translator_func_(
      TranslateKitPath(runfiles_path.c_str(), runfiles_path.length()),
      TranslateKitLanguage(source_lang.c_str(), source_lang.length()),
      TranslateKitLanguage(target_lang.c_str(), target_lang.length()));
}

DISABLE_CFI_DLSYM
TranslateKitClient::TranslatorImpl::~TranslatorImpl() {
  CHECK(client_->IsInitialized());
  client_->delete_translator_func_(translator_ptr_);
}

DISABLE_CFI_DLSYM
std::optional<std::string> TranslateKitClient::TranslatorImpl::Translate(
    const std::string& text) {
  CHECK(client_->IsInitialized());

  // TODO(crbug.com/362123222): Update the ABI.
  std::vector<char> buffer(kTranslationResultBufferSize);
  TranslateKitOutputText output;
  output.buffer = buffer.data();
  output.buffer_size = buffer.size();
  if (client_->do_translation_func_(
          translator_ptr_, TranslateKitInputText(text.c_str(), text.length()),
          output)) {
    return std::string(buffer.begin(), buffer.end());
  }
  return std::nullopt;
}

}  // namespace on_device_translation
