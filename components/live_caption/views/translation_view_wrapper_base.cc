// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/translation_view_wrapper_base.h"

#include <cstddef>
#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "components/language/core/common/language_util.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "components/live_caption/views/format_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_ui_languages_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/md_text_button_with_down_arrow.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace captions {
namespace {

void InitButton(views::MdTextButton* button, views::Label* label) {
  button->SetCustomPadding(kLanguageButtonInsets);
  label->SetMultiLine(false);
  button->SetImageLabelSpacing(kLanguageButtonImageLabelSpacing);
  button->SetBgColorIdOverride(ui::kColorLiveCaptionBubbleButtonBackground);
  button->SetPaintToLayer();
}

}  // namespace

class LanguageTextButton : public views::MdTextButton,
                           public TranslationViewWrapperBase::LanguageButton {
  METADATA_HEADER(LanguageTextButton, views::MdTextButton)
 public:
  explicit LanguageTextButton(views::MdTextButton::PressedCallback callback)
      : views::MdTextButton(std::move(callback)) {
    InitButton(GetMdTextButton(), GetLabel());
  }

  LanguageTextButton(const LanguageTextButton&) = delete;
  LanguageTextButton& operator=(const LanguageTextButton&) = delete;
  ~LanguageTextButton() override = default;

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  views::MdTextButton* GetMdTextButton() override { return this; }

  views::Label* GetLabel() override { return label(); }
};

BEGIN_METADATA(LanguageTextButton)
END_METADATA

class LanguageDropdownButton
    : public views::MdTextButtonWithDownArrow,
      public TranslationViewWrapperBase::LanguageButton {
  METADATA_HEADER(LanguageDropdownButton, views::MdTextButtonWithDownArrow)
 public:
  LanguageDropdownButton(
      views::MdTextButtonWithDownArrow::PressedCallback callback,
      std::u16string label_text)
      : views::MdTextButtonWithDownArrow(std::move(callback), label_text) {
    InitButton(GetMdTextButton(), GetLabel());
  }

  LanguageDropdownButton(const LanguageDropdownButton&) = delete;
  LanguageDropdownButton& operator=(const LanguageDropdownButton&) = delete;
  ~LanguageDropdownButton() override = default;

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  views::MdTextButton* GetMdTextButton() override { return this; }

  views::Label* GetLabel() override { return label(); }
};

BEGIN_METADATA(LanguageDropdownButton)
END_METADATA

TranslationViewWrapperBase::TranslationViewWrapperBase() = default;

TranslationViewWrapperBase::~TranslationViewWrapperBase() = default;

void TranslationViewWrapperBase::Init(views::View* translate_container,
                                      Delegate* delegate) {
  caption_bubble_settings()->SetObserver(weak_ptr_factory_.GetWeakPtr());
  delegate_ = delegate;
  std::vector<std::string> language_codes;
  translate::TranslateDownloadManager::GetSupportedLanguages(true,
                                                             &language_codes);
  std::string source_language_code =
      caption_bubble_settings()->GetLiveCaptionLanguageCode();
  language::ToTranslateLanguageSynonym(&source_language_code);
  std::string target_language_code =
      caption_bubble_settings()->GetLiveTranslateTargetLanguageCode();
  language::ToTranslateLanguageSynonym(&target_language_code);
  translate_ui_languages_manager_ =
      std::make_unique<translate::TranslateUILanguagesManager>(
          language_codes, source_language_code, target_language_code);

  auto translation_text = std::make_unique<views::Label>();
  translation_text->SetBackgroundColor(SK_ColorTRANSPARENT);
  translation_text->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  translation_text->SetText(
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_TRANSLATING));
  auto translate_indicator_container = std::make_unique<views::View>();
  translate_indicator_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kLanguageButtonImageLabelSpacing))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  translate_icon_ = translate_indicator_container->AddChildView(
      std::make_unique<views::ImageView>());
  translation_header_text_ =
      translate_indicator_container->AddChildView(std::move(translation_text));

  source_language_text_ = GetSourceLanguageName();
  target_language_text_ = GetTargetLanguageName();

  translate_indicator_container_ = translate_container->AddChildView(
      std::move(translate_indicator_container));
  source_language_button_index_ = AddLanguageTextButton(
      translate_container,
      base::BindRepeating(
          &TranslationViewWrapperBase::OnSourceLanguageButtonPressed,
          base::Unretained(this)));
  translate_arrow_icon_ =
      translate_container->AddChildView(std::make_unique<views::ImageView>());
  target_language_button_index_ = AddLanguageDropdownButton(
      translate_container,
      base::BindRepeating(
          &TranslationViewWrapperBase::OnTargetLanguageButtonPressed,
          base::Unretained(this)),
      target_language_text_);

  translation_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  // Add 1 to account for unknown language option at index 0 in
  // TranslateUILanguagesManager language list. Since the command id is also
  // assigned here, there is no further handling for the unknown language.
  for (size_t i = 1;
       i < translate_ui_languages_manager_->GetNumberOfLanguages(); ++i) {
    translation_menu_model_->AddCheckItem(
        i, translate_ui_languages_manager_->GetLanguageNameAt(i));
  }
  MaybeAddChildViews(translate_container);
  UpdateLanguageLabel();
  delegate_->UpdateLanguageDirection(GetDisplayLanguage());
}

