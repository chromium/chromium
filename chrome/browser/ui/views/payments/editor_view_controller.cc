// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/editor_view_controller.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {
namespace {

enum class EditorViewControllerTags : int {
  // The tag for the button that saves the model being edited. Starts
  // at PAYMENT_REQUEST_COMMON_TAG_MAX not to conflict with tags
  // common to all views.
  SAVE_BUTTON = static_cast<int>(
      PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX),
};

std::unique_ptr<views::View> CreateErrorLabelView(
    const base::string16& error,
    autofill::ServerFieldType type) {
  std::unique_ptr<views::View> view = std::make_unique<views::View>();

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  // This is the space between the input field and the error label.
  constexpr int kErrorLabelTopPadding = 6;
  layout->set_inside_border_insets(gfx::Insets(kErrorLabelTopPadding, 0, 0, 0));
  view->SetLayoutManager(std::move(layout));

  std::unique_ptr<views::Label> error_label =
      std::make_unique<views::Label>(error, CONTEXT_BODY_TEXT_SMALL);
  error_label->SetID(static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET) + type);
  error_label->SetEnabledColor(error_label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_AlertSeverityHigh));
  error_label->SetMultiLine(true);
  error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  view->AddChildView(std::move(error_label));
  return view;
}

}  // namespace

EditorViewController::EditorViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    BackNavigationType back_navigation_type,
    bool is_incognito)
    : PaymentRequestSheetController(spec, state, dialog),
      initial_focus_field_view_(nullptr),
      back_navigation_type_(back_navigation_type),
      is_incognito_(is_incognito) {}

EditorViewController::~EditorViewController() {}

void EditorViewController::DisplayErrorMessageForField(
    autofill::ServerFieldType type,
    const base::string16& error_message) {
  AddOrUpdateErrorMessageForField(type, error_message);
  RelayoutPane();
}

// static
int EditorViewController::GetInputFieldViewId(autofill::ServerFieldType type) {
  return static_cast<int>(DialogViewID::INPUT_FIELD_TYPE_OFFSET) +
         static_cast<int>(type);
}

std::unique_ptr<views::View> EditorViewController::CreateHeaderView() {
  return nullptr;
}

std::unique_ptr<views::View> EditorViewController::CreateCustomFieldView(
    autofill::ServerFieldType type,
    views::View** focusable_field,
    bool* valid,
    base::string16* error_message) {
  return nullptr;
}

std::unique_ptr<views::View> EditorViewController::CreateExtraViewForField(
    autofill::ServerFieldType type) {
  return nullptr;
}

bool EditorViewController::ValidateInputFields() {
  for (const auto& field : text_fields()) {
    if (!field.first->IsValid())
      return false;
  }
  for (const auto& field : comboboxes()) {
    if (!field.first->IsValid())
      return false;
  }
  return true;
}

std::unique_ptr<views::Button> EditorViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(IDS_DONE)));
  button->set_tag(static_cast<int>(EditorViewControllerTags::SAVE_BUTTON));
  button->SetID(static_cast<int>(DialogViewID::EDITOR_SAVE_BUTTON));
  return button;
}

bool EditorViewController::ShouldShowSecondaryButton() {
  // Do not show the "Cancel Payment" button.
  return false;
}

void EditorViewController::FillContentView(views::View* content_view) {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  content_view->SetLayoutManager(std::move(layout));
  // No insets. Child views below are responsible for their padding.

  // An editor can optionally have a header view specific to it.
  std::unique_ptr<views::View> header_view = CreateHeaderView();
  if (header_view.get())
    content_view->AddChildView(header_view.release());

  // The heart of the editor dialog: all the input fields with their labels.
  content_view->AddChildView(CreateEditorView().release());
}

void EditorViewController::UpdateEditorView() {
  UpdateContentView();
  UpdateFocus(GetFirstFocusedView());
  dialog()->EditorViewUpdated();
}

void EditorViewController::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(EditorViewControllerTags::SAVE_BUTTON):
      if (ValidateModelAndSave()) {
        switch (back_navigation_type_) {
          case BackNavigationType::kOneStep:
            dialog()->GoBack();
            break;
          case BackNavigationType::kPaymentSheet:
            dialog()->GoBackToPaymentSheet();
            break;
        }
      }
      break;
    default:
      PaymentRequestSheetController::ButtonPressed(sender, event);
      break;
  }
}

