// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_

#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/md_text_button.h"

namespace views {

// The material design themed text button with a drop arrow displayed on the
// right side.
class MdTextButtonWithDownArrow : public MdTextButton {
 public:
  METADATA_HEADER(MdTextButtonWithDownArrow);

  MdTextButtonWithDownArrow(PressedCallback callback,
                            const std::u16string& text);
  MdTextButtonWithDownArrow(const MdTextButtonWithDownArrow&) = delete;
  MdTextButtonWithDownArrow& operator=(const MdTextButtonWithDownArrow&) =
      delete;
  ~MdTextButtonWithDownArrow() override;

 protected:
  // views::MdTextButton:
  void OnThemeChanged() override;

 private:
  void SetDropArrowImage();
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_
