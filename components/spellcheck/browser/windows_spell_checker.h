// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_WINDOWS_SPELL_CHECKER_H_
#define COMPONENTS_SPELLCHECK_BROWSER_WINDOWS_SPELL_CHECKER_H_

#include <spellcheck.h>
#include <wrl/client.h>

#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/platform_spell_checker.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace windows_spell_checker {
class BackgroundHelper;
}

// Class used to store all the COM objects and control their lifetime. The class
// also provides wrappers for ISpellCheckerFactory and ISpellChecker APIs. All
// COM calls are executed on the background thread.
class WindowsSpellChecker : public PlatformSpellChecker {
 public:
  explicit WindowsSpellChecker(
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);

  WindowsSpellChecker(const WindowsSpellChecker&) = delete;

  WindowsSpellChecker& operator=(const WindowsSpellChecker&) = delete;

  ~WindowsSpellChecker() override;

  void CreateSpellChecker(const std::string& lang_tag,
                          base::OnceCallback<void(bool)> callback);

  void DisableSpellChecker(const std::string& lang_tag);

  void RequestTextCheck(
      int document_tag,
      const std::u16string& text,
      spellcheck_platform::TextCheckCompleteCallback callback) override;

  void GetPerLanguageSuggestions(
      const std::u16string& word,
      spellcheck_platform::GetSuggestionsCallback callback);

  void AddWordForAllLanguages(const std::u16string& word);

  void RemoveWordForAllLanguages(const std::u16string& word);

  void IgnoreWordForAllLanguages(const std::u16string& word);

  void RecordChromeLocalesStats(std::vector<std::string> chrome_locales);

  void RecordSpellcheckLocalesStats(
      std::vector<std::string> spellcheck_locales);

  void IsLanguageSupported(const std::string& lang_tag,
                           base::OnceCallback<void(bool)> callback);

  // Asynchronously retrieve language tags for registered Windows OS
  // spellcheckers on the system. Callback will pass an empty vector of language
  // tags if the OS does not support spellcheck.
  void RetrieveSpellcheckLanguages(
      spellcheck_platform::RetrieveSpellcheckLanguagesCompleteCallback
          callback);

  // Test-only method for adding fake list of Windows spellcheck languages.
  void AddSpellcheckLanguagesForTesting(
      const std::vector<std::string>& languages);

 private:
  // COM-enabled, single-thread task runner used to post invocations of
  // BackgroundHelper methods to interact with spell check native APIs.
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;

  // Instance of the background helper to invoke native APIs on the COM-enabled
  // background thread. |background_helper_| is deleted on the background thread
  // after all other background tasks complete.
  std::unique_ptr<windows_spell_checker::BackgroundHelper> background_helper_;
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_WINDOWS_SPELL_CHECKER_H_