views::View* EditorViewController::GetFirstFocusedView() {
  if (initial_focus_field_view_)
    return initial_focus_field_view_;
  return PaymentRequestSheetController::GetFirstFocusedView();
}

std::unique_ptr<ValidatingCombobox>
EditorViewController::CreateComboboxForField(const EditorField& field,
                                             base::string16* error_message) {
  std::unique_ptr<ValidationDelegate> delegate =
      CreateValidationDelegate(field);
  ValidationDelegate* delegate_ptr = delegate.get();
  std::unique_ptr<ValidatingCombobox> combobox =
      std::make_unique<ValidatingCombobox>(GetComboboxModelForType(field.type),
                                           std::move(delegate));
  combobox->SetAccessibleName(field.label);

  base::string16 initial_value = GetInitialValueForType(field.type);
  if (!initial_value.empty())
    combobox->SelectValue(initial_value);
  if (IsEditingExistingItem()) {
    combobox->SetInvalid(
        !delegate_ptr->IsValidCombobox(combobox.get(), error_message));
  }

  // Using autofill field type as a view ID.
  combobox->SetID(GetInputFieldViewId(field.type));
  combobox->set_listener(this);
  comboboxes_.insert(std::make_pair(combobox.get(), field));
  return combobox;
}

void EditorViewController::ContentsChanged(views::Textfield* sender,
                                           const base::string16& new_contents) {
  ValidatingTextfield* sender_cast = static_cast<ValidatingTextfield*>(sender);
  sender_cast->OnContentsChanged();
  primary_button()->SetEnabled(ValidateInputFields());
}

void EditorViewController::OnPerformAction(views::Combobox* sender) {
  ValidatingCombobox* sender_cast = static_cast<ValidatingCombobox*>(sender);
  sender_cast->OnContentsChanged();
  primary_button()->SetEnabled(ValidateInputFields());
}

