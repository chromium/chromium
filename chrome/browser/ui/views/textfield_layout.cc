// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/textfield_layout.h"

#include <utility>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

void AddLabelAndField(views::GridLayout* layout,
                      const base::string16& label_text,
                      std::unique_ptr<views::View> field,
                      int column_set_id,
                      const gfx::FontList& field_font) {
  constexpr int kFontContext = views::style::CONTEXT_LABEL;
  constexpr int kFontStyle = views::style::STYLE_PRIMARY;

  int row_height = views::LayoutProvider::GetControlHeightForFont(
      kFontContext, kFontStyle, field_font);
  layout->StartRow(views::GridLayout::kFixedSize, column_set_id, row_height);
  layout->AddView(
      std::make_unique<views::Label>(label_text, kFontContext, kFontStyle));
  layout->AddView(std::move(field));
}

}  // namespace

views::ColumnSet* ConfigureTextfieldStack(views::GridLayout* layout,
                                          int column_set_id) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int between_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);

  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(
      provider->GetControlLabelGridAlignment(), views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
  // TODO(tapted): This column may need some additional alignment logic under
  // Harmony so that its x-offset is not wholly dictated by the string length
  // of labels in the first column.
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, between_padding);

  // Note using FIXED here with a zero width will ignore the preferred (and
  // minimum) size of Views in the field column. Instead, fields will stretch to
  // fill the preferred size of the containing View, or the GridLayout's
  // minimum size.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::FIXED, 0, 0);
  return column_set;
}

views::Textfield* AddFirstTextfieldRow(views::GridLayout* layout,
                                       const base::string16& label,
                                       int column_set_id) {
  auto textfield = std::make_unique<views::Textfield>();
  textfield->SetAccessibleName(label);
  auto* textfield_ptr = textfield.get();
  AddLabelAndField(layout, label, std::move(textfield), column_set_id,
                   textfield_ptr->GetFontList());
  return textfield_ptr;
}

views::Textfield* AddTextfieldRow(views::GridLayout* layout,
                                  const base::string16& label,
                                  int column_set_id) {
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_CONTROL_LIST_VERTICAL));
  return AddFirstTextfieldRow(layout, label, column_set_id);
}

views::Combobox* AddComboboxRow(views::GridLayout* layout,
                                const base::string16& label,
                                std::unique_ptr<ui::ComboboxModel> model,
                                int column_set_id) {
  auto combobox = std::make_unique<views::Combobox>(std::move(model));
  combobox->SetAccessibleName(label);
  auto* combobox_ptr = combobox.get();
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_CONTROL_LIST_VERTICAL));
  AddLabelAndField(layout, label, std::move(combobox), column_set_id,
                   combobox_ptr->GetFontList());
  return combobox_ptr;
}
