// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_

#include <string>

#include "components/translate/core/browser/translate_metrics_logger_impl.h"
#include "components/translate/core/common/translate_errors.h"

// The model for the Translate bubble UX. This manages the user's manipulation
// of the bubble and offers the data to show on the bubble.
class TranslateBubbleModel {
 public:
  enum ViewState {
    // The view state before translating.
    VIEW_STATE_BEFORE_TRANSLATE,

    // The view state during translating.
    VIEW_STATE_TRANSLATING,

    // The view state after translating.
    VIEW_STATE_AFTER_TRANSLATE,

    // The view state when an error of Translate happens.
    VIEW_STATE_ERROR,

    // The view state for when the source language combobox is shown. This view
    // appears when the user selects "Page is not in {source language}" under
    // option menu.
    VIEW_STATE_SOURCE_LANGUAGE,

    // The view state for when the source language combobox is shown. This view
    // appears when the user selects "More options..." under option menu.
    VIEW_STATE_TARGET_LANGUAGE
  };

  virtual ~TranslateBubbleModel() {}

  // Returns the current view state.
  virtual ViewState GetViewState() const = 0;

  // Transitions the view state.
  virtual void SetViewState(ViewState view_state) = 0;

  // Shows an error.
  virtual void ShowError(translate::TranslateErrors::Type error_type) = 0;

  // Goes back from the 'Advanced' view state.
  virtual void GoBackFromAdvanced() = 0;

  // Returns the number of source languages supported.
  virtual int GetNumberOfSourceLanguages() const = 0;

  // Returns the number of target languages supported.
  virtual int GetNumberOfTargetLanguages() const = 0;

  // Returns the displayable name for the source language at |index|.
  virtual std::u16string GetSourceLanguageNameAt(int index) const = 0;

  // Returns the displayable name for the target language at |index|.
  virtual std::u16string GetTargetLanguageNameAt(int index) const = 0;

  // Returns the source language code.
  virtual std::string GetSourceLanguageCode() const = 0;

  // Returns the source language index.
  virtual int GetSourceLanguageIndex() const = 0;

  // Updates the source language index.
  virtual void UpdateSourceLanguageIndex(int index) = 0;

  // Returns the target language index.
  virtual int GetTargetLanguageIndex() const = 0;

  // Updates the target language index.
  virtual void UpdateTargetLanguageIndex(int index) = 0;

  // Invoked when the user actively declines to translate the page - e.g.
  // selects 'nope', 'never translate this language', etc.
  // Should not be invoked on a passive decline - i.e. if the translate bubble
  // is closed due to focus loss.
  virtual void DeclineTranslation() = 0;

  // Returns if the user doesn't want to have the page translated in the
  // current page's language.
  virtual bool ShouldNeverTranslateLanguage() = 0;

  // Sets the value if the user doesn't want to have the page translated in the
  // current page's language.
  virtual void SetNeverTranslateLanguage(bool value) = 0;

  // Returns if the user doesn't want to have the page translated the
  // current page's domain.
  virtual bool ShouldNeverTranslateSite() = 0;

  // Sets the value if the user doesn't want to have the page translated the
  // current page's domain.
  virtual void SetNeverTranslateSite(bool value) = 0;

  // Returns true if the webpage in the current source language should be
  // translated into the current target language automatically.
  virtual bool ShouldAlwaysTranslate() const = 0;

  // Returns true if the Always Translate checkbox should be checked by default.
  virtual bool ShouldAlwaysTranslateBeCheckedByDefault() const = 0;

  // Returns true if the Always Translate checkbox should be shown on the
  // initial translation prompt, when we think the user wants that
  // functionality.
  virtual bool ShouldShowAlwaysTranslateShortcut() const = 0;

  // Sets the value if the webpage in the current source language should be
  // translated into the current target language automatically.
  virtual void SetAlwaysTranslate(bool value) = 0;

  // Starts translating the current page.
  virtual void Translate() = 0;

  // Reverts translation.
  virtual void RevertTranslation() = 0;

  // Called when the translate bubble is closed. Allows final cleanup and
  // notification of delegates.
  virtual void OnBubbleClosing() = 0;

  // Returns true if the page is translated in the currently selected source
  // and target language.
  virtual bool IsPageTranslatedInCurrentLanguages() const = 0;

  // True if the site of the current page can be blocklisted.
  virtual bool CanBlocklistSite() = 0;

  // Reports a high level UI interaction to the centralzied
  // TranslateMetricsLogger.
  virtual void ReportUIInteraction(translate::UIInteraction ui_interaction) = 0;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_
