// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

using read_anything::mojom::LetterSpacing;
using read_anything::mojom::LineSpacing;

ReadAnythingModel::ReadAnythingModel()
    : font_name_(string_constants::kReadAnythingPlaceholderFontName),
      font_scale_(kReadAnythingDefaultFontScale),
      font_model_(std::make_unique<ReadAnythingFontModel>()),
      colors_model_(std::make_unique<ReadAnythingColorsModel>()),
      line_spacing_model_(std::make_unique<ReadAnythingLineSpacingModel>()),
      letter_spacing_model_(
          std::make_unique<ReadAnythingLetterSpacingModel>()) {}

ReadAnythingModel::~ReadAnythingModel() = default;

void ReadAnythingModel::Init(const std::string& lang_code,
                             const std::string& font_name,
                             double font_scale,
                             read_anything::mojom::Colors colors,
                             LineSpacing line_spacing,
                             LetterSpacing letter_spacing) {
  font_model_->SetDefaultLanguage(lang_code);

  // If this profile has previously selected choices that were saved to
  // prefs, check they are still a valid, and then assign if so.
  size_t font_index = font_model_->GetFontNameIndex(font_name);
  if (font_model_->IsValidFontIndex(font_index)) {
    font_model_->SetSelectedIndex(font_index);
  }

  font_scale_ = GetValidFontScale(font_scale);

  size_t colors_index = static_cast<size_t>(colors);
  if (colors_model_->IsValidIndex(colors_index)) {
    colors_model_->SetSelectedIndex(colors_index);
  }

  // LineSpacing contains a deprecated value, so it doesn't correspond exactly
  // to drop-down indices.
  size_t line_spacing_index =
      line_spacing_model_->GetIndexForLineSpacing(line_spacing);
  if (line_spacing_model_->IsValidIndex(line_spacing_index)) {
    line_spacing_model_->SetSelectedIndex(line_spacing_index);
  }

  // LetterSpacing contains a deprecated value, so it doesn't correspond exactly
  // to drop-down indices.
  size_t letter_spacing_index =
      letter_spacing_model_->GetIndexForLetterSpacing(letter_spacing);
  if (letter_spacing_model_->IsValidIndex(letter_spacing_index)) {
    letter_spacing_model_->SetSelectedIndex(letter_spacing_index);
  }

  font_name_ = font_model_->GetFontNameAt(font_model_->GetSelectedIndex());
  colors_combobox_index_ = colors_model_->GetSelectedIndex().value();
  auto& initial_colors = colors_model_->GetColorsAt(colors_combobox_index_);
  foreground_color_id_ = initial_colors.foreground_color_id;
  background_color_id_ = initial_colors.background_color_id;
  separator_color_id_ = initial_colors.separator_color_id;
  dropdown_color_id_ = initial_colors.dropdown_color_id;
  selected_dropdown_color_id_ = initial_colors.selected_dropdown_color_id;
  focus_ring_color_id_ = initial_colors.focus_ring_color_id;

  line_spacing_ = line_spacing_model_->GetLineSpacingAt(
      line_spacing_model_->GetSelectedIndex().value());
  letter_spacing_ = letter_spacing_model_->GetLetterSpacingAt(
      letter_spacing_model_->GetSelectedIndex().value());
}

void ReadAnythingModel::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
  if (!features::IsReadAnythingWebUIToolbarEnabled()) {
    NotifyThemeChanged();
  }
}

void ReadAnythingModel::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void ReadAnythingModel::SetSelectedFontByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(font_model_->IsValidFontIndex(new_index));

  // Keep track of current selection for GetDefaultIndex().
  font_model_->SetSelectedIndex(new_index);

  // Update state and notify listeners
  font_name_ = font_model_->GetFontNameAt(new_index);
  NotifyThemeChanged();
}

