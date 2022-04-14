// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/view.h"

// Generic View for the toolbar of the Read Anything side panel.
class ReadAnythingToolbarView : public views::View {
 public:
  class Delegate {
   public:
    virtual void OnFontChoiceChanged(int new_choice) = 0;
  };

  explicit ReadAnythingToolbarView(ReadAnythingToolbarView::Delegate* delegate);
  ReadAnythingToolbarView(const ReadAnythingToolbarView&) = delete;
  ReadAnythingToolbarView& operator=(const ReadAnythingToolbarView&) = delete;
  ~ReadAnythingToolbarView() override;

  // View bindings for the font style combobox.
  void SetFontModel(ui::ComboboxModel* model);

 private:
  void FontNameChangedCallback();

  raw_ptr<views::Combobox> font_combobox_;
  raw_ptr<ReadAnythingToolbarView::Delegate> delegate_;

  base::WeakPtrFactory<ReadAnythingToolbarView> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOOLBAR_VIEW_H_