std::vector<raw_ptr<views::View, VectorExperimental>>
TranslationViewWrapperBase::GetButtons() {
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons;
  for (auto& language_button : language_buttons_) {
    buttons.push_back(language_button->GetMdTextButton());
  }
  return buttons;
}

void TranslationViewWrapperBase::SetTextSizeAndFontFamily(
    double text_scale_factor,
    const gfx::FontList& font_list) {
  translation_header_text_->SetLineHeight(kLiveTranslateLabelLineHeightDip *
                                          text_scale_factor);
  translation_header_text_->SetFontList(font_list);
  for (auto& language_button : language_buttons_) {
    language_button->GetLabel()->SetLineHeight(
        kLiveTranslateLabelLineHeightDip * text_scale_factor);
    language_button->GetLabel()->SetFontList(font_list);
    language_button->GetMdTextButton()->SetFocusRingCornerRadius(
        text_scale_factor * kLineHeightDip / 2);
  }
}

void TranslationViewWrapperBase::SetTextColor(
    SkColor language_label_color,
    SkColor language_label_border_color,
    SkColor header_color) {
  for (auto& language_button : language_buttons_) {
    views::MdTextButton* const button = language_button->GetMdTextButton();
    button->SetEnabledTextColors(language_label_color);
// On macOS whenever the caption bubble is not in main focus the button state
// is set to disabled. These buttons are never disabled so it is okay to
// override this state.
#if BUILDFLAG(IS_MAC)
    button->SetTextColor(views::Button::STATE_DISABLED, language_label_color);
#endif
    // TODO(crbug.com/40259212): The live caption bubble allows users to set
    // their own color and alpha value from a predefined list. This SKColor is
    // calculated during  ParseNonTransparentRGBACSSColorString, however the
    // equivalent ui::ColorId may not exist. To avoid needing to define around
    // 40 new color ids to account for each combination, we use the deprecated
    // SKColor function.
    button->SetStrokeColorOverrideDeprecated(language_label_border_color);
  }
  translation_header_text_->SetEnabledColor(header_color);
  translate_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kTranslateIcon, header_color, kLiveTranslateImageWidthDip));
  translate_arrow_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kTranslateIcon, header_color, kLiveTranslateImageWidthDip));
  translate_arrow_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kArrowRightAltIcon, header_color,
      kLiveTranslateImageWidthDip));
}

void TranslationViewWrapperBase::UpdateLanguageLabel() {
  UpdateLanguageLabelInternal();
}

void TranslationViewWrapperBase::OnAutoDetectedLanguageChanged(
    std::string auto_detected_language_code) {
  language::ToTranslateLanguageSynonym(&auto_detected_language_code);
  translate_ui_languages_manager_->UpdateSourceLanguage(
      auto_detected_language_code);
  source_language_text_ = GetSourceLanguageName();

  std::string live_caption_language_code =
      caption_bubble_settings()->GetLiveCaptionLanguageCode();
  language::ToTranslateLanguageSynonym(&live_caption_language_code);
  auto_detected_source_language_ =
      live_caption_language_code != auto_detected_language_code;
  UpdateLanguageLabel();
  delegate_->OnLanguageChanged(GetDisplayLanguage());
}

void TranslationViewWrapperBase::UpdateContentSize() {
  for (auto& language_button : language_buttons_) {
    views::MdTextButton* const button = language_button->GetMdTextButton();
    button->SetMinSize(gfx::Size());
    button->SetPreferredSize(button->CalculatePreferredSize({}));
  }
}

