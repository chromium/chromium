// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_IMPL_H_

#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"

class PartialTranslateBubbleModelImpl : public PartialTranslateBubbleModel {
 public:
  PartialTranslateBubbleModelImpl(
      ViewState view_state,
      std::unique_ptr<translate::TranslateUIDelegate> ui_delegate);

  PartialTranslateBubbleModelImpl(const PartialTranslateBubbleModelImpl&) =
      delete;
  PartialTranslateBubbleModelImpl& operator=(
      const PartialTranslateBubbleModelImpl&) = delete;

  ~PartialTranslateBubbleModelImpl() override;

  // PartialTranslateBubbleModel methods:
  ViewState GetViewState() const override;
  void SetViewState(ViewState view_state) override;
  void ShowError(translate::TranslateErrors::Type error_type) override;
  int GetNumberOfSourceLanguages() const override;
  int GetNumberOfTargetLanguages() const override;
  std::u16string GetSourceLanguageNameAt(int index) const override;
  std::u16string GetTargetLanguageNameAt(int index) const override;
  int GetSourceLanguageIndex() const override;
  void UpdateSourceLanguageIndex(int index) override;
  int GetTargetLanguageIndex() const override;
  void UpdateTargetLanguageIndex(int index) override;
  void Translate() override;
  void RevertTranslation() override;
  bool IsCurrentSelectionTranslated() const override;
  void TranslateFullPage(content::WebContents* web_contents) override;

 private:
  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate_;

  // The current view type.
  ViewState current_view_state_;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_IMPL_H_
