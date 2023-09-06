// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CHIP_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

namespace chromeos::editor_menu {

struct PresetTextQuery;

// A label button with an icon and a rounded rectangle border.
class EditorMenuChipView : public views::LabelButton {
 public:
  METADATA_HEADER(EditorMenuChipView);

  EditorMenuChipView(views::Button::PressedCallback callback,
                     const PresetTextQuery& preset_text_query);
  EditorMenuChipView(const EditorMenuChipView&) = delete;
  EditorMenuChipView& operator=(const EditorMenuChipView&) = delete;
  ~EditorMenuChipView() override;

  // views::LabelButton:
  void AddedToWidget() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  void InitLayout();

  const raw_ptr<const gfx::VectorIcon> icon_;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CHIP_VIEW_H_
