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

  PartialTranslateBubbleModel(
      ViewState view_state,
      std::unique_ptr<translate::TranslateUIDelegate> ui_delegate);

  PartialTranslateBubbleModel(const PartialTranslateBubbleModel&) = delete;
  PartialTranslateBubbleModel& operator=(const PartialTranslateBubbleModel&) =
      delete;

  ~PartialTranslateBubbleModel() override;

  // Returns the current view state.
  ViewState GetViewState() const;

  // Transitions the view state.
  void SetViewState(ViewState view_state);

  // Shows an error.
  void ShowError(translate::TranslateErrors::Type error_type);

  // Goes back from the 'Advanced' view state.
  void GoBackFromAdvanced();

  // TranslateLanguageListModel:
  int GetNumberOfSourceLanguages() const override;
  int GetNumberOfTargetLanguages() const override;
  std::u16string GetSourceLanguageNameAt(int index) const override;
  std::u16string GetTargetLanguageNameAt(int index) const override;
  int GetSourceLanguageIndex() const override;
  void UpdateSourceLanguageIndex(int index) override;
  int GetTargetLanguageIndex() const override;
  void UpdateTargetLanguageIndex(int index) override;

  // Starts translating the selected text.
  void Translate();

  // Reverts translation.
  void RevertTranslation();

  // Returns true if the current text selection is translated in the currently
  // selected source and target language.
  bool IsCurrentSelectionTranslated() const;

 private:
  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate_;

  // The current view type.
  ViewState current_view_state_;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_H_
