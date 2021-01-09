// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_item_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"

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
    const LabInfo& lab,
    int default_index,
    const flags_ui::FeatureEntry* feature_entry,
    base::RepeatingCallback<void(ChromeLabsItemView* item_view)>
        combobox_callback)
    : feature_entry_(feature_entry) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                      DISTANCE_CONTROL_LIST_VERTICAL),
                  0)));

  AddChildView(views::Builder<views::Label>()
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetText(lab.visible_name)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .Build());
  AddChildView(
      views::Builder<views::Label>()
          .SetText(lab.visible_description)
          .SetTextContext(ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kPreferred,
                           views::MaximumFlexSizeRule::kPreferred, true))
          .SetBorder(views::CreateEmptyBorder(
              gfx::Insets(0, 0,
                          views::LayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_VERTICAL),
                          0)))
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