std::unique_ptr<views::View> EditorViewController::CreateEditorView() {
  std::unique_ptr<views::View> editor_view = std::make_unique<views::View>();
  text_fields_.clear();
  comboboxes_.clear();
  initial_focus_field_view_ = nullptr;

  // The editor view is padded horizontally.
  editor_view->SetBorder(views::CreateEmptyBorder(
      0, payments::kPaymentRequestRowHorizontalInsets, 0,
      payments::kPaymentRequestRowHorizontalInsets));

  // All views have fixed size except the Field which stretches. The fixed
  // padding at the end is computed so that Field views have a minimum of
  // 176/272dp (short/long fields) as per spec.
  // ___________________________________________________________________________
  // |Label | 16dp pad | Field (flex) | 8dp pad | Extra View | Computed Padding|
  // |______|__________|______________|_________|____________|_________________|
  constexpr int kLabelWidth = 140;
  // This is the horizontal padding between the label and the field.
  constexpr int kLabelInputFieldHorizontalPadding = 16;
  // This is the horizontal padding between the field and the extra view.
  constexpr int kFieldExtraViewHorizontalPadding = 8;
  constexpr int kShortFieldMinimumWidth = 176;
  constexpr int kLongFieldMinimumWidth = 272;

  views::GridLayout* editor_layout =
      editor_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  // Column set for short fields.
  views::ColumnSet* columns_short = editor_layout->AddColumnSet(0);
  columns_short->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::FIXED, kLabelWidth, 0);
  columns_short->AddPaddingColumn(views::GridLayout::kFixedSize,
                                  kLabelInputFieldHorizontalPadding);
  // The field view column stretches.
  columns_short->AddColumn(views::GridLayout::LEADING,
                           views::GridLayout::CENTER, 1.0,
                           views::GridLayout::USE_PREF, 0, 0);
  columns_short->AddPaddingColumn(views::GridLayout::kFixedSize,
                                  kFieldExtraViewHorizontalPadding);
  // The extra field view column is fixed size, computed from the largest
  // extra view.
  int short_extra_view_width =
      ComputeWidestExtraViewWidth(EditorField::LengthHint::HINT_SHORT);
  columns_short->AddColumn(views::GridLayout::LEADING,
                           views::GridLayout::CENTER,
                           views::GridLayout::kFixedSize,
                           views::GridLayout::FIXED, short_extra_view_width, 0);
  // The padding at the end is fixed, computed to make sure the short field
  // maintains its minimum width.
  int short_padding = kDialogMinWidth - kShortFieldMinimumWidth - kLabelWidth -
                      (2 * kPaymentRequestRowHorizontalInsets) -
                      kLabelInputFieldHorizontalPadding -
                      kFieldExtraViewHorizontalPadding - short_extra_view_width;
  columns_short->AddPaddingColumn(views::GridLayout::kFixedSize, short_padding);

  // Column set for long fields.
  views::ColumnSet* columns_long = editor_layout->AddColumnSet(1);
  columns_long->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::FIXED, kLabelWidth, 0);
  columns_long->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 kLabelInputFieldHorizontalPadding);
  // The field view column stretches.
  columns_long->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          1.0, views::GridLayout::USE_PREF, 0, 0);
  columns_long->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 kFieldExtraViewHorizontalPadding);
  // The extra field view column is fixed size, computed from the largest
  // extra view.
  int long_extra_view_width =
      ComputeWidestExtraViewWidth(EditorField::LengthHint::HINT_LONG);
  columns_long->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::FIXED, long_extra_view_width, 0);
  // The padding at the end is fixed, computed to make sure the long field
  // maintains its minimum width.
  int long_padding = kDialogMinWidth - kLongFieldMinimumWidth - kLabelWidth -
                     (2 * kPaymentRequestRowHorizontalInsets) -
                     kLabelInputFieldHorizontalPadding -
                     kFieldExtraViewHorizontalPadding - long_extra_view_width;
  columns_long->AddPaddingColumn(views::GridLayout::kFixedSize, long_padding);

  // This column set is used for the error label in CreateInputField().
  views::ColumnSet* columns_error = editor_layout->AddColumnSet(2);
  columns_error->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::FIXED, kLabelWidth, 0);
  columns_error->AddPaddingColumn(views::GridLayout::kFixedSize,
                                  kLabelInputFieldHorizontalPadding);
  columns_error->AddColumn(views::GridLayout::LEADING,
                           views::GridLayout::CENTER, 1.0,
                           views::GridLayout::USE_PREF, 0, 0);

  views::View* first_field = nullptr;
  for (const auto& field : GetFieldDefinitions()) {
    bool valid = false;
    views::View* focusable_field =
        CreateInputField(editor_layout, field, &valid);
    if (!first_field)
      first_field = focusable_field;
    if (!initial_focus_field_view_ && !valid)
      initial_focus_field_view_ = focusable_field;
  }

  if (!initial_focus_field_view_)
    initial_focus_field_view_ = first_field;

  // Validate all fields and disable the primary (Done) button if necessary.
  primary_button()->SetEnabled(ValidateInputFields());

  views::ColumnSet* required_field_columns = editor_layout->AddColumnSet(3);
  required_field_columns->AddColumn(views::GridLayout::LEADING,
                                    views::GridLayout::CENTER, 1.0,
                                    views::GridLayout::USE_PREF, 0, 0);
  editor_layout->StartRow(views::GridLayout::kFixedSize, 3);

  // Adds the "* indicates a required field" label in "hint" grey text.
  editor_layout->AddView(CreateHintLabel(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_REQUIRED_FIELD_MESSAGE)));

  return editor_view;
}

