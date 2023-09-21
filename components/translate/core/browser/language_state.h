// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_LANGUAGE_STATE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_LANGUAGE_STATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/language/core/common/language_util.h"
#include "components/translate/core/browser/translate_metrics_logger.h"

namespace translate {

class TranslateDriver;

// This class holds the language state of the current page.
// There is one LanguageState instance per tab.
// It is used to determine when navigating to a new page whether it should
// automatically be translated.
// This auto-translate behavior is the expected behavior when:
// - user is on page in language A that they had translated to language B.
// - user clicks a link in that page that takes them to a page also in language
//   A.
class LanguageState {
 public:
  explicit LanguageState(TranslateDriver* driver);

  LanguageState(const LanguageState&) = delete;
  LanguageState& operator=(const LanguageState&) = delete;

  ~LanguageState();

  // Should be called when the page did a new navigation (whether it is a main
  // frame or sub-frame navigation).
  void DidNavigate(bool is_same_document_navigation,
                   bool is_main_frame,
                   bool reload,
                   const std::string& href_translate,
                   bool navigation_from_google);

  // Should be called when the language of the page has been determined.
  // |page_level_translation_criteria_met| when false indicates that the browser
  // should not offer to translate the page.
  void LanguageDetermined(const std::string& page_language,
                          bool page_level_translation_criteria_met);

  // Returns the language the current page should be translated to, based on the
  // previous page languages and the transition.  This should be called after
  // the language page has been determined.
  // Returns an empty string if the page should not be auto-translated.
  std::string AutoTranslateTo() const;

  // Returns true if the user is navigating through translated links.
  bool InTranslateNavigation() const;

  // Returns true if the current page in the associated tab has been translated.
  bool IsPageTranslated() const { return source_lang_ != current_lang_; }

  // Returns the source language represented as a lowercase alphabetic string
  // of length 0 to 3 or "zh-CN" or "zh-TW".
  const std::string& source_language() const { return source_lang_; }
  void SetSourceLanguage(const std::string& language);

  // Returns the current language represented as a lowercase alphabetic string
  // of length 0 to 3 or "zh-CN" or "zh-TW".
  const std::string& current_language() const { return current_lang_; }
  void SetCurrentLanguage(const std::string& language);

  bool page_level_translation_criteria_met() const {
    return page_level_translation_criteria_met_;
  }

  // Whether the page is currently in the process of being translated.
  bool translation_pending() const { return translation_pending_; }
  void set_translation_pending(bool value) { translation_pending_ = value; }

  // Whether an error occured during translation.
  bool translation_error() const { return translation_error_; }
  void set_translation_error(bool value) { translation_error_ = value; }

  // Whether the user has already declined to translate the page.
  bool translation_declined() const { return translation_declined_; }
  void set_translation_declined(bool value) { translation_declined_ = value; }

  // Whether the translate is enabled.
  bool translate_enabled() const { return translate_enabled_; }
  void SetTranslateEnabled(bool value);

  // The type of the last translation that was initiated, if any.
  TranslationType translation_type() const { return translation_type_; }
  void SetTranslationType(TranslationType type) { translation_type_ = type; }

  // Whether the current page's language is different from the previous
  // language.
  bool HasLanguageChanged() const;

  const std::string& href_translate() const { return href_translate_; }
  bool navigation_from_google() const { return navigation_from_google_; }

  const std::string& GetPredefinedTargetLanguage() const {
    return predefined_target_language_;
  }
  void SetPredefinedTargetLanguage(const std::string& language,
                                   bool should_auto_translate) {
    predefined_target_language_ = language;
    language::ToTranslateLanguageSynonym(&predefined_target_language_);
    should_auto_translate_to_predefined_target_language_ =
        should_auto_translate;
  }

  bool should_auto_translate_to_predefined_target_language() const {
    return should_auto_translate_to_predefined_target_language_;
  }

 private:
  void SetIsPageTranslated(bool value);

  // Whether the page is translated or not.
  bool is_page_translated_;

  // The languages this page is in. Note that current_lang_ is different from
  // source_lang_ when the page has been translated.
  // Note that these might be empty if the page language has not been determined
  // yet.
  std::string source_lang_;
  std::string current_lang_;

  // Same as above but for the previous page.
  std::string prev_source_lang_;
  std::string prev_current_lang_;

  // Provides driver-level context to the shared code of the component. Must
  // outlive this object.
  raw_ptr<TranslateDriver> translate_driver_;

  // Whether it is OK to offer to translate the page. Translation is not offered
  // if we cannot determine the source language. In addition, some pages
  // explicitly specify that they should not be translated by the browser (this
  // is the case for GMail for example, which provides its own translation
  // features).
  bool page_level_translation_criteria_met_;

  // Whether a translation is currently pending.
  // This is needed to avoid sending duplicate translate requests to a page.
  // Translations may be initiated every time the load stops for the main frame,
  // which may happen several times.
  // TODO(jcampan): make the client send the language just once per navigation
  //                then we can get rid of that state.
  bool translation_pending_;

  // Whether an error occured during translation.
  bool translation_error_;

  // Whether the user has declined to translate the page (by closing the infobar
  // for example).  This is necessary as a new infobar could be shown if a new
  // load happens in the page after the user closed the infobar.
  bool translation_declined_;

  // Whether the current navigation is a same-document navigation.
  bool is_same_document_navigation_;

  // Whether the Translate is enabled.
  bool translate_enabled_;

  // The type of the current translation or uninitialized if there is none.
  TranslationType translation_type_;

  // The value of the hrefTranslate attribute on the link that initiated the
  // current navigation, if it was specified.
  std::string href_translate_;

  // True when the current page was the result of a navigation originated in a
  // Google origin.
  bool navigation_from_google_ = false;

  // Target language set by client.
  std::string predefined_target_language_;

  // Indicates that the page should be automatically translated to
  // |predefined_target_language_| if possible.
  bool should_auto_translate_to_predefined_target_language_ = false;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_LANGUAGE_STATE_H_
