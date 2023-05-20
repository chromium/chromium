// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_font_combobox.h"

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/combobox/combobox_menu_model.h"

// Adapts a ui::ComboboxModel for Read Anything.
class ReadAnythingFontCombobox::MenuModel : public ComboboxMenuModel {
 public:
  MenuModel(Combobox* owner, ui::ComboboxModel* model)
      : ComboboxMenuModel(owner, model) {}
  MenuModel(const MenuModel&) = delete;
  MenuModel& operator&(const MenuModel&) = delete;
  ~MenuModel() override = default;

  // Overridden from ComboboxMenuModel:
 private:
  // The Read Anything font combobox will not have icons on any platform.
  bool HasIcons() const override { return false; }
};

ReadAnythingFontCombobox::ReadAnythingFontCombobox(
    ReadAnythingFontCombobox::Delegate* delegate)
    : Combobox(std::move(delegate->GetFontComboboxModel())),
      delegate_(std::move(delegate)) {
  SetTooltipTextAndAccessibleName(
      l10n_util::GetStringUTF16(IDS_READING_MODE_FONT_NAME_COMBOBOX_LABEL));
  SetCallback(
      base::BindRepeating(&ReadAnythingFontCombobox::FontNameChangedCallback,
                          weak_pointer_factory_.GetWeakPtr()));

  std::unique_ptr<ComboboxMenuModel> new_model =
      std::make_unique<MenuModel>(this, GetModel());

  SetBorderColorId(ui::kColorSidePanelComboboxBorder);
  SetMenuModel(std::move(new_model));
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetEventHighlighting(true);
}

void ReadAnythingFontCombobox::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  Combobox::GetAccessibleNodeData(node_data);
  node_data->SetDescription(
      GetModel()->GetDropDownTextAt(GetSelectedIndex().value()));
}

void ReadAnythingFontCombobox::FontNameChangedCallback() {
  if (delegate_)
    delegate_->OnFontChoiceChanged(GetSelectedIndex().value());
}

gfx::Size ReadAnythingFontCombobox::GetMinimumSize() const {
  return gfx::Size(kMinimumComboboxWidth, CalculatePreferredSize().height());
}

void ReadAnythingFontCombobox::SetFocusRingColorId(
    ui::ColorId focus_ring_color) {
  DCHECK(views::FocusRing::Get(this));
  views::FocusRing::Get(this)->SetColorId(focus_ring_color);
}

void ReadAnythingFontCombobox::SetDropdownColorIds(ui::ColorId background_color,
                                                   ui::ColorId foreground_color,
                                                   ui::ColorId selected_color) {
  delegate_->GetFontComboboxModel()->SetForegroundColorId(foreground_color);
  delegate_->GetFontComboboxModel()->SetBackgroundColorId(background_color);
  delegate_->GetFontComboboxModel()->SetSelectedBackgroundColorId(
      selected_color);
}

BEGIN_METADATA(ReadAnythingFontCombobox, views::Combobox)
END_METADATA

ReadAnythingFontCombobox::~ReadAnythingFontCombobox() = default;
