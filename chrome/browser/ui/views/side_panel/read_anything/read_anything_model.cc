// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"

ReadAnythingModel::ReadAnythingModel()
    : font_name_(kReadAnythingDefaultFontName),
      font_scale_(kReadAnythingDefaultFontScale),
      font_model_(std::make_unique<ReadAnythingFontModel>()) {}

ReadAnythingModel::~ReadAnythingModel() = default;

void ReadAnythingModel::Init(std::string& font_name, double font_scale) {
  // If this profile has previously selected a preferred font name choice,
  // check that it is still a valid font, and then assign if so.
  if (font_model_->IsValidFontName(font_name)) {
    font_model_->SetDefaultIndexFromPrefsFontName(font_name);
    font_name_ = font_name;
  }

  font_scale_ = font_scale;
}

void ReadAnythingModel::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
  NotifyFontNameUpdated();
  NotifyAXTreeDistilled();
  NotifyFontSizeChanged();
}

void ReadAnythingModel::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void ReadAnythingModel::SetSelectedFontByIndex(size_t new_index) {
  // Check that the index is valid.
  DCHECK(font_model_->IsValidFontIndex(new_index));

  // Update state and notify listeners
  font_name_ = font_model_->GetFontNameAt(new_index);
  NotifyFontNameUpdated();
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

  NotifyFontSizeChanged();
}

void ReadAnythingModel::IncreaseTextSize() {
  font_scale_ += 0.2;
  if (font_scale_ > kReadAnythingMaximumFontScale)
    font_scale_ = kReadAnythingMaximumFontScale;

  NotifyFontSizeChanged();
}

void ReadAnythingModel::NotifyFontNameUpdated() {
  for (Observer& obs : observers_) {
    obs.OnFontNameUpdated(font_name_);
  }
}

void ReadAnythingModel::NotifyAXTreeDistilled() {
  for (Observer& obs : observers_) {
    obs.OnAXTreeDistilled(snapshot_, content_node_ids_);
  }
}

void ReadAnythingModel::NotifyFontSizeChanged() {
  for (Observer& obs : observers_) {
    obs.OnFontSizeChanged(kReadAnythingDefaultFontSize * font_scale_);
  }
}

ReadAnythingFontModel::ReadAnythingFontModel() {
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
  // TODO(1266555): Placeholder text, replace when finalized.
  return u"Default font";
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