void ReadAnythingModel::SetSelectedColorsByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(colors_model_->IsValidIndex(new_index));

  colors_combobox_index_ = new_index;
  auto& new_colors = colors_model_->GetColorsAt(new_index);
  foreground_color_id_ = new_colors.foreground_color_id;
  background_color_id_ = new_colors.background_color_id;
  separator_color_id_ = new_colors.separator_color_id;
  dropdown_color_id_ = new_colors.dropdown_color_id;
  selected_dropdown_color_id_ = new_colors.selected_dropdown_color_id;
  focus_ring_color_id_ = new_colors.focus_ring_color_id;

  NotifyThemeChanged();
}

void ReadAnythingModel::SetSelectedLineSpacingByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(line_spacing_model_->IsValidIndex(new_index));

  line_spacing_ = line_spacing_model_->GetLineSpacingAt(new_index);
  NotifyThemeChanged();
}

void ReadAnythingModel::SetSelectedLetterSpacingByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(letter_spacing_model_->IsValidIndex(new_index));

  letter_spacing_ = letter_spacing_model_->GetLetterSpacingAt(new_index);
  NotifyThemeChanged();
}

double ReadAnythingModel::GetValidFontScale(double font_scale) {
  if (font_scale < kReadAnythingMinimumFontScale)
    return kReadAnythingMinimumFontScale;
  if (font_scale > kReadAnythingMaximumFontScale)
    return kReadAnythingMaximumFontScale;
  return font_scale;
}

// TODO(1266555): Update with text scaling approach based on UI/UX feedback.
void ReadAnythingModel::DecreaseTextSize() {
  font_scale_ -= kReadAnythingFontScaleIncrement;
  if (font_scale_ < kReadAnythingMinimumFontScale)
    font_scale_ = kReadAnythingMinimumFontScale;

  NotifyThemeChanged();
}

void ReadAnythingModel::IncreaseTextSize() {
  font_scale_ += kReadAnythingFontScaleIncrement;
  if (font_scale_ > kReadAnythingMaximumFontScale)
    font_scale_ = kReadAnythingMaximumFontScale;

  NotifyThemeChanged();
}

void ReadAnythingModel::OnSystemThemeChanged() {
  NotifyThemeChanged();
}

