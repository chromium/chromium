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
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

using read_anything::mojom::ReadAnythingTheme;
using read_anything::mojom::Spacing;

ReadAnythingModel::ReadAnythingModel()
    : font_name_(kReadAnythingDefaultFontName),
      font_scale_(kReadAnythingDefaultFontScale),
      font_model_(std::make_unique<ReadAnythingFontModel>()),
      colors_model_(std::make_unique<ReadAnythingColorsModel>()),
      line_spacing_model_(std::make_unique<ReadAnythingLineSpacingModel>()),
      letter_spacing_model_(
          std::make_unique<ReadAnythingLetterSpacingModel>()) {}

ReadAnythingModel::~ReadAnythingModel() = default;

void ReadAnythingModel::Init(std::string& font_name,
                             double font_scale,
                             read_anything::mojom::Colors colors,
                             Spacing line_spacing,
                             Spacing letter_spacing) {
  // If this profile has previously selected choices that were saved to
  // prefs, check they are still a valid, and then assign if so.
  if (font_model_->IsValidFontName(font_name)) {
    font_model_->SetDefaultIndexFromPrefsFontName(font_name);
    font_name_ = font_name;
  }

  font_scale_ = font_scale;

  size_t colors_index = static_cast<size_t>(colors);
  if (colors_model_->IsValidColorsIndex(colors_index)) {
    colors_model_->SetDefaultColorsIndexFromPref(colors_index);
  }

  colors_combobox_index_ = colors_model_->GetStartingStateIndex();
  auto& initial_colors = colors_model_->GetColorsAt(colors_combobox_index_);
  foreground_color_ = initial_colors.foreground;
  background_color_ = initial_colors.background;

  size_t line_spacing_index = static_cast<size_t>(line_spacing);
  if (line_spacing_model_->IsValidLineSpacingIndex(line_spacing_index)) {
    line_spacing_model_->SetDefaultLineSpacingIndexFromPref(line_spacing_index);
    line_spacing_ = line_spacing_model_->GetLineSpacingAt(line_spacing_index);
  }

  size_t letter_spacing_index = static_cast<size_t>(letter_spacing);
  if (letter_spacing_model_->IsValidLetterSpacingIndex(letter_spacing_index)) {
    letter_spacing_model_->SetDefaultLetterSpacingIndexFromPref(
        letter_spacing_index);
    letter_spacing_ =
        letter_spacing_model_->GetLetterSpacingAt(letter_spacing_index);
  }
}

void ReadAnythingModel::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
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

  colors_combobox_index_ = new_index;
  auto& new_colors = colors_model_->GetColorsAt(new_index);
  foreground_color_ = new_colors.foreground;
  background_color_ = new_colors.background;

  NotifyThemeChanged();
}

ui::ColorId ReadAnythingModel::GetForegroundColorId() {
  // Check that the index is valid.
  DCHECK(colors_model_->IsValidColorsIndex(colors_combobox_index_));

  return colors_model_->GetForegroundColorId(colors_combobox_index_);
}

void ReadAnythingModel::SetSelectedLineSpacingByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(line_spacing_model_->IsValidLineSpacingIndex(new_index));

  line_spacing_ = line_spacing_model_->GetLineSpacingAt(new_index);
  NotifyThemeChanged();
}

void ReadAnythingModel::SetSelectedLetterSpacingByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(letter_spacing_model_->IsValidLetterSpacingIndex(new_index));

  letter_spacing_ = letter_spacing_model_->GetLetterSpacingAt(new_index);
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

void ReadAnythingModel::NotifyAXTreeDistilled() {
  // The snapshot must have a valid root id.
  DCHECK(snapshot_.root_id != ui::kInvalidAXNodeID);
  for (Observer& obs : observers_) {
    obs.OnAXTreeDistilled(snapshot_, content_node_ids_);
  }
}

