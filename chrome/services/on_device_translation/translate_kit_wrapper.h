// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_WRAPPER_H_
#define CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_WRAPPER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"

namespace on_device_translation {

using TranslateFunc = base::RepeatingCallback<std::string(const std::string&)>;

const char kTranslateKitLibraryName[] = "translatekit";

// A wrapper of the translatekit library. It loads the library's binary and
// store the exposed function pointers for the translator to use.
class TranslateKitWrapper {
 public:
  TranslateKitWrapper(const TranslateKitWrapper&) = delete;
  TranslateKitWrapper& operator=(const TranslateKitWrapper&) = delete;

  // Gets or creates the static instance of TranslateKitWrapper. It also
  // attempts to load the TranslateKit library if it's not done yet.
  static void GetInstance(
      base::OnceCallback<void(TranslateKitWrapper*)> callback);

  // Returns the function that does translation from `source_lang` to
  // `target_lang`.
  std::optional<TranslateFunc> GetTranslateFunc(const std::string& source_lang,
                                                const std::string& target_lang);

  // Returns if the translation from `source_lang` to `target_lang` is
  // supported.
  bool CanTranslate(const std::string& source_lang,
                    const std::string& target_lang);

 private:
  friend base::NoDestructor<TranslateKitWrapper>;

  TranslateKitWrapper();
  ~TranslateKitWrapper();

  void Load(base::OnceCallback<void(TranslateKitWrapper*)> callback);

  std::optional<base::FilePath> GetTranslateKitLibraryBasePath();

  std::optional<base::FilePath> GetTranslateKitLibraryPath();

  std::optional<base::FilePath> GetTranslateLibraryRunFilesPath();

  void LoadTranslateKit();

  // bool IsLanguageSupported(const std::string& lang)
  bool (*is_language_supported_func_)(const std::string&);
  base::ScopedNativeLibrary translate_kit_library_;

  // std::uintptr_t CreateTranslator(
  //    const std::string& runfiles_dir,
  //    const std::string& source_lang,
  //    const std::string& target_lang
  // )
  std::uintptr_t (*create_translator_func_)(const std::string&,
                                            const std::string&,
                                            const std::string&);

  // std::string DoTranslation(
  //    std::uintptr_t translator_ptr,
  //    const std::string& text
  // )
  std::string (*do_translation_func_)(std::uintptr_t translator_ptr,
                                      const std::string& text);

  bool is_library_loaded_ = false;
};

}  // namespace on_device_translation

#endif  // CHROME_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_WRAPPER_H_
