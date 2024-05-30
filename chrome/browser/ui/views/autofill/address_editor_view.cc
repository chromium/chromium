// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_editor_view.h"

#include <cstddef>
#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

namespace {
// Returns the View ID that can be used to lookup the input field for |type|.
int GetInputFieldViewId(autofill::FieldType type) {
  return static_cast<int>(type);
}

}  // namespace

AddressEditorView::AddressEditorView(
    std::unique_ptr<AddressEditorController> controller)
    : controller_(std::move(controller)) {
  CreateEditorView();
}

AddressEditorView::~AddressEditorView() = default;

void AddressEditorView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  SizeToPreferredSize();
}

const autofill::AutofillProfile& AddressEditorView::GetAddressProfile() {
  if (controller_->is_validatable()) {
    ValidateAllFields();
    CHECK(controller_->is_valid().has_value() && *controller_->is_valid())
        << "The editor doesn't return an invalid profile, check "
           "`AddressEditorController::is_valid()` before calling this method.";
  }

  SaveFieldsToProfile();
  return controller_->GetAddressProfile();
}

bool AddressEditorView::ValidateAllFields() {
  if (!controller_->is_validatable()) {
    return true;
  }

  all_address_fields_have_been_validated_ = true;

  int number_of_invalid_fields = 0;
  for (const auto& field : text_fields_) {
    bool is_field_invalid =
        !controller_->IsValid(field.second, field.first->GetText());
    field.first->SetInvalid(is_field_invalid);
    number_of_invalid_fields += is_field_invalid;
  }

  bool is_valid = number_of_invalid_fields == 0;
  controller_->SetIsValid(is_valid);

  std::u16string validation_error;
  if (number_of_invalid_fields == 1) {
    validation_error = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELD_FORM_ERROR);
  } else if (number_of_invalid_fields > 1) {
    validation_error = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_EDIT_ADDRESS_REQUIRED_FIELDS_FORM_ERROR);
  }
  validation_error_->SetText(validation_error);

  return is_valid;
}

void AddressEditorView::SelectCountryForTesting(const std::u16string& country) {
  auto* combobox = static_cast<views::Combobox*>(
      GetViewByID(GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY)));
  CHECK(combobox->SelectValue(country));
  OnSelectedCountryChanged(combobox);
  UpdateEditorView();
}

void AddressEditorView::SetTextInputFieldValueForTesting(
    autofill::FieldType type,
    const std::u16string& value) {
  views::Textfield* text_field =
      static_cast<views::Textfield*>(GetViewByID(GetInputFieldViewId(type)));
  text_field->SetText(value);
}

std::u16string AddressEditorView::GetValidationErrorForTesting() const {
  return validation_error_ ? validation_error_->GetText() : u"";
}

void AddressEditorView::CreateEditorView() {
  text_fields_.clear();
  field_change_callbacks_.clear();

  const int kBetweenChildSpacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_CONTROL_LIST_VERTICAL);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kBetweenChildSpacing / 2, 0), kBetweenChildSpacing));

  views::View* first_field = nullptr;
  for (const auto& field : controller_->editor_fields()) {
    views::View* view = CreateInputField(field);
    if (first_field == nullptr) {
      first_field = view;
    }
  }
  initial_focus_view_ = first_field;

  if (controller_->is_validatable()) {
    validation_error_ =
        AddChildView(views::Builder<views::Label>()
                         .SetMultiLine(true)
                         .SetEnabledColorId(ui::kColorAlertHighSeverity)
                         .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                         .Build());
  }
}

