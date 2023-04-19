// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MENU_MODEL_H_

#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/font_list.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingMenuModel
//
//  This class makes the menu (dropdown) for the ReadAnythingMenuButton.
//
class ReadAnythingMenuModel : public ui::SimpleMenuModel,
                              public ui::SimpleMenuModel::Delegate {
 public:
  ReadAnythingMenuModel();
  ReadAnythingMenuModel(const ReadAnythingMenuModel&) = delete;
  ReadAnythingMenuModel& operator=(const ReadAnythingMenuModel&) = delete;
  ~ReadAnythingMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  virtual bool IsValidIndex(size_t index);
  void SetSelectedIndex(size_t index);
  absl::optional<size_t> GetSelectedIndex() const { return selected_index_; }
  void SetCallback(base::RepeatingCallback<void()> callback);

  absl::optional<ui::ColorId> GetForegroundColorId(size_t index) override;
  absl::optional<ui::ColorId> GetSubmenuBackgroundColorId(
      size_t index) override;
  absl::optional<ui::ColorId> GetSelectedBackgroundColorId(
      size_t index) override;

  void SetForegroundColorId(ui::ColorId foreground_color) {
    foreground_color_id_ = foreground_color;
  }

  void SetSubmenuBackgroundColorId(ui::ColorId background_color) {
    submenu_background_color_id_ = background_color;
  }

  void SetSelectedBackgroundColorId(ui::ColorId selected_color) {
    selected_color_id_ = selected_color;
  }

  void SetLabelFontList(const std::string& font_string);

  const gfx::FontList* GetLabelFontListAt(size_t index) const override;

 private:
  absl::optional<size_t> selected_index_ = absl::nullopt;
  base::RepeatingClosure callback_;
  absl::optional<ui::ColorId> foreground_color_id_;
  absl::optional<ui::ColorId> submenu_background_color_id_;
  absl::optional<ui::ColorId> selected_color_id_;
  absl::optional<gfx::FontList> font_ = absl::nullopt;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MENU_MODEL_H_
