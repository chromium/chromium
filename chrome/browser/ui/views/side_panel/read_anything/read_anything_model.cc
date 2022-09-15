// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"
#include "chrome/grit/component_extension_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

using read_anything::mojom::ReadAnythingTheme;

ReadAnythingModel::ReadAnythingModel()
    : font_name_(kReadAnythingDefaultFontName),
      font_scale_(kReadAnythingDefaultFontScale),
      font_model_(std::make_unique<ReadAnythingFontModel>()),
      colors_model_(std::make_unique<ReadAnythingColorsModel>()),
      letter_spacing_model_(
          std::make_unique<ReadAnythingLetterSpacingModel>()) {}

ReadAnythingModel::~ReadAnythingModel() = default;

void ReadAnythingModel::Init(
    std::string& font_name,
    double font_scale,
    read_anything::mojom::Colors colors,
    read_anything::mojom::LetterSpacing letter_spacing) {
  // If this profile has previously selected choices that were saved to
  // prefs, check they are still a valid, and then assign if so.
  if (font_model_->IsValidFontName(font_name)) {
    font_model_->SetDefaultIndexFromPrefsFontName(font_name);
    font_name_ = font_name;
  }

  size_t letter_spacing_index = static_cast<size_t>((int)letter_spacing);
  if (letter_spacing_model_->IsValidLetterSpacingIndex(letter_spacing_index)) {
    letter_spacing_model_->SetDefaultLetterSpacingIndexFromPref(
        letter_spacing_index);
    letter_spacing_ =
        (int)(letter_spacing_model_->GetLetterSpacingAt(letter_spacing_index));
  }

  font_scale_ = font_scale;

  size_t colors_index = static_cast<size_t>((int)colors);
  if (colors_model_->IsValidColorsIndex(colors_index)) {
    colors_model_->SetDefaultColorsIndexFromPref(colors_index);
  }

  auto& initial_colors =
      colors_model_->GetColorsAt(colors_model_->GetStartingStateIndex());
  foreground_color_ = initial_colors.foreground;
  background_color_ = initial_colors.background;
}

void ReadAnythingModel::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
  NotifyAXTreeDistilled();
  NotifyThemeChanged();
}

void ReadAnythingModel::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void ReadAnythingModel::SetSelectedFontByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(font_model_->IsValidFontIndex(new_index));

  // Update state and notify listeners
  font_name_ = font_model_->GetFontNameAt(new_index);
  NotifyThemeChanged();
}

void ReadAnythingModel::SetSelectedColorsByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(colors_model_->IsValidColorsIndex(new_index));

  auto& new_colors = colors_model_->GetColorsAt(new_index);
  foreground_color_ = new_colors.foreground;
  background_color_ = new_colors.background;

  NotifyThemeChanged();
}

void ReadAnythingModel::SetSelectedLetterSpacingByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(letter_spacing_model_->IsValidLetterSpacingIndex(new_index));

  letter_spacing_ = (int)(letter_spacing_model_->GetLetterSpacingAt(new_index));
  NotifyThemeChanged();
}

void ReadAnythingModel::SetDistilledAXTree(
    ui::AXTreeUpdate snapshot,
    std::vector<ui::AXNodeID> content_node_ids) {
  // Update state and notify listeners
  snapshot_ = std::move(snapshot);
  content_node_ids_ = std::move(content_node_ids);
  NotifyAXTreeDistilled();
}

// TODO(1266555): Update with text scaling approach based on UI/UX feedback.
void ReadAnythingModel::DecreaseTextSize() {
  font_scale_ -= 0.2f;
  if (font_scale_ < kReadAnythingMinimumFontScale)
    font_scale_ = kReadAnythingMinimumFontScale;

  NotifyThemeChanged();
}

void ReadAnythingModel::IncreaseTextSize() {
  font_scale_ += 0.2;
  if (font_scale_ > kReadAnythingMaximumFontScale)
    font_scale_ = kReadAnythingMaximumFontScale;

  NotifyThemeChanged();
}