// Each input field is a 4-quadrant grid.
// +----------------------------------------------------------+
// | Field Label           | Input field (textfield/combobox) |
// |_______________________|__________________________________|
// |   (empty)             | Error label                      |
// +----------------------------------------------------------+
views::View* EditorViewController::CreateInputField(views::GridLayout* layout,
                                                    const EditorField& field,
                                                    bool* valid) {
  int column_set =
      field.length_hint == EditorField::LengthHint::HINT_SHORT ? 0 : 1;

  // This is the top padding for every row.
  constexpr int kInputRowSpacing = 6;
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, column_set,
                              views::GridLayout::kFixedSize, kInputRowSpacing);

  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      field.required ? field.label + base::ASCIIToUTF16("*") : field.label);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(std::move(label));

  views::View* focusable_field = nullptr;
  constexpr int kInputFieldHeight = 28;

  base::string16 error_message;
  switch (field.control_type) {
    case EditorField::ControlType::TEXTFIELD:
    case EditorField::ControlType::TEXTFIELD_NUMBER: {
      std::unique_ptr<ValidationDelegate> validation_delegate =
          CreateValidationDelegate(field);
      ValidationDelegate* delegate_ptr = validation_delegate.get();

      base::string16 initial_value = GetInitialValueForType(field.type);
      auto text_field =
          std::make_unique<ValidatingTextfield>(std::move(validation_delegate));
      // Set the initial value and validity state.
      text_field->SetText(initial_value);
      text_field->SetAccessibleName(field.label);
      *valid = IsEditingExistingItem() &&
               delegate_ptr->IsValidTextfield(text_field.get(), &error_message);
      if (IsEditingExistingItem())
        text_field->SetInvalid(!(*valid));

      if (field.control_type == EditorField::ControlType::TEXTFIELD_NUMBER)
        text_field->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
      text_field->set_controller(this);
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
      std::unique_ptr<ValidatingCombobox> combobox =
          CreateComboboxForField(field, &error_message);

      *valid = combobox->IsValid();

      // |combobox| will now be owned by |row|.
      focusable_field =
          layout->AddView(std::move(combobox), 1.0, 1.0,
                          views::GridLayout::FILL, views::GridLayout::FILL,
                          views::GridLayout::kFixedSize, kInputFieldHeight);
      break;
    }
    case EditorField::ControlType::CUSTOMFIELD: {
      // Custom field view will now be owned by |row|. And it must be valid
      // since the derived class specified a custom view for this field.
      std::unique_ptr<views::View> field_view = CreateCustomFieldView(
          field.type, &focusable_field, valid, &error_message);
      DCHECK(field_view);

      layout->AddView(std::move(field_view), 1, 1, views::GridLayout::FILL,
                      views::GridLayout::FILL, views::GridLayout::kFixedSize,
                      kInputFieldHeight);
      break;
    }
    case EditorField::ControlType::READONLY_LABEL: {
      std::unique_ptr<views::Label> label =
          std::make_unique<views::Label>(GetInitialValueForType(field.type));
      label->SetID(GetInputFieldViewId(field.type));
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      layout->AddView(std::move(label), 1, 1, views::GridLayout::FILL,
                      views::GridLayout::FILL, 0, kInputFieldHeight);
      break;
    }
  }

  // If an extra view needs to go alongside the input field view, add it to the
  // last column.
  std::unique_ptr<views::View> extra_view = CreateExtraViewForField(field.type);
  if (extra_view)
    layout->AddView(std::move(extra_view));

  layout->StartRow(views::GridLayout::kFixedSize, 2);
  layout->SkipColumns(1);
  std::unique_ptr<views::View> error_label_view =
      std::make_unique<views::View>();
  error_label_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  error_labels_[field.type] = error_label_view.get();
  if (IsEditingExistingItem() && !error_message.empty())
    AddOrUpdateErrorMessageForField(field.type, error_message);

  layout->AddView(std::move(error_label_view));

  // Bottom padding for the row.
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kInputRowSpacing);
  return focusable_field;
}

int EditorViewController::ComputeWidestExtraViewWidth(
    EditorField::LengthHint size) {
  int widest_column_width = 0;

  for (const auto& field : GetFieldDefinitions()) {
    if (field.length_hint != size)
      continue;

    std::unique_ptr<views::View> extra_view =
        CreateExtraViewForField(field.type);
    if (!extra_view)
      continue;
    widest_column_width =
        std::max(extra_view->GetPreferredSize().width(), widest_column_width);
  }
  return widest_column_width;
}

void EditorViewController::AddOrUpdateErrorMessageForField(
    autofill::ServerFieldType type,
    const base::string16& error_message) {
  const auto& label_view_it = error_labels_.find(type);
  DCHECK(label_view_it != error_labels_.end());

  if (error_message.empty()) {
    label_view_it->second->RemoveAllChildViews(/*delete_children=*/true);
  } else {
    if (label_view_it->second->children().empty()) {
      // If there was no error label view, add it.
      label_view_it->second->AddChildView(
          CreateErrorLabelView(error_message, type).release());
    } else {
      // The error view is the only child, and has a Label as only child itself.
      static_cast<views::Label*>(
          label_view_it->second->children().front()->children().front())
          ->SetText(error_message);
    }
  }
}

}  // namespace payments
