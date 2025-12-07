// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_live_caption_dialog.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/live_translate_combobox_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr gfx::Size kLiveCaptionDialogSize(260, 124);

constexpr int kLiveCaptionDialogCornerRadius = 12;

constexpr int kHorizontalMarginDip = 20;
constexpr int kImageWidthDip = 20;
constexpr int kVerticalMarginDip = 10;

}  // namespace

OverlayWindowLiveCaptionDialog::OverlayWindowLiveCaptionDialog(Profile* profile)
    : profile_(profile) {
  SetSize(kLiveCaptionDialogSize);
  SetBackground(views::CreateRoundedRectBackground(
      ui::kColorSysSurface, kLiveCaptionDialogCornerRadius));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  auto live_caption_container = std::make_unique<View>();

  auto live_caption_image = std::make_unique<views::ImageView>();
  live_caption_image->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kLiveCaptionOnIcon, ui::kColorIcon, kImageWidthDip));
  live_caption_container->AddChildView(std::move(live_caption_image));

  auto live_caption_title =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_LIVE_CAPTION_CONTROL_TEXT));
  live_caption_title->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  live_caption_title->SetMultiLine(true);
  live_caption_title_ =
      live_caption_container->AddChildView(std::move(live_caption_title));

  auto live_caption_button =
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &OverlayWindowLiveCaptionDialog::OnLiveCaptionButtonPressed,
          base::Unretained(this)));
  live_caption_button->SetIsOn(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  live_caption_button->GetViewAccessibility().SetName(
      std::u16string(live_caption_title_->GetText()));
  live_caption_button_ =
      live_caption_container->AddChildView(std::move(live_caption_button));

  auto* live_caption_container_layout =
      live_caption_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets::VH(kVerticalMarginDip, kHorizontalMarginDip),
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL)));
  live_caption_container_layout->SetFlexForView(live_caption_title_, 1);
  AddChildView(std::move(live_caption_container));

  auto live_translate_container = std::make_unique<View>();

  auto live_translate_image = std::make_unique<views::ImageView>();
  live_translate_image->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kTranslateIcon, ui::kColorIcon, kImageWidthDip));
  live_translate_container->AddChildView(std::move(live_translate_image));

  auto live_translate_label_wrapper = std::make_unique<View>();
  live_translate_label_wrapper->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  auto live_translate_title =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_SETTINGS_CAPTIONS_ENABLE_LIVE_TRANSLATE_TITLE));
  live_translate_title->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  live_translate_title->SetMultiLine(true);
  live_translate_title_ = live_translate_label_wrapper->AddChildView(
      std::move(live_translate_title));

  live_translate_label_wrapper_ = live_translate_container->AddChildView(
      std::move(live_translate_label_wrapper));

  auto live_translate_button =
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &OverlayWindowLiveCaptionDialog::OnLiveTranslateButtonPressed,
          base::Unretained(this)));
  live_translate_button->SetIsOn(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled));
  live_translate_button->SetEnabled(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  live_translate_button->GetViewAccessibility().SetName(
      std::u16string(live_translate_title_->GetText()));
  auto* live_translate_container_layout =
      live_translate_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets::VH(kVerticalMarginDip, kHorizontalMarginDip),
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL)));
  live_translate_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  live_translate_container_layout->SetFlexForView(live_translate_label_wrapper_,
                                                  1);
  live_translate_button_ =
      live_translate_container->AddChildView(std::move(live_translate_button));
  AddChildView(std::move(live_translate_container));

  // Initialize the target language container.
  auto target_language_container = std::make_unique<View>();
  target_language_container->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kVerticalMarginDip, 0)));
  target_language_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  auto target_language_model =
      std::make_unique<LiveTranslateComboboxModel>(profile_);
  auto target_language_combobox =
      std::make_unique<views::Combobox>(std::move(target_language_model));
  target_language_combobox->SetCallback(base::BindRepeating(
      &OverlayWindowLiveCaptionDialog::TargetLanguageChanged,
      base::Unretained(this)));
  target_language_combobox->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_LIVE_TRANSLATE_TARGET_LANGUAGE_ACCNAME));
  target_language_combobox->SetEnabled(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled) &&
      profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  target_language_combobox_ = target_language_container->AddChildView(
      std::move(target_language_combobox));
  AddChildView(std::move(target_language_container));

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(
          &OverlayWindowLiveCaptionDialog::OnLiveCaptionEnabledChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveTranslateEnabled,
      base::BindRepeating(
          &OverlayWindowLiveCaptionDialog::OnLiveTranslateEnabledChanged,
          base::Unretained(this)));
}

OverlayWindowLiveCaptionDialog::~OverlayWindowLiveCaptionDialog() = default;

void OverlayWindowLiveCaptionDialog::OnLiveCaptionButtonPressed() {
  bool enabled = !profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled);
  profile_->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, enabled);
  base::UmaHistogramBoolean(
      "Accessibility.LiveCaption.EnableFromVideoPictureInPicture", enabled);
}

void OverlayWindowLiveCaptionDialog::OnLiveCaptionEnabledChanged() {
  bool enabled = profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled);
  live_caption_button_->SetIsOn(enabled);
  live_translate_button_->SetEnabled(enabled);
  target_language_combobox_->SetEnabled(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled) &&
      enabled);
}

void OverlayWindowLiveCaptionDialog::OnLiveTranslateButtonPressed() {
  bool enabled =
      !profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled);
  profile_->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled, enabled);
  base::UmaHistogramBoolean(
      "Accessibility.LiveTranslate.EnableFromVideoPictureInPicture", enabled);
}

void OverlayWindowLiveCaptionDialog::OnLiveTranslateEnabledChanged() {
  bool enabled = profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled);
  live_translate_button_->SetIsOn(enabled);
  target_language_combobox_->SetEnabled(
      profile_->GetPrefs()->GetBoolean(prefs::kLiveTranslateEnabled) &&
      profile_->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
}

void OverlayWindowLiveCaptionDialog::TargetLanguageChanged() {
  static_cast<LiveTranslateComboboxModel*>(
      target_language_combobox_->GetModel())
      ->UpdateTargetLanguageIndex(
          target_language_combobox_->GetSelectedIndex().value());
}

BEGIN_METADATA(OverlayWindowLiveCaptionDialog)
END_METADATA
