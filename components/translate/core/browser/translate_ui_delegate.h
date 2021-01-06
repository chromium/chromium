// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/common/translate_errors.h"

namespace translate {

class LanguageState;
class TranslateDriver;
class TranslateManager;
class TranslatePrefs;

// The TranslateUIDelegate is a generic delegate for UI which offers Translate
// feature to the user.

// Note that the API offers a way to read/set language values through array
// indices. Such indices are only valid as long as the visual representation
// (infobar, bubble...) is in sync with the underlying language list which
// can actually change at run time (see translate_language_list.h).
// It is recommended that languages are only updated by language code to
// avoid bugs like crbug.com/555124

class TranslateUIDelegate {
 public:
  static const size_t kNoIndex = static_cast<size_t>(-1);

  TranslateUIDelegate(const base::WeakPtr<TranslateManager>& translate_manager,
                      const std::string& original_language,
                      const std::string& target_language);
  virtual ~TranslateUIDelegate();

  // Handles when an error message is shown.
  void OnErrorShown(TranslateErrors::Type error_type);

  // Returns the LanguageState associated with this object.
  const LanguageState& GetLanguageState();

  // Returns the number of languages supported.
  size_t GetNumberOfLanguages() const;

  // Returns the original language index.
  size_t GetOriginalLanguageIndex() const { return original_language_index_; }

  // Returns the original language code.
  std::string GetOriginalLanguageCode() const;

  // Updates the original language index.
  void UpdateOriginalLanguageIndex(size_t language_index);

  void UpdateOriginalLanguage(const std::string& language_code);

  // Returns the target language index.
  size_t GetTargetLanguageIndex() const { return target_language_index_; }

  // Returns the target language code.
  std::string GetTargetLanguageCode() const;

  // Updates the target language index.
  void UpdateTargetLanguageIndex(size_t language_index);

  void UpdateTargetLanguage(const std::string& language_code);

  // Returns the ISO code for the language at |index|.
  std::string GetLanguageCodeAt(size_t index) const;

  // Returns the displayable name for the language at |index|.
  base::string16 GetLanguageNameAt(size_t index) const;

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
  bool CanAddToNeverPromptList() const;

  // Sets the never-prompt state for the host of the current page. If
  // value is true, the current host will be blocklisted and translation
  // prompts will not show for that site.
  void SetNeverPrompt(bool value);

  // Returns true if the webpage in the current original language should be
  // translated into the current target language automatically.
  bool ShouldAlwaysTranslate() const;

  // Sets the value if the webpage in the current original language should be
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

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslateUIDelegateTest, GetPageHost);

  // Gets the host of the page being translated, or an empty string if no URL is
  // associated with the current page.
  std::string GetPageHost() const;

  TranslateDriver* translate_driver_;
  base::WeakPtr<TranslateManager> translate_manager_;

  // ISO code (en, fr...) -> displayable name in the current locale
  typedef std::pair<std::string, base::string16> LanguageNamePair;

  // The list supported languages for translation.
  // The languages are sorted alphabetically based on the displayable name.
  std::vector<LanguageNamePair> languages_;

  // The index for language the page is originally in.
  size_t original_language_index_;

  // The index for language the page is originally in that was originally
  // reported (original_language_index_ changes if the user selects a new
  // original language, but this one does not).  This is necessary to report
  // language detection errors with the right original language even if the user
  // changed the original language.
  size_t initial_original_language_index_;

  // The index for language the page should be translated to.
  size_t target_language_index_;

  // The translation related preferences.
  std::unique_ptr<TranslatePrefs> prefs_;

  DISALLOW_COPY_AND_ASSIGN(TranslateUIDelegate);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_UI_DELEGATE_H_
