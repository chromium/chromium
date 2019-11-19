// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_INFOBAR_DELEGATE_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_INFOBAR_DELEGATE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_errors.h"

namespace infobars {
class InfoBarManager;
}

namespace translate {

// Feature flag used to control the auto-always and auto-never snackbar
// parameters (i.e. threshold and maximum-number-of).
extern const base::Feature kTranslateAutoSnackbars;

// Feature flag for "Translate Compact Infobar UI" project.
extern const base::Feature kTranslateCompactUI;

class TranslateDriver;
class TranslateManager;

class TranslateInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  // An observer to handle different translate steps' UI changes.
  class Observer {
   public:
    // Handles UI changes on the translate step given.
    virtual void OnTranslateStepChanged(translate::TranslateStep step,
                                        TranslateErrors::Type error_type) = 0;
    // Return whether user declined translate service.
    virtual bool IsDeclinedByUser() = 0;
    // Called when the TranslateInfoBarDelegate instance is destroyed.
    virtual void OnTranslateInfoBarDelegateDestroyed(
        TranslateInfoBarDelegate* delegate) = 0;

   protected:
    virtual ~Observer() {}
  };

  static const size_t kNoIndex;

  // Get the threshold and maximum number of occurences that parameterize
  // automatic always- and never-translate.
  static int GetAutoAlwaysThreshold();
  static int GetAutoNeverThreshold();
  static int GetMaximumNumberOfAutoAlways();
  static int GetMaximumNumberOfAutoNever();

  ~TranslateInfoBarDelegate() override;

  // Factory method to create a translate infobar.  |error_type| must be
  // specified iff |step| == TRANSLATION_ERROR.  For other translate steps,
  // |original_language| and |target_language| must be ASCII language codes
  // (e.g. "en", "fr", etc.) for languages the TranslateManager supports
  // translating.  The lone exception is when the user initiates translation
  // from the context menu, in which case it's legal to call this with
  // |step| == TRANSLATING and |original_language| == kUnknownLanguageCode.
  //
  // If |replace_existing_infobar| is true, the infobar is created and added to
  // the infobar manager, replacing any other translate infobar already present
  // there.  Otherwise, the infobar will only be added if there is no other
  // translate infobar already present.
  static void Create(bool replace_existing_infobar,
                     const base::WeakPtr<TranslateManager>& translate_manager,
                     infobars::InfoBarManager* infobar_manager,
                     bool is_off_the_record,
                     translate::TranslateStep step,
                     const std::string& original_language,
                     const std::string& target_language,
                     TranslateErrors::Type error_type,
                     bool triggered_from_menu);

  // Returns the number of languages supported.
  virtual size_t num_languages() const;

  // Returns the ISO code for the language at |index|.
  virtual std::string language_code_at(size_t index) const;

  // Returns the displayable name for the language at |index|.
  virtual base::string16 language_name_at(size_t index) const;

  translate::TranslateStep translate_step() const { return step_; }

  bool is_off_the_record() { return is_off_the_record_; }

  TranslateErrors::Type error_type() const { return error_type_; }

  std::string original_language_code() const {
    return ui_delegate_.GetOriginalLanguageCode();
  }

  virtual base::string16 original_language_name() const;

  void UpdateOriginalLanguage(const std::string& language_code);

  std::string target_language_code() const {
    return ui_delegate_.GetTargetLanguageCode();
  }

  base::string16 target_language_name() const {
    return language_name_at(ui_delegate_.GetTargetLanguageIndex());
  }

  void UpdateTargetLanguage(const std::string& language_code);

  // Returns true if the current infobar indicates an error (in which case it
  // should get a yellow background instead of a blue one).
  bool is_error() const {
    return step_ == translate::TRANSLATE_STEP_TRANSLATE_ERROR;
  }

  // Return true if the translation was triggered by a menu entry instead of
  // via an infobar/bubble or preference.
  bool triggered_from_menu() const {
    return triggered_from_menu_;
  }

  virtual void Translate();
  virtual void RevertTranslation();
  void RevertWithoutClosingInfobar();
  void ReportLanguageDetectionError();

  // Called when the user declines to translate a page, by either closing the
  // infobar or pressing the "Don't translate" button.
  virtual void TranslationDeclined();