// Field views have a width of 196/260dp (short/long fields) as per spec.
// __________________________________
// |Label | 16dp pad | Field (flex) |
// |______|__________|______________|
//
// Each input field is a 2 cells.
// +----------------------------------------------------------+
// | Field Label           | Input field (textfield/combobox) |
// +----------------------------------------------------------+
views::View* AddressEditorView::CreateInputField(const EditorField& field) {
  constexpr int kLabelWidth = 140;
  // This is the horizontal padding between the label and the field.
  constexpr int kLabelInputFieldHorizontalPadding = 16;
  constexpr int kShortFieldWidth = 196;
  constexpr int kLongFieldWidth = 260;
  constexpr int kInputFieldHeight = 28;

  views::Label* label;

  views::BoxLayoutView* field_layout = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetBetweenChildSpacing(kLabelInputFieldHorizontalPadding)
          .AddChildren(views::Builder<views::Label>()
                           .CopyAddressTo(&label)
                           .SetText(field.label)
                           .SetMultiLine(true)
                           .SetHorizontalAlignment(gfx::ALIGN_LEFT))
          .Build());

  label->SizeToFit(kLabelWidth);
  views::View* focusable_field = nullptr;

  switch (field.control_type) {
    case EditorField::ControlType::TEXTFIELD:
    case EditorField::ControlType::TEXTFIELD_NUMBER: {
      std::u16string initial_value = controller_->GetProfileInfo(field.type);

      auto text_field = std::make_unique<views::Textfield>();
      // Set the initial value and validity state.
      text_field->SetText(initial_value);
      text_field->GetViewAccessibility().SetName(field.label);

      if (field.control_type == EditorField::ControlType::TEXTFIELD_NUMBER) {
        text_field->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
      }

      // Using autofill field type as a view ID (for testing).
      text_field->SetID(GetInputFieldViewId(field.type));
      field_change_callbacks_.push_back(text_field->AddTextChangedCallback(
          base::BindRepeating(&AddressEditorView::ValidateField,
                              base::Unretained(this), text_field.get())));
      text_fields_.insert(std::make_pair(text_field.get(), field));

      field.length_hint == EditorField::LengthHint::HINT_SHORT
          ? text_field->SetPreferredSize(
                gfx::Size(kShortFieldWidth, kInputFieldHeight))
          : text_field->SetPreferredSize(
                gfx::Size(kLongFieldWidth, kInputFieldHeight));

      // |text_field| will now be owned by |row|.
      focusable_field = field_layout->AddChildView(std::move(text_field));
      break;
    }
    case EditorField::ControlType::COMBOBOX: {
      DCHECK_EQ(field.type, autofill::ADDRESS_HOME_COUNTRY);
      std::unique_ptr<views::Combobox> combobox =
          CreateCountryCombobox(field.label);
      // |combobox| will now be owned by |row|.
      focusable_field = field_layout->AddChildView(std::move(combobox));
      break;
    }
  }
  return focusable_field;
}

std::unique_ptr<views::Combobox> AddressEditorView::CreateCountryCombobox(
    const std::u16string& label) {
  auto& combobox_model = controller_->GetCountryComboboxModel();
  auto combobox = std::make_unique<views::Combobox>(&combobox_model);
  combobox->GetViewAccessibility().SetName(label);

  std::u16string initial_value =
      controller_->GetProfileInfo(autofill::ADDRESS_HOME_COUNTRY);

  // TODO(crbug.com/40277889): check if it's possible that address country is
  // not in the combobox value list.
  if (!combobox->SelectValue(initial_value)) {
    combobox->SelectValue(
        combobox_model.GetItemAt(combobox_model.GetDefaultIndex().value()));
  }

  // Using autofill field type as a view ID.
  combobox->SetID(GetInputFieldViewId(autofill::ADDRESS_HOME_COUNTRY));
  field_change_callbacks_.push_back(combobox->AddSelectedIndexChangedCallback(
      base::BindRepeating(&AddressEditorView::OnSelectedCountryChanged,
                          base::Unretained(this), combobox.get())));
  return combobox;
}

void AddressEditorView::UpdateEditorView() {
  validation_error_ = nullptr;
  initial_focus_view_ = nullptr;
  RemoveAllChildViews();
  CreateEditorView();
  PreferredSizeChanged();

  // If the editor was once fully validated (`ValidateAllFields()`), it should
  // keep validating the full address on any change. It ensures the error
  // messages are always consistent.
  if (all_address_fields_have_been_validated_) {
    ValidateAllFields();
  }

  if (initial_focus_view_) {
    initial_focus_view_->RequestFocus();
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
        combobox->GetTextForRow(combobox->GetSelectedIndex().value()));
    controller_->SetProfileInfo(autofill::ADDRESS_HOME_COUNTRY, country);
  }

  for (const auto& field : text_fields_) {
    controller_->SetProfileInfo(field.second.type, field.first->GetText());
  }
}

void AddressEditorView::OnSelectedCountryChanged(views::Combobox* combobox) {
  CHECK(combobox->GetSelectedIndex().has_value());
  SaveFieldsToProfile();
  size_t selected_index = combobox->GetSelectedIndex().value();
  CHECK(!controller_->GetCountryComboboxModel().IsItemSeparatorAt(
      selected_index));
  controller_->UpdateEditorFields(controller_->GetCountryComboboxModel()
                                      .countries()[selected_index]
                                      ->country_code());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AddressEditorView::UpdateEditorView,
                                weak_ptr_factory_.GetWeakPtr()));
}

void AddressEditorView::ValidateField(views::Textfield* textfield) {
  if (!controller_->is_validatable()) {
    return;
  }

  // If the editor was once fully validated (`ValidateAllFields()`), it should
  // keep validating the full address on any change. It ensures the error
  // messages are always consistent.
  if (all_address_fields_have_been_validated_) {
    ValidateAllFields();
    return;
  }

  const EditorField& field = text_fields_.at(textfield);
  bool is_valid = controller_->IsValid(field, textfield->GetText());
  textfield->SetInvalid(!is_valid);
}

BEGIN_METADATA(AddressEditorView)
END_METADATA

}  // namespace autofill