views::Label* TranslationViewWrapperBase::GetSourceLanguageLabelForTesting() {
  return views::AsViewClass<views::Label>(
      language_buttons_[source_language_button_index_]->GetLabel());
}

views::Label* TranslationViewWrapperBase::GetTargetLanguageLabelForTesting() {
  return views::AsViewClass<views::Label>(
      language_buttons_[target_language_button_index_]->GetLabel());
}

views::MdTextButton*
TranslationViewWrapperBase::GetSourceLanguageButtonForTesting() {
  return source_language_button();
}

views::MdTextButton*
TranslationViewWrapperBase::GetTargetLanguageButtonForTesting() {
  return target_language_button();
}

views::View* TranslationViewWrapperBase::GetTranslateIconAndTextForTesting() {
  return translate_indicator_container_.get();
}

views::View* TranslationViewWrapperBase::GetTranslateArrowIconForTesting() {
  return translate_arrow_icon_.get();
}

void TranslationViewWrapperBase::SetTargetLanguageForTesting(
    const std::string& language_code) {
  for (size_t i = 0;
       i < translate_ui_languages_manager_->GetNumberOfLanguages(); ++i) {
    if (language_code ==
        translate_ui_languages_manager_->GetLanguageCodeAt(i)) {
      ExecuteCommand(/*target_language_code_index=*/i, /*event_flags=*/0);
    }
  }
}

void TranslationViewWrapperBase::MaybeAddChildViews(
    views::View* translate_container) {}

void TranslationViewWrapperBase::UpdateLanguageLabelInternal() {
  if (auto_detected_source_language_) {
    source_language_button()->SetText(l10n_util::GetStringFUTF16(
        IDS_LIVE_CAPTION_CAPTION_LANGUAGE_AUTODETECTED, source_language_text_));
  } else {
    source_language_button()->SetText(source_language_text_);
  }

  if (caption_bubble_settings()->IsLiveTranslateFeatureEnabled() &&
      caption_bubble_settings()->GetLiveTranslateEnabled()) {
    if (SourceAndTargetLanguageCodeMatch() && auto_detected_source_language_) {
      target_language_button()->SetText(l10n_util::GetStringFUTF16(
          IDS_LIVE_CAPTION_CAPTION_LANGUAGE_AUTODETECTED,
          target_language_text_));
    } else {
      target_language_button()->SetText(target_language_text_);
    }
    SetTranslationsViewVisible(true);
  } else {
    SetTranslationsViewVisible(false);
  }
}

int TranslationViewWrapperBase::AddLanguageTextButton(
    views::View* translate_container,
    views::MdTextButton::PressedCallback callback) {
  auto language_button =
      std::make_unique<LanguageTextButton>(std::move(callback));
  language_button->GetMdTextButton()->GetViewAccessibility().SetIsIgnored(true);
  language_buttons_.push_back(
      translate_container->AddChildView(std::move(language_button)));
  return language_buttons_.size() - 1;
}

int TranslationViewWrapperBase::AddLanguageDropdownButton(
    views::View* translate_container,
    views::MdTextButtonWithDownArrow::PressedCallback callback,
    const std::u16string& label_text) {
  auto language_button =
      std::make_unique<LanguageDropdownButton>(std::move(callback), label_text);
  language_button->GetMdTextButton()->GetViewAccessibility().SetIsIgnored(true);
  language_buttons_.push_back(
      translate_container->AddChildView(std::move(language_button)));
  return language_buttons_.size() - 1;
}

views::MdTextButton* TranslationViewWrapperBase::button(int index) const {
  return language_buttons_.at(index)->GetMdTextButton();
}

views::MdTextButton* TranslationViewWrapperBase::source_language_button()
    const {
  return button(source_language_button_index_);
}

views::MdTextButton* TranslationViewWrapperBase::target_language_button()
    const {
  return button(target_language_button_index_);
}

void TranslationViewWrapperBase::SetTranslationsViewVisible(
    bool live_translate_enabled) {
  target_language_button()->SetVisible(live_translate_enabled);
  translate_indicator_container_->SetVisible(live_translate_enabled);
  translate_arrow_icon_->SetVisible(live_translate_enabled);

  if (live_translate_enabled) {
    // When Live Translate is enabled and the source language matches the target
    // language, only the dropdown button to select a new target language should
    // be visible. Otherwise show all translation views.
    const bool sourceMatchesTarget = SourceAndTargetLanguageCodeMatch();
    translate_indicator_container_->SetVisible(!sourceMatchesTarget);
    translate_arrow_icon_->SetVisible(!sourceMatchesTarget);
    source_language_button()->SetVisible(!sourceMatchesTarget);
  } else {
    source_language_button()->SetVisible(true);
  }
}

