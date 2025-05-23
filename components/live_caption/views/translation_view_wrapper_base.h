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
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/md_text_button_with_down_arrow.h"
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
class View;
class MenuRunner;

}  // namespace views

namespace captions {

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

  class LanguageButton {
   public:
    LanguageButton() = default;

    LanguageButton(const LanguageButton&) = delete;
    LanguageButton& operator=(const LanguageButton&) = delete;

    virtual ~LanguageButton() = default;

    virtual views::MdTextButton* GetMdTextButton() = 0;
    virtual views::Label* GetLabel() = 0;
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

  virtual void MaybeAddChildViews(views::View* translate_container);

  virtual void UpdateLanguageLabelInternal();

  virtual void SetTranslationsViewVisible(bool live_translate_enabled);

  int AddLanguageTextButton(views::View* translate_container,
                            views::MdTextButton::PressedCallback callback);

  views::MdTextButton* button(int index) const;
  views::MdTextButton* source_language_button() const;
  views::MdTextButton* target_language_button() const;

 private:
  int AddLanguageDropdownButton(
      views::View* translate_container,
      views::MdTextButtonWithDownArrow::PressedCallback callback,
      const std::u16string& label_text);

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
  int source_language_button_index_ = -1;
  int target_language_button_index_ = -1;
  std::vector<LanguageButton*> language_buttons_;
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
