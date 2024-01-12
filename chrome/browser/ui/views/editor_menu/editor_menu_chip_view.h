// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CHIP_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/md_text_button.h"

namespace chromeos::editor_menu {

struct PresetTextQuery;

// A label button with an icon and a rounded rectangle border.
class EditorMenuChipView : public views::MdTextButton {
  METADATA_HEADER(EditorMenuChipView, views::MdTextButton)

 public:
  EditorMenuChipView(views::Button::PressedCallback callback,
                     const PresetTextQuery& preset_text_query);
  EditorMenuChipView(const EditorMenuChipView&) = delete;
  EditorMenuChipView& operator=(const EditorMenuChipView&) = delete;
  ~EditorMenuChipView() override;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CHIP_VIEW_H_
