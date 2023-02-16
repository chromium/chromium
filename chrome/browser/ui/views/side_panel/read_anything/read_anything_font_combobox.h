// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_FONT_COMBOBOX_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_FONT_COMBOBOX_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"

class ReadAnythingFontCombobox : public views::Combobox {
 public:
  METADATA_HEADER(ReadAnythingFontCombobox);
  class Delegate {
   public:
    virtual void OnFontChoiceChanged(int new_index) = 0;
    virtual ui::ComboboxModel* GetFontComboboxModel() = 0;
  };

  explicit ReadAnythingFontCombobox(
      ReadAnythingFontCombobox::Delegate* delegate);
  ReadAnythingFontCombobox(const ReadAnythingFontCombobox&) = delete;
  ReadAnythingFontCombobox& operator=(const ReadAnythingFontCombobox&) = delete;
  ~ReadAnythingFontCombobox() override;

  // views::Combobox:
  gfx::Size GetMinimumSize() const override;

 private:
  class MenuModel;

  void FontNameChangedCallback();

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  raw_ptr<ReadAnythingFontCombobox::Delegate> delegate_;

  base::WeakPtrFactory<ReadAnythingFontCombobox> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_FONT_COMBOBOX_H_
