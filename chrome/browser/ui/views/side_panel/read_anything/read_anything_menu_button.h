// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MENU_BUTTON_H_

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/menu_button.h"

namespace views {
class MenuRunner;
}  // namespace views

class ReadAnythingMenuModel;

class ReadAnythingMenuButton : public views::MenuButton {
 public:
  METADATA_HEADER(ReadAnythingMenuButton);
  ReadAnythingMenuButton(base::RepeatingCallback<void()> callback,
                         const gfx::VectorIcon& icon,
                         const std::u16string& tooltip,
                         ReadAnythingMenuModel* menu_model);
  ReadAnythingMenuButton(const ReadAnythingMenuButton&) = delete;
  ReadAnythingMenuButton& operator=(const ReadAnythingMenuButton&) = delete;
  ~ReadAnythingMenuButton() override;

  void SetMenuModel(ReadAnythingMenuModel* menu_model);
  ReadAnythingMenuModel* GetMenuModel() const;
  absl::optional<size_t> GetSelectedIndex() const;
  void SetIcon(const gfx::VectorIcon& icon, int icon_size, SkColor icon_color);

 private:
  void ButtonPressed();

  raw_ptr<ReadAnythingMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MENU_BUTTON_H_
