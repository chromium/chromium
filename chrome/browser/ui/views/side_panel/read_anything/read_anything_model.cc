// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

ReadAnythingModel::ReadAnythingModel(std::string prefs_font_name)
    : font_model_(std::make_unique<ReadAnythingFontModel>()) {
  // If this profile has previously selected a preferred font name choice,
  // check that it is still a valid font, and if so, make it the default.
  if (font_model_->IsValidFontName(prefs_font_name)) {
    font_model_->SetDefaultIndexFromPrefsFontName(prefs_font_name);
    font_name_ = prefs_font_name;
  }

  // TODO(1266555): Add font size to users prefs and initialize here.
  font_size_ = 18.0f;
}

ReadAnythingModel::~ReadAnythingModel() = default;

void ReadAnythingModel::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
  NotifyFontNameUpdated();
  NotifyAXTreeDistilled();
}

void ReadAnythingModel::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void ReadAnythingModel::SetSelectedFontByIndex(int new_index) {
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
  font_size_ *= 0.83333f;
  NotifyFontSizeChanged();
}

void ReadAnythingModel::IncreaseTextSize() {
  font_size_ *= 1.2f;
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
    obs.OnFontSizeChanged(font_size_);
  }
}

ReadAnythingFontModel::ReadAnythingFontModel() {
  // TODO(1266555): Replace these with proper versions once finalized.
  font_choices_.emplace_back(u"Standard font");
  font_choices_.emplace_back(u"Sans-serif");
  font_choices_.emplace_back(u"Serif");
  font_choices_.emplace_back(u"Arial");
  font_choices_.emplace_back(u"Open Sans");
  font_choices_.emplace_back(u"Calibri");
  font_choices_.shrink_to_fit();
}

bool ReadAnythingFontModel::IsValidFontName(const std::string& font_name) {
  return std::find(font_choices_.begin(), font_choices_.end(),
                   base::UTF8ToUTF16(font_name)) != font_choices_.end();
}

bool ReadAnythingFontModel::IsValidFontIndex(int index) {
  return index >= 0 && index <= GetItemCount();
}

void ReadAnythingFontModel::SetDefaultIndexFromPrefsFontName(
    std::string prefs_font_name) {
  auto it = std::find(font_choices_.begin(), font_choices_.end(),
                      base::UTF8ToUTF16(prefs_font_name));
  default_index_ = it - font_choices_.begin();
}

int ReadAnythingFontModel::GetDefaultIndex() const {
  return default_index_;
}

int ReadAnythingFontModel::GetItemCount() const {
  return font_choices_.size();
}

std::u16string ReadAnythingFontModel::GetItemAt(int index) const {
  // TODO(1266555): Placeholder text, replace when finalized.
  return u"Default font";
}

std::u16string ReadAnythingFontModel::GetDropDownTextAt(int index) const {
  DCHECK(index >= 0 && index < GetItemCount());
  return font_choices_.at(index);
}

std::string ReadAnythingFontModel::GetFontNameAt(int index) {
  DCHECK(index >= 0 && index < GetItemCount());
  return base::UTF16ToUTF8(font_choices_.at(index));
}

ReadAnythingFontModel::~ReadAnythingFontModel() = default;
