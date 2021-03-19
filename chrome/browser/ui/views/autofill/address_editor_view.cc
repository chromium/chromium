// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_editor_view.h"

#include "chrome/browser/ui/autofill/address_editor_controller.h"
#include "ui/views/border.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

namespace {
// Returns the View ID that can be used to lookup the input field for |type|.
int GetInputFieldViewId(autofill::ServerFieldType type) {
  return static_cast<int>(type);
}

}  // namespace

AddressEditorView::AddressEditorView(AddressEditorController* controller)
    : controller_(controller) {
  CreateEditorView();
}

AddressEditorView::~AddressEditorView() = default;

void AddressEditorView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  SizeToPreferredSize();
}

const autofill::AutofillProfile& AddressEditorView::GetAddressProfile() {
  SaveFieldsToProfile();
  return controller_->GetAddressProfile();
}

void AddressEditorView::SetTextInputFieldValueForTesting(
    autofill::ServerFieldType type,
    const std::u16string& value) {
  views::Textfield* text_field =
      static_cast<views::Textfield*>(GetViewByID(GetInputFieldViewId(type)));
  text_field->SetText(value);
}

void AddressEditorView::CreateEditorView() {
  text_fields_.clear();
  constexpr int kRowHorizontalInsets = 16;

  // The editor view is padded horizontally.
  SetBorder(views::CreateEmptyBorder(0, kRowHorizontalInsets, 0,
                                     kRowHorizontalInsets));

  // All views have fixed size except the Field which stretches. The fixed
  // padding at the end is computed so that Field views have a minimum of
  // 176/272dp (short/long fields) as per spec.
  // ______________________________________________________
  // |Label | 16dp pad | Field (flex) |  Computed Padding |
  // |______|__________|______________|___________________|
  constexpr int kLabelWidth = 140;
  constexpr int kDialogMinWidth = 512;
  // This is the horizontal padding between the label and the field.
  constexpr int kLabelInputFieldHorizontalPadding = 16;
  constexpr int kShortFieldMinimumWidth = 176;
  constexpr int kLongFieldMinimumWidth = 272;

  using ColumnSize = views::GridLayout::ColumnSize;
  views::GridLayout* editor_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  // Column set for short fields.
  views::ColumnSet* columns_short = editor_layout->AddColumnSet(0);
  columns_short->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, ColumnSize::kFixed, kLabelWidth, 0);
  columns_short->AddPaddingColumn(views::GridLayout::kFixedSize,
                                  kLabelInputFieldHorizontalPadding);
  // The field view column stretches.
  columns_short->AddColumn(views::GridLayout::LEADING,
                           views::GridLayout::CENTER, 1.0,
                           ColumnSize::kUsePreferred, 0, 0);
  // The padding at the end is fixed, computed to make sure the short field
  // maintains its minimum width.
  int short_padding = kDialogMinWidth - kShortFieldMinimumWidth - kLabelWidth -
                      (2 * kRowHorizontalInsets) -
                      kLabelInputFieldHorizontalPadding;
  columns_short->AddPaddingColumn(views::GridLayout::kFixedSize, short_padding);

  // Column set for long fields.
  views::ColumnSet* columns_long = editor_layout->AddColumnSet(1);
  columns_long->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          views::GridLayout::kFixedSize, ColumnSize::kFixed,
                          kLabelWidth, 0);
  columns_long->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 kLabelInputFieldHorizontalPadding);
  // The field view column stretches.
  columns_long->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1.0, ColumnSize::kUsePreferred, 0, 0);

  // The padding at the end is fixed, computed to make sure the long field
  // maintains its minimum width.
  int long_padding = kDialogMinWidth - kLongFieldMinimumWidth - kLabelWidth -
                     (2 * kRowHorizontalInsets) -
                     kLabelInputFieldHorizontalPadding;
  columns_long->AddPaddingColumn(views::GridLayout::kFixedSize, long_padding);

  for (const auto& field : controller_->editor_fields()) {
    CreateInputField(editor_layout, field);
  }
}