// The command id for the SimpleMenuModel will be the index of the target
// language selected. ExecuteCommand will be used to set the Live Translate
// pref.
void TranslationViewWrapperBase::ExecuteCommand(int target_language_code_index,
                                                int event_flags) {
  const bool updated =
      translate_ui_languages_manager_->UpdateTargetLanguageIndex(
          target_language_code_index);
  if (updated) {
    std::string target_language_code = GetTargetLanguageCode();
    language::ToChromeLanguageSynonym(&target_language_code);
    caption_bubble_settings()->SetLiveTranslateTargetLanguageCode(
        target_language_code);
  }
}

bool TranslationViewWrapperBase::IsCommandIdChecked(
    int target_language_code_index) const {
  return translate_ui_languages_manager_->GetTargetLanguageIndex() ==
         static_cast<size_t>(target_language_code_index);
}

void TranslationViewWrapperBase::OnLiveTranslateEnabledChanged() {
  UpdateLanguageLabel();
  delegate_->OnLanguageChanged(GetDisplayLanguage());
}

void TranslationViewWrapperBase::OnLiveCaptionLanguageChanged() {
  auto_detected_source_language_ = false;
  std::string source_language_code =
      caption_bubble_settings()->GetLiveCaptionLanguageCode();
  language::ToTranslateLanguageSynonym(&source_language_code);
  translate_ui_languages_manager_->UpdateSourceLanguage(source_language_code);
  source_language_text_ = GetSourceLanguageName();
  UpdateLanguageLabel();
  delegate_->OnLanguageChanged(GetDisplayLanguage());
}

void TranslationViewWrapperBase::OnLiveTranslateTargetLanguageChanged() {
  std::string target_language_code =
      caption_bubble_settings()->GetLiveTranslateTargetLanguageCode();
  language::ToTranslateLanguageSynonym(&target_language_code);
  translate_ui_languages_manager_->UpdateTargetLanguage(target_language_code);
  target_language_text_ = GetTargetLanguageName();
  UpdateLanguageLabel();
  delegate_->OnLanguageChanged(GetDisplayLanguage());
}

void TranslationViewWrapperBase::OnSourceLanguageButtonPressed() {
  delegate_->CaptionSettingsButtonPressed();
}

void TranslationViewWrapperBase::OnTargetLanguageButtonPressed() {
  translation_menu_runner_ = std::make_unique<views::MenuRunner>(
      translation_menu_model_.get(), views::MenuRunner::COMBOBOX);
  const gfx::Rect& screen_bounds =
      target_language_button()->GetBoundsInScreen();
  translation_menu_runner_->RunMenuAt(
      target_language_button()->GetWidget(), /*button_controller=*/nullptr,
      screen_bounds, views::MenuAnchorPosition::kTopLeft,
      ui::mojom::MenuSourceType::kMouse);
}

std::string TranslationViewWrapperBase::GetDisplayLanguage() {
  return caption_bubble_settings()->GetLiveTranslateEnabled()
             ? GetTargetLanguageCode()
             : GetSourceLanguageCode();
}

std::string TranslationViewWrapperBase::GetSourceLanguageCode() const {
  CHECK(translate_ui_languages_manager_);
  return translate_ui_languages_manager_->GetSourceLanguageCode();
}

std::string TranslationViewWrapperBase::GetTargetLanguageCode() const {
  CHECK(translate_ui_languages_manager_);
  return translate_ui_languages_manager_->GetTargetLanguageCode();
}

bool TranslationViewWrapperBase::SourceAndTargetLanguageCodeMatch() {
  CHECK(translate_ui_languages_manager_);
  return GetSourceLanguageCode() == GetTargetLanguageCode();
}

std::u16string TranslationViewWrapperBase::GetSourceLanguageName() const {
  CHECK(translate_ui_languages_manager_);
  return translate_ui_languages_manager_->GetLanguageNameAt(
      translate_ui_languages_manager_->GetSourceLanguageIndex());
}

std::u16string TranslationViewWrapperBase::GetTargetLanguageName() const {
  CHECK(translate_ui_languages_manager_);
  return translate_ui_languages_manager_->GetLanguageNameAt(
      translate_ui_languages_manager_->GetTargetLanguageIndex());
}

}  // namespace captions
