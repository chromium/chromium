// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_H_

#include <string>

#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/translate_language_list_model.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents.h"

// The model for the Partial Translate bubble UX. This manages the user's
// manipulation of the bubble and offers the data to show on the bubble.
// TODO(crbug/1314825): When the PartialTranslateManager is added it
// will replace and take the role of the TranslateUIDelegate.
class PartialTranslateBubbleModel : public TranslateLanguageListModel {
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

  ~PartialTranslateBubbleModel() override = default;

  // Returns the current view state.
  virtual ViewState GetViewState() const = 0;

  // Transitions the view state.
  virtual void SetViewState(ViewState view_state) = 0;

  // Shows an error.
  virtual void ShowError(translate::TranslateErrors::Type error_type) = 0;

  // TranslateLanguageListModel:
  int GetNumberOfSourceLanguages() const override = 0;
  int GetNumberOfTargetLanguages() const override = 0;
  std::u16string GetSourceLanguageNameAt(int index) const override = 0;
  std::u16string GetTargetLanguageNameAt(int index) const override = 0;
  int GetSourceLanguageIndex() const override = 0;
  void UpdateSourceLanguageIndex(int index) override = 0;
  int GetTargetLanguageIndex() const override = 0;
  void UpdateTargetLanguageIndex(int index) override = 0;

  // Starts translating the selected text.
  virtual void Translate() = 0;

  // Reverts translation.
  virtual void RevertTranslation() = 0;

  // Returns true if the current text selection is translated in the currently
  // selected source and target language.
  virtual bool IsCurrentSelectionTranslated() const = 0;

  // Closes the Partial Translate bubble, then immediately opens the Full Page
  // Translate bubble and starts a translation.
  virtual void TranslateFullPage(content::WebContents* web_contents) = 0;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_H_