void ReadAnythingModel::NotifyThemeChanged() {
  for (Observer& obs : observers_) {
    obs.OnReadAnythingThemeChanged(ReadAnythingTheme::New(
        font_name_, font_scale_, foreground_color_, background_color_,
        line_spacing_, letter_spacing_));
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
  return base::Contains(font_choices_, base::UTF8ToUTF16(font_name));
}

bool ReadAnythingFontModel::IsValidFontIndex(size_t index) {
  return index < GetItemCount();
}

void ReadAnythingFontModel::SetDefaultIndexFromPrefsFontName(
    std::string prefs_font_name) {
  auto it =
      base::ranges::find(font_choices_, base::UTF8ToUTF16(prefs_font_name));
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
                              gfx::kGoogleGrey800, gfx::kGoogleGrey050,
                              ui::kColorReadAnythingForegroundLight};

  ColorInfo kLightColors = {u"Light", IDS_READ_ANYTHING_LIGHT_PNG,
                            gfx::kGoogleGrey800, gfx::kGoogleGrey050,
                            ui::kColorReadAnythingForegroundLight};

  ColorInfo kDarkColors = {u"Dark", IDS_READ_ANYTHING_DARK_PNG,
                           gfx::kGoogleGrey200, gfx::kGoogleGrey900,
                           ui::kColorReadAnythingForegroundDark};

  ColorInfo kYellowColors = {u"Yellow", IDS_READ_ANYTHING_YELLOW_PNG,
                             gfx::kGoogleGrey800, gfx::kGoogleYellow200,
                             ui::kColorReadAnythingForegroundYellow};

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

ui::ColorId ReadAnythingColorsModel::GetForegroundColorId(size_t index) {
  return GetColorsAt(index).foreground_color_id;
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
// ReadAnythingLineSpacingModel
///////////////////////////////////////////////////////////////////////////////

ReadAnythingLineSpacingModel::ReadAnythingLineSpacingModel() {
  // Define the line spacing options available to the user.
  lines_choices_.emplace_back(Spacing::kTight);
  lines_choices_.emplace_back(Spacing::kDefault);
  lines_choices_.emplace_back(Spacing::kLoose);
  lines_choices_.emplace_back(Spacing::kVeryLoose);
  lines_choices_.shrink_to_fit();
}

bool ReadAnythingLineSpacingModel::IsValidLineSpacingIndex(size_t index) {
  return index < GetItemCount();
}

void ReadAnythingLineSpacingModel::SetDefaultLineSpacingIndexFromPref(
    size_t index) {
  default_index_ = index;
}

Spacing ReadAnythingLineSpacingModel::GetLineSpacingAt(size_t index) {
  DCHECK_LT(index, GetItemCount());

  return lines_choices_[index];
}

absl::optional<size_t> ReadAnythingLineSpacingModel::GetDefaultIndex() const {
  return default_index_;
}

size_t ReadAnythingLineSpacingModel::GetItemCount() const {
  return lines_choices_.size();
}

ui::ImageModel ReadAnythingLineSpacingModel::GetIconAt(size_t index) const {
  // The dropdown should always show the line spacing icon.
  return ui::ImageModel::FromImageSkia(gfx::CreateVectorIcon(
      kLineSpacingIcon, kColorsIconSize, gfx::kPlaceholderColor));
}

ui::ImageModel ReadAnythingLineSpacingModel::GetDropDownIconAt(
    size_t index) const {
  return ui::ImageModel();
}

std::u16string ReadAnythingLineSpacingModel::GetItemAt(size_t index) const {
  // Only display the icon in the toolbar, so return empty string here.
  return std::u16string();
}

std::u16string ReadAnythingLineSpacingModel::GetLineSpacingName(
    Spacing line_spacing) const {
  switch (line_spacing) {
    case Spacing::kTight:
      return u"Tight";
    case Spacing::kDefault:
      return u"Default";
    case Spacing::kLoose:
      return u"Loose";
    case Spacing::kVeryLoose:
      return u"Very Loose";
  }
}

std::u16string ReadAnythingLineSpacingModel::GetDropDownTextAt(
    size_t index) const {
  DCHECK_LT(index, GetItemCount());
  return GetLineSpacingName(lines_choices_[index]);
}

ReadAnythingLineSpacingModel::~ReadAnythingLineSpacingModel() = default;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingLetterSpacingModel
///////////////////////////////////////////////////////////////////////////////

ReadAnythingLetterSpacingModel::ReadAnythingLetterSpacingModel() {
  letter_spacing_choices_.emplace_back(Spacing::kTight);
  letter_spacing_choices_.emplace_back(Spacing::kDefault);
  letter_spacing_choices_.emplace_back(Spacing::kLoose);
  letter_spacing_choices_.emplace_back(Spacing::kVeryLoose);
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

Spacing ReadAnythingLetterSpacingModel::GetLetterSpacingAt(size_t index) {
  return letter_spacing_choices_[index];
}

// TODO (crbug.com/1266555): Change to translatable messages
std::u16string ReadAnythingLetterSpacingModel::GetLetterSpacingName(
    Spacing letter_spacing) const {
  int label;
  switch (letter_spacing) {
    case Spacing::kTight:
      label = IDS_READ_ANYTHING_SPACING_COMBOBOX_TIGHT;
      break;
    case Spacing::kDefault:
      label = IDS_READ_ANYTHING_SPACING_COMBOBOX_DEFAULT;
      break;
    case Spacing::kLoose:
      label = IDS_READ_ANYTHING_SPACING_COMBOBOX_LOOSE;
      break;
    case Spacing::kVeryLoose:
      label = IDS_READ_ANYTHING_SPACING_COMBOBOX_VERY_LOOSE;
  }
  return l10n_util::GetStringUTF16(label);
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
