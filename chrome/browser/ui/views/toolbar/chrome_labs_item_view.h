// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_ITEM_VIEW_H_

#include "base/callback.h"
#include "components/flags_ui/feature_entry.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/view.h"

struct LabInfo;

class ChromeLabsItemView : public views::View {
 public:
  ChromeLabsItemView(
      const LabInfo& lab,
      int default_index,
      const flags_ui::FeatureEntry* feature_entry,
      base::RepeatingCallback<void(ChromeLabsItemView* item_view)>
          combobox_callback);

  int GetSelectedIndex();

  views::Combobox* GetLabStateComboboxForTesting() {
    return lab_state_combobox_;
  }

  const flags_ui::FeatureEntry* GetFeatureEntry();

 private:
  // Combobox with selected state of the lab.
  views::Combobox* lab_state_combobox_;

  const flags_ui::FeatureEntry* feature_entry_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_ITEM_VIEW_H_
