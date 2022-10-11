// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_H_

#include <string>

#include "base/observer_list_types.h"
#include "chrome/browser/ui/translate/translate_language_list_model.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents.h"

// The model for the Partial Translate bubble UX. This manages the user's
// manipulation of the bubble and offers the data to show on the bubble.
class PartialTranslateBubbleModel : public TranslateLanguageListModel {
 public:
  enum ViewState {
    // The view state while waiting for translation.
    VIEW_STATE_WAITING,

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

  ~PartialTranslateBubbleModel() override = default;

  // Allows clients to be notified about translate status changes (e.g. for view
  // updates).
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPartialTranslateComplete() = 0;
  };
  virtual void AddObserver(Observer* obs) = 0;
  virtual void RemoveObserver(Observer* obs) = 0;

  // Returns the current view state.
  virtual ViewState GetViewState() const = 0;

  // Transitions the view state.
  virtual void SetViewState(ViewState view_state) = 0;

  // Sets the source language.
  virtual void SetSourceLanguage(const std::string& language_code) = 0;
  // Sets the target language.
  virtual void SetTargetLanguage(const std::string& language_code) = 0;
  // Sets the source (selected) text that will be translated.
  virtual void SetSourceText(const std::u16string& text) = 0;
  // Returns the source (selected) text that will be translated.
  virtual std::u16string GetSourceText() const = 0;
  // Sets the target (translated) text.
  virtual void SetTargetText(const std::u16string& text) = 0;
  // Returns the target (translated) text.
  virtual std::u16string GetTargetText() const = 0;

  // Shows an error.
  virtual void SetError(translate::TranslateErrors error_type) = 0;
  virtual translate::TranslateErrors GetError() const = 0;

  // TranslateLanguageListModel:
  int GetNumberOfSourceLanguages() const override = 0;
  int GetNumberOfTargetLanguages() const override = 0;
  std::u16string GetSourceLanguageNameAt(int index) const override = 0;
  std::u16string GetTargetLanguageNameAt(int index) const override = 0;
  int GetSourceLanguageIndex() const override = 0;
  void UpdateSourceLanguageIndex(int index) override = 0;
  int GetTargetLanguageIndex() const override = 0;
  void UpdateTargetLanguageIndex(int index) override = 0;

  // Returns the source and target language codes.
  virtual std::string GetSourceLanguageCode() const = 0;
  virtual std::string GetTargetLanguageCode() const = 0;

  // Convenience methods for getting the source and target language names.
  std::u16string GetSourceLanguageName() const {
    return GetSourceLanguageNameAt(GetSourceLanguageIndex());
  }
  std::u16string GetTargetLanguageName() const {
    return GetTargetLanguageNameAt(GetTargetLanguageIndex());
  }

  // Starts translating the selected text. Clients will be notified of
  // completion via Observer::OnPartialTranslateCompleted.
  virtual void Translate(content::WebContents* web_contents) = 0;

  // Closes the Partial Translate bubble, then immediately opens the Full Page
  // Translate bubble and starts a translation.
  virtual void TranslateFullPage(content::WebContents* web_contents) = 0;

  // Set whether the selected text is truncated.
  virtual void SetSourceTextTruncated(bool is_truncated) = 0;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_H_