void ReadAnythingModel::NotifyThemeChanged() {
  for (Observer& obs : observers_) {
    obs.OnReadAnythingThemeChanged(
        font_name_, font_scale_, foreground_color_id_, background_color_id_,
        separator_color_id_, dropdown_color_id_, selected_dropdown_color_id_,
        focus_ring_color_id_, line_spacing_, letter_spacing_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingFontModel
///////////////////////////////////////////////////////////////////////////////

ReadAnythingFontModel::ReadAnythingFontModel() {}

// If you change these fonts, please also update read_anything_constants.h
void ReadAnythingFontModel::SetDefaultLanguage(const std::string& lang) {
  if (base::Contains(kLanguagesSupportedByPoppins, lang)) {
    font_choices_.emplace_back(u"Poppins");
  }
  font_choices_.emplace_back(u"Sans-serif");
  font_choices_.emplace_back(u"Serif");
  if (base::Contains(kLanguagesSupportedByComicNeue, lang)) {
    font_choices_.emplace_back(u"Comic Neue");
  }
  if (base::Contains(kLanguagesSupportedByLexendDeca, lang)) {
    font_choices_.emplace_back(u"Lexend Deca");
  }
  if (base::Contains(kLanguagesSupportedByEbGaramond, lang)) {
    font_choices_.emplace_back(u"EB Garamond");
  }
  if (base::Contains(kLanguagesSupportedByStixTwoText, lang)) {
    font_choices_.emplace_back(u"STIX Two Text");
  }
  if (base::Contains(kLanguagesSupportedByAndika, lang)) {
    font_choices_.emplace_back(u"Andika");
  }
  font_choices_.shrink_to_fit();
}

bool ReadAnythingFontModel::IsValidFontIndex(size_t index) {
  return index < GetItemCount();
}

size_t ReadAnythingFontModel::GetFontNameIndex(std::string font_name) {
  auto it = base::ranges::find(font_choices_, base::UTF8ToUTF16(font_name));
  return static_cast<size_t>(it - font_choices_.begin());
}

// ui::Combobox needs a default option to show whenever Read Anything is
// reopened in the same browser window.
absl::optional<size_t> ReadAnythingFontModel::GetDefaultIndex() const {
  return selected_index_;
}

absl::optional<size_t> ReadAnythingFontModel::GetDefaultIndexForTesting() {
  return selected_index_;
}

void ReadAnythingFontModel::SetSelectedIndex(size_t index) {
  selected_index_ = index;
}

size_t ReadAnythingFontModel::GetItemCount() const {
  return font_choices_.size();
}

std::u16string ReadAnythingFontModel::GetItemAt(size_t index) const {
  return GetDropDownTextAt(index);
}

std::u16string ReadAnythingFontModel::GetDropDownTextAt(size_t index) const {
  DCHECK_LT(index, GetItemCount());
  return font_choices_[index];
}

std::string ReadAnythingFontModel::GetFontNameAt(size_t index) {
  DCHECK_LT(index, GetItemCount());
  return base::UTF16ToUTF8(font_choices_[index]);
}

absl::optional<ui::ColorId>
ReadAnythingFontModel::GetDropdownForegroundColorIdAt(size_t index) const {
  return foreground_color_id_;
}

absl::optional<ui::ColorId>
ReadAnythingFontModel::GetDropdownBackgroundColorIdAt(size_t index) const {
  return background_color_id_;
}

absl::optional<ui::ColorId>
ReadAnythingFontModel::GetDropdownSelectedBackgroundColorIdAt(
    size_t index) const {
  return selected_color_id_;
}

ReadAnythingFontModel::~ReadAnythingFontModel() = default;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingColorsModel
///////////////////////////////////////////////////////////////////////////////
ReadAnythingColorsModel::ColorInfo::ColorInfo(
    std::u16string name,
    int icon_asset,
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    ui::ColorId separator_color_id,
    ui::ColorId dropdown_color_id,
    ui::ColorId selected_dropdown_color_id,
    ui::ColorId focus_ring_color_id)
    : name(name),
      icon_asset(icon_asset),
      foreground_color_id(foreground_color_id),
      background_color_id(background_color_id),
      separator_color_id(separator_color_id),
      dropdown_color_id(dropdown_color_id),
      selected_dropdown_color_id(selected_dropdown_color_id),
      focus_ring_color_id(focus_ring_color_id) {}
ReadAnythingColorsModel::ColorInfo::ColorInfo(const ColorInfo& other) = default;
ReadAnythingColorsModel::ColorInfo::ColorInfo(ColorInfo&&) = default;
ReadAnythingColorsModel::ColorInfo&
ReadAnythingColorsModel::ColorInfo::operator=(const ColorInfo&) = default;
ReadAnythingColorsModel::ColorInfo&
ReadAnythingColorsModel::ColorInfo::operator=(ColorInfo&&) = default;
ReadAnythingColorsModel::ColorInfo::~ColorInfo() = default;

ReadAnythingColorsModel::ReadAnythingColorsModel() {
  // Define the possible sets of colors available to the user.
  ColorInfo kDefaultColors = {
      l10n_util::GetStringUTF16(IDS_READING_MODE_DEFAULT_COLOR_LABEL),
      IDS_READING_MODE_DEFAULT_PNG,
      kColorReadAnythingForeground,
      kColorReadAnythingBackground,
      kColorReadAnythingSeparator,
      kColorReadAnythingDropdownBackground,
      kColorReadAnythingDropdownSelected,
      kColorReadAnythingFocusRingBackground};

  ColorInfo kLightColors = {
      l10n_util::GetStringUTF16(IDS_READING_MODE_LIGHT_COLOR_LABEL),
      IDS_READING_MODE_LIGHT_PNG,
      kColorReadAnythingForegroundLight,
      kColorReadAnythingBackgroundLight,
      kColorReadAnythingSeparatorLight,
      kColorReadAnythingDropdownBackgroundLight,
      kColorReadAnythingDropdownSelectedLight,
      kColorReadAnythingFocusRingBackgroundLight};

  ColorInfo kDarkColors = {
      l10n_util::GetStringUTF16(IDS_READING_MODE_DARK_COLOR_LABEL),
      IDS_READING_MODE_DARK_PNG,
      kColorReadAnythingForegroundDark,
      kColorReadAnythingBackgroundDark,
      kColorReadAnythingSeparatorDark,
      kColorReadAnythingDropdownBackgroundDark,
      kColorReadAnythingDropdownSelectedDark,
      kColorReadAnythingFocusRingBackgroundDark};

  ColorInfo kYellowColors = {
      l10n_util::GetStringUTF16(IDS_READING_MODE_YELLOW_COLOR_LABEL),
      IDS_READING_MODE_YELLOW_PNG,
      kColorReadAnythingForegroundYellow,
      kColorReadAnythingBackgroundYellow,
      kColorReadAnythingSeparatorYellow,
      kColorReadAnythingDropdownBackgroundYellow,
      kColorReadAnythingDropdownSelectedYellow,
      kColorReadAnythingFocusRingBackgroundYellow};

  ColorInfo kBlueColors = {
      l10n_util::GetStringUTF16(IDS_READING_MODE_BLUE_COLOR_LABEL),
      IDS_READING_MODE_BLUE_PNG,
      kColorReadAnythingForegroundBlue,
      kColorReadAnythingBackgroundBlue,
      kColorReadAnythingSeparatorBlue,
      kColorReadAnythingDropdownBackgroundBlue,
      kColorReadAnythingDropdownSelectedBlue,
      kColorReadAnythingFocusRingBackgroundBlue};

  colors_choices_.emplace_back(kDefaultColors);
  colors_choices_.emplace_back(kLightColors);
  colors_choices_.emplace_back(kDarkColors);
  colors_choices_.emplace_back(kYellowColors);
  colors_choices_.emplace_back(kBlueColors);
  colors_choices_.shrink_to_fit();

  for (std::vector<ColorInfo>::size_type i = 0; i < colors_choices_.size();
       i++) {
    AddCheckItem(i, colors_choices_[i].name);
    SetIcon(i, GetDropDownIconAt(i));
  }
}
bool ReadAnythingColorsModel::IsValidIndex(size_t index) {
  return index < colors_choices_.size();
}

ReadAnythingColorsModel::ColorInfo& ReadAnythingColorsModel::GetColorsAt(
    size_t index) {
  return colors_choices_[index];
}

ui::ImageModel ReadAnythingColorsModel::GetDropDownIconAt(size_t index) const {
  const gfx::ImageSkia* icon_skia_asset =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          colors_choices_[index].icon_asset);
  DCHECK(icon_skia_asset);

  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkiaOperations::CreateResizedImage(
          *icon_skia_asset, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
          gfx::Size(kColorsIconSize, kColorsIconSize)));
}

ReadAnythingColorsModel::~ReadAnythingColorsModel() = default;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingLineSpacingModel
///////////////////////////////////////////////////////////////////////////////

ReadAnythingLineSpacingModel::ReadAnythingLineSpacingModel() {
  // Define the line spacing options available to the user.
  LineSpacingInfo kStandard = {
      LineSpacing::kStandard,
      l10n_util::GetStringUTF16(IDS_READING_MODE_SPACING_COMBOBOX_STANDARD),
      kReadAnythingLineSpacingStandardIcon};
  LineSpacingInfo kLoose = {
      LineSpacing::kLoose,
      l10n_util::GetStringUTF16(IDS_READING_MODE_SPACING_COMBOBOX_LOOSE),
      kReadAnythingLineSpacingLooseIcon};
  LineSpacingInfo kVeryLoose = {
      LineSpacing::kVeryLoose,
      l10n_util::GetStringUTF16(IDS_READING_MODE_SPACING_COMBOBOX_VERY_LOOSE),
      kReadAnythingLineSpacingVeryLooseIcon};

  lines_choices_.emplace_back(kStandard);
  lines_choices_.emplace_back(kLoose);
  lines_choices_.emplace_back(kVeryLoose);
  lines_choices_.shrink_to_fit();

  for (std::vector<LetterSpacing>::size_type i = 0; i < lines_choices_.size();
       i++) {
    AddCheckItem(i, lines_choices_[i].name);
    SetIcon(i,
            ui::ImageModel::FromVectorIcon(lines_choices_[i].icon_asset,
                                           ui::kColorIcon, kSpacingIconSize));
  }
}

bool ReadAnythingLineSpacingModel::IsValidIndex(size_t index) {
  return index < lines_choices_.size();
}

size_t ReadAnythingLineSpacingModel::GetIndexForLineSpacing(
    LineSpacing line_spacing) {
  switch (line_spacing) {
    // If we read the deprecated value, choose the closest option.
    case LineSpacing::kTightDeprecated:
    case LineSpacing::kStandard:
      return 0;
    case LineSpacing::kLoose:
      return 1;
    case LineSpacing::kVeryLoose:
      return 2;
  }
}

LineSpacing ReadAnythingLineSpacingModel::GetLineSpacingAt(size_t index) {
  return lines_choices_[index].enum_value;
}

ReadAnythingLineSpacingModel::~ReadAnythingLineSpacingModel() = default;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingLetterSpacingModel
///////////////////////////////////////////////////////////////////////////////
ReadAnythingLetterSpacingModel::ReadAnythingLetterSpacingModel() {
  LetterSpacingInfo kStandard = {
      LetterSpacing::kStandard,
      l10n_util::GetStringUTF16(IDS_READING_MODE_SPACING_COMBOBOX_STANDARD),
      kReadAnythingLetterSpacingStandardIcon};
  LetterSpacingInfo kWide = {
      LetterSpacing::kWide,
      l10n_util::GetStringUTF16(IDS_READING_MODE_SPACING_COMBOBOX_WIDE),
      kReadAnythingLetterSpacingWideIcon};
  LetterSpacingInfo kVeryWide = {
      LetterSpacing::kVeryWide,
      l10n_util::GetStringUTF16(IDS_READING_MODE_SPACING_COMBOBOX_VERY_WIDE),
      kReadAnythingLetterSpacingVeryWideIcon};

  letters_choices_.emplace_back(kStandard);
  letters_choices_.emplace_back(kWide);
  letters_choices_.emplace_back(kVeryWide);
  letters_choices_.shrink_to_fit();

  for (std::vector<LetterSpacing>::size_type i = 0; i < letters_choices_.size();
       i++) {
    AddCheckItem(i, letters_choices_[i].name);
    SetIcon(i,
            ui::ImageModel::FromVectorIcon(letters_choices_[i].icon_asset,
                                           ui::kColorIcon, kSpacingIconSize));
  }
}

bool ReadAnythingLetterSpacingModel::IsValidIndex(size_t index) {
  return index < letters_choices_.size();
}

size_t ReadAnythingLetterSpacingModel::GetIndexForLetterSpacing(
    LetterSpacing letter_spacing) {
  switch (letter_spacing) {
    // If we read the deprecated value, choose the closest option.
    case LetterSpacing::kTightDeprecated:
    case LetterSpacing::kStandard:
      return 0;
    case LetterSpacing::kWide:
      return 1;
    case LetterSpacing::kVeryWide:
      return 2;
  }
}

LetterSpacing ReadAnythingLetterSpacingModel::GetLetterSpacingAt(size_t index) {
  return letters_choices_[index].enum_value;
}

ReadAnythingLetterSpacingModel::~ReadAnythingLetterSpacingModel() = default;
