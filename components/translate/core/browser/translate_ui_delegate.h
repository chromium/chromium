// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/translate_errors.h"

namespace translate {

class LanguageState;
class TranslateDriver;
class TranslateManager;
class TranslatePrefs;
class TranslateUILanguagesManager;

// The delegate for the Full Page Translate Bubble UI.
class TranslateUIDelegate {
 public:
  TranslateUIDelegate(const base::WeakPtr<TranslateManager>& translate_manager,
                      const std::string& source_language,
                      const std::string& target_language);

  TranslateUIDelegate(const TranslateUIDelegate&) = delete;
  TranslateUIDelegate& operator=(const TranslateUIDelegate&) = delete;

  ~TranslateUIDelegate();

  TranslateUILanguagesManager* translate_ui_languages_manager() {
    return translate_ui_languages_manager_.get();
  }

  // Wrappers for equivalent TranslateUILanguagesManager APIs used to add
  // Full Page Translate related logging.
  void UpdateAndRecordSourceLanguageIndex(size_t language_index);
  void UpdateAndRecordSourceLanguage(const std::string& language_code);
  void UpdateAndRecordTargetLanguageIndex(size_t language_index);
  void UpdateAndRecordTargetLanguage(const std::string& language_code);

  // Handles when an error message is shown.
  void OnErrorShown(TranslateErrors error_type);

  // Returns the LanguageState associated with this object.
  const LanguageState* GetLanguageState();

  // Translatable content languages.
  void GetContentLanguagesCodes(
      std::vector<std::string>* content_languages_codes) const;

  // Starts translating the current page.
  void Translate();

  // Reverts translation.
  void RevertTranslation();

  // Processes when the user declines translation.
  // The function name is not accurate. It only means the user did not take
  // affirmative action after the translation ui show up. The user either
  // actively decline the translation or ignore the prompt of translation.
  //   Pass |explicitly_closed| as true if user explicityly decline the
  //     translation.
  //   Pass |explicitly_closed| as false if the translation UI is dismissed
  //     implicit by some user actions which ignore the translation UI,
  //     such as switch to a new tab/window or navigate to another page by
  //     click a link.
  void TranslationDeclined(bool explicitly_closed);

  // Returns true if the current language is blocked.
  bool IsLanguageBlocked() const;

  // Sets the value if the current language is blocked.
  void SetLanguageBlocked(bool value);

  // Returns true if the current webpage should never be prompted for
  // translation.
  bool IsSiteOnNeverPromptList() const;

  // Returns true if the site of the current webpage can be put on the never
  // prompt list.
  bool CanAddSiteToNeverPromptList() const;

  // Sets the never-prompt state for the host of the current page. If
  // value is true, the current host will be blocklisted and translation
  // prompts will not show for that site.
  void SetNeverPromptSite(bool value);

  // Returns true if the webpage in the current source language should be
  // translated into the current target language automatically.
  bool ShouldAlwaysTranslate() const;

  // Sets the value if the webpage in the current source language should be
  // translated into the current target language automatically.
  void SetAlwaysTranslate(bool value);

  // Returns true if the Always Translate checkbox should be checked by default.
  bool ShouldAlwaysTranslateBeCheckedByDefault() const;

  // Returns true if the UI should offer the user a shortcut to always translate
  // the language, when we think the user wants that functionality.
  bool ShouldShowAlwaysTranslateShortcut() const;

  // Returns true if the UI should offer the user a shortcut to never translate
  // the language, when we think the user wants that functionality.
  bool ShouldShowNeverTranslateShortcut() const;

  // Updates metrics when a user's action closes the translate UI. This includes
  // when: the user presses the 'x' button, the user selects to never translate
  // this site, and the user selects to never translate this language.
  void OnUIClosedByUser();

  // Records a high level UI interaction.
  void ReportUIInteraction(UIInteraction ui_interaction);

  // Updates TranslateMetricsLogger state of whether Translate UI is currently
  // shown.
  void ReportUIChange(bool is_ui_shown);

  // If kContentLanguagesinLanguagePicker is on, build a vector of content
  // languages data.
  void MaybeSetContentLanguages();

  // Returns whether or not the current session is off-the-record.
  bool IsIncognito() const;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Returns whether "Always Translate Language" should automatically trigger.
  // If true, this method has the side effect of mutating some prefs.
  bool ShouldAutoAlwaysTranslate();

  // Returns whether "Never Translate Language" should automatically trigger.
  // If true, this method has the side effect of mutating some prefs.
  bool ShouldAutoNeverTranslate();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslateUIDelegateTest, GetPageHost);
  FRIEND_TEST_ALL_PREFIXES(TranslateUIDelegateTest, MaybeSetContentLanguages);

  // Gets the host of the page being translated, or an empty string if no URL is
  // associated with the current page.
  std::string GetPageHost() const;

  const TranslateDriver* GetTranslateDriver() const;

  base::WeakPtr<TranslateManager> translate_manager_;

  // Manages the Translate UI language list related APIs.
  std::unique_ptr<TranslateUILanguagesManager> translate_ui_languages_manager_;

  // The list of language codes representing translatable user's setting
  // languages. The languages are in order defined by the user.
  std::vector<std::string> translatable_content_languages_codes_;

  // Translate related preferences.
  std::unique_ptr<TranslatePrefs> prefs_;

  // Listens to accept languages changes.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_
