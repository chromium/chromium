// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_CLIENT_H_
#define CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_CLIENT_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "chrome/services/on_device_translation/translate_kit_structs.h"

namespace on_device_translation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LoadTranslateKitResult)
enum class LoadTranslateKitResult {
  kUnknown = 0,
  // Success to load TranslateKit library.
  kSuccess = 1,
  // Fails due to invalid TranslateKit binary.
  kInvalidBinary = 2,
  // Success to load TranslateKit binary but fails due to invalid function
  // pointers.
  kInvalidFunctionPointer = 3,
  kMaxValue = kInvalidFunctionPointer,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ai/enums.xml:LoadTranslateKitResult)

// The client that wraps the plain C-style interfaces between Chrome and the
// TranslateKit. Changes to the interfaces must be backwards compatible and
// reflected in the Google3-side definition.
class TranslateKitClient {
 public:
  // Translator provides access to translation functionality.
  class Translator {
   public:
    virtual ~Translator() = default;
    virtual std::optional<std::string> Translate(const std::string& text) = 0;
  };

  // `library_path` is the fully-qualified path to the TranslateKit binary.
  // `package_info_dir` is the fully-qualified path to the directory where
  // PackageInfo files exist.
  // `model_root_dir` is the fully-qualified path to the directory where
  // TranslateKit models exist.
  TranslateKitClient(base::FilePath library_path,
                     base::FilePath package_info_dir,
                     base::FilePath model_root_dir);
  ~TranslateKitClient();

  // Not copyable.
  TranslateKitClient(const TranslateKitClient&) = delete;
  TranslateKitClient& operator=(const TranslateKitClient&) = delete;

  // Returns whether this client has been initialized.
  bool IsInitialized() {
    return load_lib_result_ == LoadTranslateKitResult::kSuccess;
  }

  // Returns if the translation from `source_lang` to `target_lang` is
  // supported.
  bool CanTranslate(const std::string& source_lang,
                    const std::string& target_lang);

  // Returns a Translator instance for the given pair of languages.
  // Returns null if either the language pair is not supported or fails to
  // to create such Translator.
  Translator* GetTranslator(const std::string& source_lang,
                            const std::string& target_lang);

 private:
  // TranslatorImpl manages an instance of translator created by TranslateKit
  // library and provides access to its translation functionality.
  class TranslatorImpl : public Translator {
   public:
    TranslatorImpl(TranslateKitClient* client,
                   const std::string& source_lang,
                   const std::string& target_lang);
    ~TranslatorImpl() override;
    // Not copyable.
    TranslatorImpl(const TranslatorImpl&) = delete;
    TranslatorImpl& operator=(const TranslatorImpl&) = delete;

    std::optional<std::string> Translate(const std::string& text) override;

   private:
    // Guaranteed to exist, as `client_` owns `this`.
    raw_ptr<TranslateKitClient> client_;
    // A pointer to a Translator instance created by the TranslateKit.
    // It should only be instantiated and deleted by the TranslateKit library.
    std::uintptr_t translator_ptr_;
  };

  // The TranslateKit binary.
  base::ScopedNativeLibrary lib_;
  // The results after attempting to load `lib_`.
  LoadTranslateKitResult load_lib_result_ = LoadTranslateKitResult::kUnknown;

  // The absolute path to the directory containing PackageInfo files.
  const base::FilePath package_info_dir_;
  // The absolute path to the directory of TranslateKit models.
  const base::FilePath model_root_dir_;

  using TranslatorKey = std::pair<std::string, std::string>;
  // Manages all instances of `Translator` created by this client.
  std::map<TranslatorKey, std::unique_ptr<Translator>> translators_;

  // WARNING:
  // Changes to the below interfaces must be backwards compatible and
  // reflected in the Google3-side definition.
  typedef bool (*IsLanguageSupportedFunction)(TranslateKitLanguage lang);
  IsLanguageSupportedFunction is_language_supported_func_;

  typedef std::uintptr_t (*CreateTranslatorFunction)(
      TranslateKitTranslatorConfig);
  CreateTranslatorFunction create_translator_func_;

  typedef void (*DeleteTranslatorFunction)(std::uintptr_t translator_ptr);
  DeleteTranslatorFunction delete_translator_func_;

  typedef bool (*DoTranslationFunction)(std::uintptr_t translator_ptr,
                                        TranslateKitInputText,
                                        TranslateKitOutputText);
  DoTranslationFunction do_translation_func_;
};

}  // namespace on_device_translation

#endif  // CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_CLIENT_H_