// Each input field is a 2 cells.
// +----------------------------------------------------------+
// | Field Label           | Input field (textfield/combobox) |
// +----------------------------------------------------------+
views::View* AddressEditorView::CreateInputField(views::GridLayout* layout,
                                                 const EditorField& field) {
  int column_set =
      field.length_hint == EditorField::LengthHint::HINT_SHORT ? 0 : 1;

  // This is the top padding for every row.
  constexpr int kInputRowSpacing = 6;
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, column_set,
                              views::GridLayout::kFixedSize, kInputRowSpacing);

  std::unique_ptr<views::Label> label =
      std::make_unique<views::Label>(field.label);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(std::move(label));

  views::View* focusable_field = nullptr;
  constexpr int kInputFieldHeight = 28;

  switch (field.control_type) {
    case EditorField::ControlType::TEXTFIELD:
    case EditorField::ControlType::TEXTFIELD_NUMBER: {
      std::u16string initial_value = controller_->GetProfileInfo(field.type);

      auto text_field = std::make_unique<views::Textfield>();
      // Set the initial value and validity state.
      text_field->SetText(initial_value);
      text_field->SetAccessibleName(field.label);

      if (field.control_type == EditorField::ControlType::TEXTFIELD_NUMBER)
        text_field->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);

      // Using autofill field type as a view ID (for testing).
      text_field->SetID(GetInputFieldViewId(field.type));
      text_fields_.insert(std::make_pair(text_field.get(), field));

      // |text_field| will now be owned by |row|.
      focusable_field =
          layout->AddView(std::move(text_field), 1.0, 1.0,
                          views::GridLayout::FILL, views::GridLayout::FILL,
                          views::GridLayout::kFixedSize, kInputFieldHeight);
      break;
    }
    case EditorField::ControlType::COMBOBOX: {
      DCHECK_EQ(field.type, autofill::ADDRESS_HOME_COUNTRY);
      std::unique_ptr<views::Combobox> combobox =
          CreateCountryCombobox(field.label);
      // |combobox| will now be owned by |row|.
      focusable_field =
          layout->AddView(std::move(combobox), 1.0, 1.0,
                          views::GridLayout::FILL, views::GridLayout::FILL,
                          views::GridLayout::kFixedSize, kInputFieldHeight);
      break;
    }
  }
  // Bottom padding for the row.
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kInputRowSpacing);
  return focusable_field;
}

std::unique_ptr<views::Combobox> AddressEditorView::CreateCountryCombobox(
    const std::u16string& label) {
  auto combobox =
      std::make_unique<views::Combobox>(controller_->GetCountryComboboxModel());
  combobox->SetAccessibleName(label);

  std::u16string initial_value =
      controller_->GetProfileInfo(autofill::ADDRESS_HOME_COUNTRY);

  if (!initial_value.empty())
    combobox->SelectValue(initial_value);

  // Using autofill field type as a view ID.
  combobox->SetID(GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY));
  combobox->SetCallback(base::BindRepeating(&AddressEditorView::OnPerformAction,
                                            base::Unretained(this),
                                            combobox.get()));
  return combobox;
}

void AddressEditorView::UpdateEditorView() {
  RemoveAllChildViews(true);
  CreateEditorView();
  PreferredSizeChanged();

  if (controller_->chosen_country_index() > 0UL &&
      controller_->chosen_country_index() < controller_->GetCountriesSize()) {
    views::Combobox* country_combo_box = static_cast<views::Combobox*>(
        GetViewByID(GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY)));
    DCHECK(country_combo_box);
    DCHECK_EQ(controller_->GetCountriesSize(),
              static_cast<size_t>(country_combo_box->GetRowCount()));
    country_combo_box->SetSelectedIndex(controller_->chosen_country_index());
  } else if (controller_->GetCountriesSize() > 0UL) {
    controller_->set_chosen_country_index(0UL);
  } else {
    controller_->set_chosen_country_index(kInvalidCountryIndex);
  }
}

void AddressEditorView::SaveFieldsToProfile() {
  // The country must be set first, because the profile uses the country to
  // interpret some of the data (e.g., phone numbers) passed to SetInfo.
  views::Combobox* combobox = static_cast<views::Combobox*>(
      GetViewByID(GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY)));
  // The combobox can be null when saving to temporary profile while updating
  // the view.
  if (combobox) {
    std::u16string country(
        combobox->GetTextForRow(combobox->GetSelectedIndex()));
    controller_->SetProfileInfo(autofill::ADDRESS_HOME_COUNTRY, country);
  }

  for (const auto& field : text_fields_) {
    controller_->SetProfileInfo(field.second.type, field.first->GetText());
  }
}

void AddressEditorView::OnPerformAction(views::Combobox* combobox) {
  if (combobox->GetID() != GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY))
    return;
  DCHECK_GE(combobox->GetSelectedIndex(), 0);
  if (controller_->chosen_country_index() !=
      static_cast<size_t>(combobox->GetSelectedIndex())) {
    controller_->set_chosen_country_index(combobox->GetSelectedIndex());
    OnDataChanged();
  }
}

void AddressEditorView::OnDataChanged() {
  SaveFieldsToProfile();
  controller_->UpdateEditorFields();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AddressEditorView::UpdateEditorView,
                                weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace autofill
