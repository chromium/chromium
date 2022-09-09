// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_

#include <string>

#include "chrome/browser/ui/translate/translate_language_list_model.h"
#include "components/translate/core/browser/translate_metrics_logger_impl.h"
#include "components/translate/core/common/translate_errors.h"

// The model for the Full Page Translate bubble UX. This manages the user's
// manipulation of the bubble and offers the data to show on the bubble.
class TranslateBubbleModel : public TranslateLanguageListModel {
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
    // the options menu.
    VIEW_STATE_SOURCE_LANGUAGE,

    // The view state for when the target language combobox is shown. This view
    // appears when the user selects "Choose another language..." under the
    // options menu.
    VIEW_STATE_TARGET_LANGUAGE
  };

  ~TranslateBubbleModel() override = default;

  // Returns the current view state.
  virtual ViewState GetViewState() const = 0;

  // Transitions the view state.
  virtual void SetViewState(ViewState view_state) = 0;

  // Shows an error.
  virtual void ShowError(translate::TranslateErrors error_type) = 0;

  // TranslateLanguageListModel:
  int GetNumberOfSourceLanguages() const override = 0;
  int GetNumberOfTargetLanguages() const override = 0;
  std::u16string GetSourceLanguageNameAt(int index) const override = 0;
  std::u16string GetTargetLanguageNameAt(int index) const override = 0;
  int GetSourceLanguageIndex() const override = 0;
  void UpdateSourceLanguageIndex(int index) override = 0;
  int GetTargetLanguageIndex() const override = 0;
  void UpdateTargetLanguageIndex(int index) override = 0;

  // Returns the source language code.
  virtual std::string GetSourceLanguageCode() const = 0;

  // Invoked when the user actively declines to translate the page - e.g.
  // selects 'nope', 'never translate this language', etc.
  // Should not be invoked on a passive decline - i.e. if the bubble is closed
  // due to focus loss.
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

  // Called when the bubble is closed. Allows final cleanup
  // and notification of delegates.
  virtual void OnBubbleClosing() = 0;

  // Returns true if the page is translated in the currently selected source
  // and target language.
  virtual bool IsPageTranslatedInCurrentLanguages() const = 0;

  // True if the site of the current page can be blocklisted.
  virtual bool CanAddSiteToNeverPromptList() = 0;

  // Reports a high level UI interaction to the centralzied
  // TranslateMetricsLogger.
  virtual void ReportUIInteraction(translate::UIInteraction ui_interaction) = 0;

  // Updates TranslateMetricsLogger state of whether Translate UI is currently
  // shown.
  virtual void ReportUIChange(bool is_ui_shown) = 0;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_