void ReadAnythingModel::NotifyAXTreeDistilled() {
  for (Observer& obs : observers_) {
    obs.OnAXTreeDistilled(snapshot_, content_node_ids_);
  }
}

void ReadAnythingModel::NotifyThemeChanged() {
  for (Observer& obs : observers_) {
    obs.OnReadAnythingThemeChanged(ReadAnythingTheme::New(
        font_name_, kReadAnythingDefaultFontSize * font_scale_,
        foreground_color_, background_color_, letter_spacing_));
  }
}

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingFontModel
///////////////////////////////////////////////////////////////////////////////

ReadAnythingFontModel::ReadAnythingFontModel() {
  // TODO(1266555): i18n.
  font_choices_.emplace_back(u"Standard font");
  font_choices_.emplace_back(u"Sans-serif");
  font_choices_.emplace_back(u"Serif");
  font_choices_.emplace_back(u"Avenir");
  font_choices_.emplace_back(u"Comic Neue");
  font_choices_.emplace_back(u"Comic Sans MS");
  font_choices_.emplace_back(u"Poppins");
  font_choices_.shrink_to_fit();
}

bool ReadAnythingFontModel::IsValidFontName(const std::string& font_name) {
  return std::find(font_choices_.begin(), font_choices_.end(),
                   base::UTF8ToUTF16(font_name)) != font_choices_.end();
}

bool ReadAnythingFontModel::IsValidFontIndex(size_t index) {
  return index < GetItemCount();
}

void ReadAnythingFontModel::SetDefaultIndexFromPrefsFontName(
    std::string prefs_font_name) {
  auto it = std::find(font_choices_.begin(), font_choices_.end(),
                      base::UTF8ToUTF16(prefs_font_name));
  default_index_ = static_cast<size_t>(it - font_choices_.begin());
}