  // Methods called by the Options menu delegate.
  virtual bool IsTranslatableLanguageByPrefs() const;
  virtual void ToggleTranslatableLanguageByPrefs();
  virtual bool IsSiteBlacklisted() const;
  virtual void ToggleSiteBlacklist();
  virtual bool ShouldAlwaysTranslate() const;
  virtual void ToggleAlwaysTranslate();

  // Methods called by the extra-buttons that can appear on the "before
  // translate" infobar (when the user has accepted/declined the translation
  // several times).
  void AlwaysTranslatePageLanguage();
  void NeverTranslatePageLanguage();

  int GetTranslationAcceptedCount();
  int GetTranslationDeniedCount();

  void ResetTranslationAcceptedCount();
  void ResetTranslationDeniedCount();

  // Returns whether "Always Translate Language" should automatically trigger.
  // If true, this method has the side effect of mutating some prefs.
  bool ShouldAutoAlwaysTranslate();
  // Returns whether "Never Translate Language" should automatically trigger.
  // If true, this method has the side effect of mutating some prefs.
  bool ShouldAutoNeverTranslate();

  int GetTranslationAutoAlwaysCount();
  int GetTranslationAutoNeverCount();

  void IncrementTranslationAutoAlwaysCount();
  void IncrementTranslationAutoNeverCount();

  // The following methods are called by the infobar that displays the status
  // while translating and also the one displaying the error message.
  base::string16 GetMessageInfoBarText();
  base::string16 GetMessageInfoBarButtonText();
  void MessageInfoBarButtonPressed();
  bool ShouldShowMessageInfoBarButton();

  // Returns true if the infobar should offer a (platform-specific) shortcut to
  // allow the user to always/never translate the language, when we think the
  // user wants that functionality.
  bool ShouldShowAlwaysTranslateShortcut();
  bool ShouldShowNeverTranslateShortcut();

#if defined(OS_IOS)
  // Shows the Infobar offering to never translate the language or the site.
  void ShowNeverTranslateInfobar();
#endif

  // Adds the strings that should be displayed in the after translate infobar to
  // |strings|. If |autodetermined_source_language| is false, the text in that
  // infobar is:
  // "The page has been translated from <lang1> to <lang2>."
  // Otherwise:
  // "The page has been translated to <lang1>."
  // Because <lang1>, or <lang1> and <lang2> are displayed in menu buttons, the
  // text is split in 2 or 3 chunks. |swap_languages| is set to true if
  // |autodetermined_source_language| is false, and <lang1> and <lang2>
  // should be inverted (some languages express the sentense as "The page has
  // been translate to <lang2> from <lang1>."). It is ignored if
  // |autodetermined_source_language| is true.
  static void GetAfterTranslateStrings(std::vector<base::string16>* strings,
                                       bool* swap_languages,
                                       bool autodetermined_source_language);

  // Gets the TranslateDriver associated with this object.
  // May return NULL if the driver has been destroyed.
  TranslateDriver* GetTranslateDriver();

  // Set a observer.
  virtual void SetObserver(Observer* observer);

  // InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  void InfoBarDismissed() override;
  TranslateInfoBarDelegate* AsTranslateInfoBarDelegate() override;

 protected:
  TranslateInfoBarDelegate(
      const base::WeakPtr<TranslateManager>& translate_manager,
      bool is_off_the_record,
      translate::TranslateStep step,
      const std::string& original_language,
      const std::string& target_language,
      TranslateErrors::Type error_type,
      bool triggered_from_menu);

 private:
  friend class TranslationInfoBarTest;
  typedef std::pair<std::string, base::string16> LanguageNamePair;

  bool is_off_the_record_;
  translate::TranslateStep step_;

  TranslateUIDelegate ui_delegate_;
  base::WeakPtr<TranslateManager> translate_manager_;

  // The error that occurred when trying to translate (NONE if no error).
  TranslateErrors::Type error_type_;

  // The translation related preferences.
  std::unique_ptr<TranslatePrefs> prefs_;

  // Whether the translation was triggered via a menu click vs automatically
  // (due to language detection, preferences...)
  bool triggered_from_menu_;

  // A observer to handle front-end changes on different steps.
  // It's only used when we try to reuse the existing UI.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarDelegate);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_INFOBAR_DELEGATE_H_
