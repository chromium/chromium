// Copyright 2014 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/browser/translate_ui_languages_manager.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_errors.h"

namespace infobars {
class InfoBarManager;
}

namespace translate {

// Feature flag for "Translate Compact Infobar UI" project.
BASE_DECLARE_FEATURE(kTranslateCompactUI);

class TranslateDriver;
class TranslateManager;
class TranslateUILanguagesManager;

class TranslateInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  // An observer to handle different translate steps' UI changes.
  class Observer : public base::CheckedObserver {
   public:
    // Handles UI changes on the translate step given.
    virtual void OnTranslateStepChanged(translate::TranslateStep step,
                                        TranslateErrors error_type) = 0;
    // Handles UI changes when the target language is updated.
    virtual void OnTargetLanguageChanged(
        const std::string& target_language_code) = 0;
    // Return whether user declined translate service.
    virtual bool IsDeclinedByUser() = 0;
    // Called when the TranslateInfoBarDelegate instance is destroyed.
    virtual void OnTranslateInfoBarDelegateDestroyed(
        TranslateInfoBarDelegate* delegate) = 0;
  };

  static const size_t kNoIndex;

  TranslateInfoBarDelegate(const TranslateInfoBarDelegate&) = delete;
  TranslateInfoBarDelegate& operator=(const TranslateInfoBarDelegate&) = delete;

  ~TranslateInfoBarDelegate() override;

  // Factory method to create a translate infobar.  |error_type| must be
  // specified iff |step| == TRANSLATION_ERROR.  For other translate steps,
  // |source_language| and |target_language| must be ASCII language codes
  // (e.g. "en", "fr", etc.) for languages the TranslateManager supports
  // translating.  The lone exception is when the user initiates translation
  // from the context menu, in which case it's legal to call this with
  // |step| == TRANSLATING and |source_language| == kUnknownLanguageCode.
  //
  // If |replace_existing_infobar| is true, the infobar is created and added to
  // the infobar manager, replacing any other translate infobar already present
  // there.  Otherwise, the infobar will only be added if there is no other
  // translate infobar already present.
  static void Create(bool replace_existing_infobar,
                     const base::WeakPtr<TranslateManager>& translate_manager,
                     infobars::InfoBarManager* infobar_manager,
                     translate::TranslateStep step,
                     const std::string& source_language,
                     const std::string& target_language,
                     TranslateErrors error_type,
                     bool triggered_from_menu);

  // Returns the number of languages supported.
  virtual size_t num_languages() const;

  // Returns the ISO code for the language at |index|.
  virtual std::string language_code_at(size_t index) const;

  // Returns the displayable name for the language at |index|.
  virtual std::u16string language_name_at(size_t index) const;

  translate::TranslateStep translate_step() const { return step_; }

  TranslateErrors error_type() const { return error_type_; }

  std::string source_language_code() const {
    return ui_languages_manager_->GetSourceLanguageCode();
  }

  virtual std::u16string source_language_name() const;

  virtual std::u16string initial_source_language_name() const;

  virtual std::u16string unknown_language_name() const;

  virtual void UpdateSourceLanguage(const std::string& language_code);

  std::string target_language_code() const {
    return ui_languages_manager_->GetTargetLanguageCode();
  }

  virtual std::u16string target_language_name() const;

  virtual void UpdateTargetLanguage(const std::string& language_code);

  // Returns true if the current infobar indicates an error (in which case it
  // should get a yellow background instead of a blue one).
  bool is_error() const {
    return step_ == translate::TRANSLATE_STEP_TRANSLATE_ERROR;
  }

  // Return true if the translation was triggered by a menu entry instead of
  // via an infobar/bubble or preference.
  bool triggered_from_menu() const { return triggered_from_menu_; }

  virtual void Translate();
  virtual void RevertTranslation();
  virtual void RevertWithoutClosingInfobar();

  // Called when the user declines to translate a page, by either closing the
  // infobar or pressing the "Don't translate" button.
  virtual void TranslationDeclined();

  // Methods called by the Options menu delegate.
  virtual bool IsTranslatableLanguageByPrefs() const;
  virtual void ToggleTranslatableLanguageByPrefs();
  virtual bool IsSiteOnNeverPromptList() const;
  virtual void ToggleNeverPromptSite();
  virtual bool ShouldAlwaysTranslate() const;
  virtual void ToggleAlwaysTranslate();

  // Returns whether "Always Translate Language" should automatically trigger.
  // If true, this method has the side effect of mutating some prefs.
  bool ShouldAutoAlwaysTranslate();
  // Returns whether "Never Translate Language" should automatically trigger.
  // If true, this method has the side effect of mutating some prefs.
  bool ShouldAutoNeverTranslate();

#if BUILDFLAG(IS_IOS)
  // Shows the Infobar offering to never translate the language or the site.
  void ShowNeverTranslateInfobar();
#endif
  // Gets the TranslateDriver associated with this object.
  // May return NULL if the driver has been destroyed.
  TranslateDriver* GetTranslateDriver();

  // Add an observer.
  virtual void AddObserver(Observer* observer);

  // Remove an observer.
  virtual void RemoveObserver(Observer* observer);

  // Handles when the user closes the translate infobar. This includes when: the
  // user presses the 'x' button, the user selects to never translate the site,
  // and the user selects to never translate the language.
  void OnInfoBarClosedByUser();

  // Records a high level UI interaction.
  void ReportUIInteraction(UIInteraction ui_interaction);

  // InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  void InfoBarDismissed() override;
#if BUILDFLAG(IS_IOS)
  TranslateInfoBarDelegate* AsTranslateInfoBarDelegate() override;
#endif

 protected:
  TranslateInfoBarDelegate(
      const base::WeakPtr<TranslateManager>& translate_manager,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      TranslateErrors error_type,
      bool triggered_from_menu);

 private:
  friend class TranslateInfoBarDelegateTest;
  typedef std::pair<std::string, std::u16string> LanguageNamePair;

  translate::TranslateStep step_;

  TranslateUIDelegate ui_delegate_;
  base::WeakPtr<TranslateManager> translate_manager_;
  raw_ptr<TranslateUILanguagesManager> ui_languages_manager_;

  // The error that occurred when trying to translate (NONE if no error).
  TranslateErrors error_type_;

  // The translation related preferences.
  std::unique_ptr<TranslatePrefs> prefs_;

  // Whether the translation was triggered via a menu click vs automatically
  // (due to language detection, preferences...)
  bool triggered_from_menu_;

  // Observers to handle front-end changes on different steps.
  // It's only used when we try to reuse the existing UI.
  base::ObserverList<Observer> observers_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_INFOBAR_DELEGATE_H_
