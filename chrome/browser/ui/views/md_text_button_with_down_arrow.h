// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_
#define CHROME_BROWSER_UI_VIEWS_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/md_text_button.h"

namespace views {

class ButtonListener;

// The material design themed text button with a drop arrow displayed on the
// right side.
class MdTextButtonWithDownArrow : public MdTextButton {
 public:
  MdTextButtonWithDownArrow(ButtonListener* listener,
                            const base::string16& text);
  ~MdTextButtonWithDownArrow() override;

 protected:
  // views::MdTextButton:
  void OnThemeChanged() override;

 private:
  void SetDropArrowImage();

  DISALLOW_COPY_AND_ASSIGN(MdTextButtonWithDownArrow);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_MD_TEXT_BUTTON_WITH_DOWN_ARROW_H_