absl::optional<size_t> ReadAnythingFontModel::GetDefaultIndex() const {
  return default_index_;
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

// This method uses the text from the drop down at |index| and constructs a
// FontList to be used by the |ReadAnythingFontCombobox::MenuModel| to make
// each option to display in its associated font.
// This text is not visible to the user.
// We append 'Arial' and '18px' to have a back-up font and a set size in case
// the chosen font does not work for some reason.
// E.g. User chooses 'Serif', this method returns 'Serif, Arial, 18px'.
std::string ReadAnythingFontModel::GetLabelFontListAt(size_t index) {
  std::string font_label = base::UTF16ToUTF8(GetDropDownTextAt(index));
  base::StringAppendF(&font_label, "%s", ", Arial, 18px");
  return font_label;
}

ReadAnythingFontModel::~ReadAnythingFontModel() = default;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingColorsModel
///////////////////////////////////////////////////////////////////////////////

ReadAnythingColorsModel::ReadAnythingColorsModel() {
  // Define the possible sets of colors available to the user.
  // TODO (crbug.com/1266555): Define default colors from system theme.
  ColorInfo kDefaultColors = {u"Default", IDS_READ_ANYTHING_DEFAULT_PNG,
                              SkColors::kBlack.toSkColor(),
                              SkColors::kWhite.toSkColor()};

  ColorInfo kLightColors = {u"Light", IDS_READ_ANYTHING_LIGHT_PNG,
                            gfx::kGoogleGrey900, gfx::kGoogleGrey050};

  ColorInfo kDarkColors = {u"Dark", IDS_READ_ANYTHING_DARK_PNG,
                           gfx::kGoogleGrey200, kReadAnythingDarkBackground};

  ColorInfo kYellowColors = {u"Yellow", IDS_READ_ANYTHING_YELLOW_PNG,
                             kReadAnythingYellowForeground,
                             gfx::kGoogleYellow200};

  colors_choices_.emplace_back(kDefaultColors);
  colors_choices_.emplace_back(kLightColors);
  colors_choices_.emplace_back(kDarkColors);
  colors_choices_.emplace_back(kYellowColors);
  colors_choices_.shrink_to_fit();
}
bool ReadAnythingColorsModel::IsValidColorsIndex(size_t index) {
  return index < GetItemCount();
}

void ReadAnythingColorsModel::SetDefaultColorsIndexFromPref(size_t index) {
  default_index_ = index;
}

ReadAnythingColorsModel::ColorInfo& ReadAnythingColorsModel::GetColorsAt(
    size_t index) {
  return colors_choices_[index];
}

absl::optional<size_t> ReadAnythingColorsModel::GetDefaultIndex() const {
  return default_index_;
}

size_t ReadAnythingColorsModel::GetItemCount() const {
  return colors_choices_.size();
}

ui::ImageModel ReadAnythingColorsModel::GetIconAt(size_t index) const {
  // The dropdown should always show the color palette icon.
  return ui::ImageModel::FromImageSkia(gfx::CreateVectorIcon(
      kPaletteIcon, kColorsIconSize, colors_choices_[index].foreground));
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

std::u16string ReadAnythingColorsModel::GetItemAt(size_t index) const {
  // Only display the icon choice in the toolbar, so return empty string here.
  return std::u16string();
}

std::u16string ReadAnythingColorsModel::GetDropDownTextAt(size_t index) const {
  return colors_choices_[index].name;
}

ReadAnythingColorsModel::~ReadAnythingColorsModel() = default;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingLetterSpacingModel
///////////////////////////////////////////////////////////////////////////////

ReadAnythingLetterSpacingModel::ReadAnythingLetterSpacingModel() {
  letter_spacing_choices_.emplace_back(
      read_anything::mojom::LetterSpacing::kTight);
  letter_spacing_choices_.emplace_back(
      read_anything::mojom::LetterSpacing::kDefault);
  letter_spacing_choices_.emplace_back(
      read_anything::mojom::LetterSpacing::kLoose);
  letter_spacing_choices_.emplace_back(
      read_anything::mojom::LetterSpacing::kVeryLoose);
  letter_spacing_choices_.shrink_to_fit();
}

bool ReadAnythingLetterSpacingModel::IsValidLetterSpacingIndex(size_t index) {
  return index < GetItemCount();
}

void ReadAnythingLetterSpacingModel::SetDefaultLetterSpacingIndexFromPref(
    size_t index) {
  default_index_ = index;
}

absl::optional<size_t> ReadAnythingLetterSpacingModel::GetDefaultIndex() const {
  return default_index_;
}

size_t ReadAnythingLetterSpacingModel::GetItemCount() const {
  return letter_spacing_choices_.size();
}

read_anything::mojom::LetterSpacing
ReadAnythingLetterSpacingModel::GetLetterSpacingAt(size_t index) {
  return letter_spacing_choices_[index];
}

// TODO (crbug.com/1266555): Change to translatable messages
std::u16string ReadAnythingLetterSpacingModel::GetLetterSpacingName(
    read_anything::mojom::LetterSpacing letter_spacing) const {
  switch (letter_spacing) {
    case read_anything::mojom::LetterSpacing::kTight:
      return u"Tight";
    case read_anything::mojom::LetterSpacing::kDefault:
      return u"Default";
    case read_anything::mojom::LetterSpacing::kLoose:
      return u"Loose";
    case read_anything::mojom::LetterSpacing::kVeryLoose:
      return u"Very Loose";
  }
}

std::u16string ReadAnythingLetterSpacingModel::GetDropDownTextAt(
    size_t index) const {
  DCHECK_LT(index, GetItemCount());
  return GetLetterSpacingName(letter_spacing_choices_[index]);
}

std::u16string ReadAnythingLetterSpacingModel::GetItemAt(size_t index) const {
  // Only display the icon in the toolbar, so return empty string here.
  return std::u16string();
}

ui::ImageModel ReadAnythingLetterSpacingModel::GetIconAt(size_t index) const {
  return ui::ImageModel::FromImageSkia(gfx::CreateVectorIcon(
      kLetterSpacingIcon, kColorsIconSize, gfx::kPlaceholderColor));
}

ui::ImageModel ReadAnythingLetterSpacingModel::GetDropDownIconAt(
    size_t index) const {
  return ui::ImageModel();
}

ReadAnythingLetterSpacingModel::~ReadAnythingLetterSpacingModel() = default;
