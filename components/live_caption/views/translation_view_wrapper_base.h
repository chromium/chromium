// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_VIEWS_TRANSLATION_VIEW_WRAPPER_BASE_H_
#define COMPONENTS_LIVE_CAPTION_VIEWS_TRANSLATION_VIEW_WRAPPER_BASE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace gfx {
class FontList;
}  // namespace gfx

namespace translate {
class TranslateUILanguagesManager;
}  // namespace translate

namespace views {
class ImageView;
class Label;
class MdTextButton;
class View;
class MenuRunner;

}  // namespace views

namespace captions {

class LanguageTextButton;
class LanguageDropdownButton;

class TranslationViewWrapperBase : public ui::SimpleMenuModel::Delegate,
                                   public CaptionBubbleSettings::Observer {
 public:
  class Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual void OnLanguageChanged(const std::string& display_language) = 0;
    virtual void UpdateLanguageDirection(
        const std::string& display_language) = 0;
    virtual void CaptionSettingsButtonPressed() = 0;

   protected:
    Delegate() = default;
  };

  TranslationViewWrapperBase(const TranslationViewWrapperBase&) = delete;
  TranslationViewWrapperBase& operator=(const TranslationViewWrapperBase&) =
      delete;

  ~TranslationViewWrapperBase() override;

  void Init(views::View* translate_container, Delegate* delegate);

  std::vector<raw_ptr<views::View, VectorExperimental>> GetButtons();

  void SetTextSizeAndFontFamily(double text_scale_factor,
                                const gfx::FontList& font_list);

  void SetTextColor(SkColor language_label_color,
                    SkColor language_label_border_color,
                    SkColor header_color);

  void UpdateLanguageLabel();

  void OnAutoDetectedLanguageChanged(std::string auto_detected_language_code);

  void UpdateContentSize();

  views::Label* GetSourceLanguageLabelForTesting();
  views::Label* GetTargetLanguageLabelForTesting();
  views::MdTextButton* GetSourceLanguageButtonForTesting();
  views::MdTextButton* GetTargetLanguageButtonForTesting();
  views::View* GetTranslateIconAndTextForTesting();
  views::View* GetTranslateArrowIconForTesting();
  void SetTargetLanguageForTesting(const std::string& language_code);

 protected:
  TranslationViewWrapperBase();

  virtual CaptionBubbleSettings* caption_bubble_settings() = 0;

 private:
  void SetTranslationsViewVisible(bool live_translate_enabled);

  // ui::SimpleMenuModelDelegate:
  void ExecuteCommand(int target_language_code_index, int event_flags) override;
  bool IsCommandIdChecked(int target_language_code_index) const override;

  // CaptionBubbleSettings::Observer:
  void OnLiveTranslateEnabledChanged() override;
  void OnLiveCaptionLanguageChanged() override;
  void OnLiveTranslateTargetLanguageChanged() override;

  void OnSourceLanguageButtonPressed();

  void OnTargetLanguageButtonPressed();

  std::string GetDisplayLanguage();
  std::string GetSourceLanguageCode() const;
  std::string GetTargetLanguageCode() const;
  bool SourceAndTargetLanguageCodeMatch();

  std::u16string GetSourceLanguageName() const;
  std::u16string GetTargetLanguageName() const;

  raw_ptr<Delegate> delegate_;

  // Manages the Translate UI language list related APIs.
  std::unique_ptr<translate::TranslateUILanguagesManager>
      translate_ui_languages_manager_;

  raw_ptr<views::Label> translation_header_text_;
  raw_ptr<LanguageTextButton> source_language_button_;
  raw_ptr<LanguageDropdownButton> target_language_button_;
  raw_ptr<views::ImageView> translate_icon_;
  raw_ptr<views::View> translate_indicator_container_;
  std::u16string source_language_text_;
  std::u16string target_language_text_;
  raw_ptr<views::ImageView> translate_arrow_icon_;

  // Flag indicating whether the current source language does not match the user
  // preference source language.
  bool auto_detected_source_language_ = false;

  std::unique_ptr<ui::SimpleMenuModel> translation_menu_model_;
  std::unique_ptr<views::MenuRunner> translation_menu_runner_;

  base::WeakPtrFactory<TranslationViewWrapperBase> weak_ptr_factory_{this};
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_VIEWS_TRANSLATION_VIEW_WRAPPER_BASE_H_
