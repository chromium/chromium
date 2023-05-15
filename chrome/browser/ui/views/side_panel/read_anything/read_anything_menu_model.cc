// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_menu_model.h"
#include "chrome/common/accessibility/read_anything_constants.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingMenuModel
///////////////////////////////////////////////////////////////////////////////

ReadAnythingMenuModel::ReadAnythingMenuModel() : ui::SimpleMenuModel(this) {}

ReadAnythingMenuModel::~ReadAnythingMenuModel() = default;

void ReadAnythingMenuModel::SetCallback(
    base::RepeatingCallback<void()> callback) {
  callback_ = std::move(callback);
}

bool ReadAnythingMenuModel::IsCommandIdChecked(int command_id) const {
  return selected_index_.has_value() &&
         static_cast<size_t>(command_id) == selected_index_.value();
}

void ReadAnythingMenuModel::ExecuteCommand(int command_id, int event_flags) {
  selected_index_ = command_id;
  if (callback_)
    callback_.Run();
}

void ReadAnythingMenuModel::SetSelectedIndex(size_t index) {
  selected_index_ = index;
}

bool ReadAnythingMenuModel::IsValidIndex(size_t index) {
  return false;
}

absl::optional<ui::ColorId> ReadAnythingMenuModel::GetForegroundColorId(
    size_t index) {
  return foreground_color_id_;
}

absl::optional<ui::ColorId> ReadAnythingMenuModel::GetSubmenuBackgroundColorId(
    size_t index) {
  return submenu_background_color_id_;
}

absl::optional<ui::ColorId> ReadAnythingMenuModel::GetSelectedBackgroundColorId(
    size_t index) {
  return selected_color_id_;
}

const gfx::FontList* ReadAnythingMenuModel::GetLabelFontListAt(
    size_t index) const {
  if (font_.has_value()) {
    return &font_.value();
  }

  return nullptr;
}

void ReadAnythingMenuModel::SetLabelFontList(const std::string& font_string) {
  std::vector<std::string> font_names = {
      font_string, string_constants::kReadAnythingDefaultFontName};
  font_ = gfx::FontList(font_names, gfx::Font::FontStyle::NORMAL,
                        kMenuLabelFontSizePx, gfx::Font::Weight::NORMAL);
}
