// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_FONT_MODEL_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_FONT_MODEL_H_

#include <vector>

#include "ui/base/models/combobox_model.h"

class ReadAnythingFontModel : public ui::ComboboxModel {
 public:
  ReadAnythingFontModel();
  ReadAnythingFontModel(const ReadAnythingFontModel&) = delete;
  ~ReadAnythingFontModel() override;

  std::string GetCurrentFontName(int index);

 protected:
  // ui::Combobox implementation:
  int GetDefaultIndex() const override;
  int GetItemCount() const override;
  std::u16string GetItemAt(int index) const override;
  std::u16string GetDropDownTextAt(int index) const override;

 private:
  // Styled font names for the drop down options in front-end.
  std::vector<std::u16string> font_choices_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_FONT_MODEL_H_
