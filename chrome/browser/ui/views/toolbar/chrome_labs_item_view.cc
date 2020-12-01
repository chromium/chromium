// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_item_view.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

class LabsComboboxModel : public ui::ComboboxModel {
 public:
  explicit LabsComboboxModel(const flags_ui::FeatureEntry* feature_entry,
                             int default_index)
      : feature_entry_(feature_entry), default_index_(default_index) {}

  // ui::ComboboxModel:
  int GetItemCount() const override { return feature_entry_->NumOptions(); }

  base::string16 GetItemAt(int index) const override {
    // TODO(elainechien): remove white space for description
    return feature_entry_->DescriptionForOption(index);
  }

  int GetDefaultIndex() const override { return default_index_; }

 private:
  const flags_ui::FeatureEntry* feature_entry_;
  int default_index_;
};

ChromeLabsItemView::ChromeLabsItemView(
    std::string internal_name,
    int default_index,
    const flags_ui::FeatureEntry* feature_entry,
    base::RepeatingCallback<void(ChromeLabsItemView* item_view)>
        combobox_callback)
    : internal_name_(internal_name), feature_entry_(feature_entry) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey, gfx::Insets(10));
  AddChildView(views::Builder<views::Label>()
                   .SetText(base::ASCIIToUTF16(feature_entry->visible_name))
                   .Build());
  AddChildView(views::Builder<views::Combobox>()
                   .CopyAddressTo(&lab_state_combobox_)
                   .SetOwnedModel(std::make_unique<LabsComboboxModel>(
                       feature_entry_, default_index))
                   .SetCallback(base::BindRepeating(combobox_callback, this))
                   .Build());
}

int ChromeLabsItemView::GetSelectedIndex() {
  return lab_state_combobox_->GetSelectedIndex();
}

const flags_ui::FeatureEntry* ChromeLabsItemView::GetFeatureEntry() {
  return feature_entry_;
}
