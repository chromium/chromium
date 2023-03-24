// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"

namespace translate {
class TranslateUIDelegate;
class TranslateUILanguagesManager;
}

// The standard implementation of TranslateBubbleModel.
class TranslateBubbleModelImpl : public TranslateBubbleModel {
 public:
  TranslateBubbleModelImpl(
      translate::TranslateStep step,
      std::unique_ptr<translate::TranslateUIDelegate> ui_delegate);

  TranslateBubbleModelImpl(const TranslateBubbleModelImpl&) = delete;
  TranslateBubbleModelImpl& operator=(const TranslateBubbleModelImpl&) = delete;

  ~TranslateBubbleModelImpl() override;

  // Converts a TranslateStep to a ViewState.
  // This function never returns VIEW_STATE_ADVANCED.
  static TranslateBubbleModel::ViewState TranslateStepToViewState(
      translate::TranslateStep step);

  // TranslateBubbleModel methods.
  TranslateBubbleModel::ViewState GetViewState() const override;
  void SetViewState(TranslateBubbleModel::ViewState view_state) override;
  void ShowError(translate::TranslateErrors error_type) override;
  int GetNumberOfSourceLanguages() const override;
  int GetNumberOfTargetLanguages() const override;
  std::u16string GetSourceLanguageNameAt(int index) const override;
  std::u16string GetTargetLanguageNameAt(int index) const override;
  std::string GetSourceLanguageCode() const override;
  int GetSourceLanguageIndex() const override;
  void UpdateSourceLanguageIndex(int index) override;
  int GetTargetLanguageIndex() const override;
  void UpdateTargetLanguageIndex(int index) override;
  void DeclineTranslation() override;
  bool ShouldNeverTranslateLanguage() override;
  void SetNeverTranslateLanguage(bool value) override;
  bool ShouldNeverTranslateSite() override;
  void SetNeverTranslateSite(bool value) override;
  bool ShouldAlwaysTranslate() const override;
  bool ShouldAlwaysTranslateBeCheckedByDefault() const override;
  bool ShouldShowAlwaysTranslateShortcut() const override;
  void SetAlwaysTranslate(bool value) override;
  void Translate() override;
  void RevertTranslation() override;
  void OnBubbleClosing() override;
  bool IsPageTranslatedInCurrentLanguages() const override;
  bool CanAddSiteToNeverPromptList() override;
  void ReportUIInteraction(translate::UIInteraction ui_interaction) override;
  void ReportUIChange(bool is_ui_shown) override;

 private:
  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate_;
  raw_ptr<translate::TranslateUILanguagesManager> ui_languages_manager_;
  ViewState current_view_state_;

  bool translation_declined_;
  bool translate_executed_;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_IMPL_H_
