// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DOWNLOAD_MANAGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DOWNLOAD_MANAGER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_script.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace translate {

// Manages the downloaded resources for Translate, such as the translate script
// and the language list.
class TranslateDownloadManager {
 public:
  // Returns the singleton instance.
  static TranslateDownloadManager* GetInstance();

  // The URL loader factory used to download the resources.
  // Should be set before this class can be used.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return url_loader_factory_;
  }
  void set_url_loader_factory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = std::move(url_loader_factory);
  }

  // The application locale.
  // Should be set before this class can be used.
  const std::string& application_locale() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return application_locale_;
  }
  void set_application_locale(const std::string& locale) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    application_locale_ = locale;
  }

  // The language list.
  TranslateLanguageList* language_list() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return language_list_.get();
  }

  // The translate script.
  TranslateScript* script() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return script_.get();
  }

  // Fills |languages| with the alphabetically sorted list of languages that the
  // translate server can translate to and from. May cause a language list
  // request unless |translate_allowed| is false.
  static void GetSupportedLanguages(bool translate_allowed,
                                    std::vector<std::string>* languages);

  // Refresh the language list if needed.
  static void RequestLanguageList();

  // Returns the last-updated time when Chrome received a language list from a
  // Translate server. Returns null time if Chrome hasn't received any lists.
  static base::Time GetSupportedLanguagesLastUpdated();

  // Returns the language code that can be used with the Translate method for a
  // specified |language|. (ex. GetLanguageCode("en-US") will return "en", and
  // GetLanguageCode("zh-CN") returns "zh-CN")
  static std::string GetLanguageCode(std::string_view language);

  // Returns true if |language| is supported by the translation server.
  static bool IsSupportedLanguage(std::string_view language);

  // Must be called to shut Translate down. Cancels any pending fetches.
  void Shutdown();

  // Clears the translate script, so it will be fetched next time we translate.
  void ClearTranslateScriptForTesting();

  // Resets to its initial state as if newly created.
  void ResetForTesting();

  // Used by unit-tests to override some defaults:
  // Delay after which the translate script is fetched again from the
  // translation server.
  void SetTranslateScriptExpirationDelay(int delay_ms);

 private:
  friend struct base::DefaultSingletonTraits<TranslateDownloadManager>;
  TranslateDownloadManager();
  virtual ~TranslateDownloadManager();

  // Validates that accesses to the download manager are performed on the same
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<TranslateLanguageList> language_list_;

  // An instance of TranslateScript which manages JavaScript source for
  // Translate.
  std::unique_ptr<TranslateScript> script_;

  std::string application_locale_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DOWNLOAD_MANAGER_H_
