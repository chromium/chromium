// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/editor_view_controller.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"

namespace payments {
namespace {

constexpr int kErrorLabelTopPadding = 6;

std::unique_ptr<views::Label> CreateErrorLabel(const std::u16string& error,
                                               autofill::FieldType type) {
  return views::Builder<views::Label>()
      .SetText(error)
      .SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL)
      .SetID(static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET) + type)
      .SetMultiLine(true)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .SetBorder(views::CreateEmptyBorder(
          gfx::Insets::TLBR(kErrorLabelTopPadding, 0, 0, 0)))
      .SetEnabledColorId(ui::kColorAlertHighSeverity)
      .Build();
}

}  // namespace

EditorViewController::EditorViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog,
    BackNavigationType back_navigation_type,
    bool is_incognito)
    : PaymentRequestSheetController(spec, state, dialog),
      back_navigation_type_(back_navigation_type),
      is_incognito_(is_incognito) {}

EditorViewController::~EditorViewController() {
  ClearViewPointers();
}

void EditorViewController::DisplayErrorMessageForField(
    autofill::FieldType type,
    const std::u16string& error_message) {
  AddOrUpdateErrorMessageForField(type, error_message);
  RelayoutPane();
}

// static
int EditorViewController::GetInputFieldViewId(autofill::FieldType type) {
  return static_cast<int>(DialogViewID::INPUT_FIELD_TYPE_OFFSET) +
         static_cast<int>(type);
}

std::unique_ptr<views::View> EditorViewController::CreateHeaderView() {
  return nullptr;
}

std::unique_ptr<views::View> EditorViewController::CreateCustomFieldView(
    autofill::FieldType type,
    views::View** focusable_field,
    bool* valid,
    std::u16string* error_message) {
  return nullptr;
}

std::unique_ptr<views::View> EditorViewController::CreateExtraViewForField(
    autofill::FieldType type) {
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

void EditorViewController::Stop() {
  PaymentRequestSheetController::Stop();
  ClearViewPointers();
}

std::u16string EditorViewController::GetPrimaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_DONE);
}

PaymentRequestSheetController::ButtonCallback
EditorViewController::GetPrimaryButtonCallback() {
  return base::BindRepeating(&EditorViewController::SaveButtonPressed,
                             base::Unretained(this));
}

int EditorViewController::GetPrimaryButtonId() {
  return static_cast<int>(DialogViewID::EDITOR_SAVE_BUTTON);
}

bool EditorViewController::GetPrimaryButtonEnabled() {
  return true;
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
  if (std::unique_ptr<views::View> header_view = CreateHeaderView()) {
    content_view->AddChildView(std::move(header_view));
  }

  // The heart of the editor dialog: all the input fields with their labels.
  content_view->AddChildView(CreateEditorView());
}

bool EditorViewController::ShouldAccelerateEnterKey() {
  // We allow the user to confirm their details by pressing 'enter' irregardless
  // of which edit field is currently focused, for quicker navigation through
  // the form.
  return true;
}

void EditorViewController::UpdateEditorView() {
  // `UpdateContentView` removes all children, so reset pointers to them.
  ClearViewPointers();
  UpdateContentView();
  UpdateFocus(GetFirstFocusedView());
  dialog()->EditorViewUpdated();
}

views::View* EditorViewController::GetFirstFocusedView() {
  if (initial_focus_field_view_)
    return initial_focus_field_view_;
  return PaymentRequestSheetController::GetFirstFocusedView();
}

std::unique_ptr<ValidatingCombobox>
EditorViewController::CreateComboboxForField(const EditorField& field,
                                             std::u16string* error_message) {
  std::unique_ptr<ValidationDelegate> delegate =
      CreateValidationDelegate(field);
  ValidationDelegate* delegate_ptr = delegate.get();
  std::unique_ptr<ValidatingCombobox> combobox =
      std::make_unique<ValidatingCombobox>(GetComboboxModelForType(field.type),
                                           std::move(delegate));
  combobox->GetViewAccessibility().SetName(field.label);

  std::u16string initial_value = GetInitialValueForType(field.type);
  if (!initial_value.empty())
    combobox->SelectValue(initial_value);
  if (IsEditingExistingItem()) {
    combobox->SetInvalid(
        !delegate_ptr->IsValidCombobox(combobox.get(), error_message));
  }

  // Using autofill field type as a view ID.
  combobox->SetID(GetInputFieldViewId(field.type));
  combobox->SetCallback(
      base::BindRepeating(&EditorViewController::OnPerformAction,
                          base::Unretained(this), combobox.get()));
  comboboxes_.insert(std::make_pair(combobox.get(), field));
  return combobox;
}

void EditorViewController::ContentsChanged(views::Textfield* sender,
                                           const std::u16string& new_contents) {
  ValidatingTextfield* sender_cast = static_cast<ValidatingTextfield*>(sender);
  sender_cast->OnContentsChanged();
  primary_button()->SetEnabled(ValidateInputFields());
}

void EditorViewController::OnPerformAction(ValidatingCombobox* sender) {
  static_cast<ValidatingCombobox*>(sender)->OnContentsChanged();
  primary_button()->SetEnabled(ValidateInputFields());
}

std::unique_ptr<views::View> EditorViewController::CreateEditorView() {
  std::unique_ptr<views::View> editor_view = std::make_unique<views::View>();

  // All views have fixed size except the Field which stretches. The fixed
  // padding at the end is computed so that Field views have a minimum of
  // 176/272dp (short/long fields) as per spec.
  // ___________________________________________________________________________
  // |Label | 16dp pad | Field (flex) | 8dp pad | Extra View | Computed Padding|
  // |______|__________|______________|_________|____________|_________________|
  constexpr int kInputRowSpacing = 12;

  editor_view->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kInputRowSpacing, payments::kPaymentRequestRowHorizontalInsets, 0,
      payments::kPaymentRequestRowHorizontalInsets)));

  editor_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kInputRowSpacing));

  views::View* first_field = nullptr;
  for (const auto& field : GetFieldDefinitions()) {
    bool valid = false;
    views::View* focusable_field =
        CreateInputField(editor_view.get(), field, &valid);
    if (!first_field)
      first_field = focusable_field;
    if (!initial_focus_field_view_ && !valid)
      initial_focus_field_view_ = focusable_field;
  }

  if (!initial_focus_field_view_)
    initial_focus_field_view_ = first_field;

  // Validate all fields and disable the primary (Done) button if necessary.
  primary_button()->SetEnabled(ValidateInputFields());

  // Adds the "* indicates a required field" label in "hint" grey text.
  editor_view->AddChildView(CreateHintLabel(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_REQUIRED_FIELD_MESSAGE),
      gfx::HorizontalAlignment::ALIGN_LEFT));

  return editor_view;
}

// Each input field is a 4-quadrant grid.
// +----------------------------------------------------------+
// | Field Label           | Input field (textfield/combobox) |
// |_______________________|__________________________________|
// |   (empty)             | Error label                      |
// +----------------------------------------------------------+
views::View* EditorViewController::CreateInputField(views::View* editor_view,
                                                    const EditorField& field,
                                                    bool* valid) {
  constexpr int kShortFieldMinimumWidth = 176;
  constexpr int kLabelWidth = 140;
  constexpr int kLongFieldMinimumWidth = 272;
  // This is the horizontal padding between the label and the field.
  const int label_input_field_horizontal_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  // The horizontal padding between the field and the extra view.
  constexpr int kFieldExtraViewHorizontalPadding = 8;

  auto* field_view =
      editor_view->AddChildView(std::make_unique<views::TableLayoutView>());

  // Label.
  field_view->AddColumn(views::LayoutAlignment::kStart,
                        views::LayoutAlignment::kCenter,
                        views::TableLayout::kFixedSize,
                        views::TableLayout::ColumnSize::kFixed, kLabelWidth, 0);
  field_view->AddPaddingColumn(views::TableLayout::kFixedSize,
                               label_input_field_horizontal_padding);
  // Field.
  field_view->AddColumn(views::LayoutAlignment::kStretch,
                        views::LayoutAlignment::kStretch, 1.0,
                        views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  field_view->AddPaddingColumn(views::TableLayout::kFixedSize,
                               kFieldExtraViewHorizontalPadding);

  int extra_view_width, padding, field_width;
  if (field.length_hint == EditorField::LengthHint::HINT_SHORT) {
    field_width = kShortFieldMinimumWidth;
    extra_view_width =
        ComputeWidestExtraViewWidth(EditorField::LengthHint::HINT_SHORT);
    // The padding at the end is fixed, computed to make sure the short field
    // maintains its minimum width.
  } else {
    field_width = kLongFieldMinimumWidth;
    extra_view_width =
        ComputeWidestExtraViewWidth(EditorField::LengthHint::HINT_LONG);
  }
  // The padding at the end is fixed, computed to make sure the long field
  // maintains its minimum width.
  padding = kDialogMinWidth - field_width - kLabelWidth -
            (2 * kPaymentRequestRowHorizontalInsets) -
            label_input_field_horizontal_padding -
            kFieldExtraViewHorizontalPadding - extra_view_width;

  // Extra view.
  field_view->AddColumn(
      views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
      views::TableLayout::kFixedSize, views::TableLayout::ColumnSize::kFixed,
      extra_view_width, 0);

  field_view->AddPaddingColumn(views::TableLayout::kFixedSize, padding);

  field_view->AddRows(1, views::TableLayout::kFixedSize);

  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      field.required ? field.label + u"*" : field.label);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  field_view->AddChildView(std::move(label));

  views::View* focusable_field = nullptr;
  constexpr int kInputFieldHeight = 28;

  std::u16string error_message;
  switch (field.control_type) {
    case EditorField::ControlType::TEXTFIELD:
    case EditorField::ControlType::TEXTFIELD_NUMBER: {
      std::unique_ptr<ValidationDelegate> validation_delegate =
          CreateValidationDelegate(field);
      ValidationDelegate* delegate_ptr = validation_delegate.get();

      std::u16string initial_value = GetInitialValueForType(field.type);
      auto text_field =
          std::make_unique<ValidatingTextfield>(std::move(validation_delegate));
      // Set the initial value and validity state.
      text_field->SetText(initial_value);
      text_field->GetViewAccessibility().SetName(field.label);
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

      focusable_field = field_view->AddChildView(std::move(text_field));
      focusable_field->SetPreferredSize(
          gfx::Size(field_width, kInputFieldHeight));
      break;
    }
    case EditorField::ControlType::COMBOBOX: {
      std::unique_ptr<ValidatingCombobox> combobox =
          CreateComboboxForField(field, &error_message);

      *valid = combobox->IsValid();

      focusable_field = field_view->AddChildView(std::move(combobox));
      focusable_field->SetPreferredSize(
          gfx::Size(field_width, kInputFieldHeight));
      break;
    }
    case EditorField::ControlType::CUSTOMFIELD: {
      std::unique_ptr<views::View> custom_view = CreateCustomFieldView(
          field.type, &focusable_field, valid, &error_message);
      DCHECK(custom_view);

      field_view->AddChildView(std::move(custom_view));
      field_view->SetPreferredSize(gfx::Size(field_width, kInputFieldHeight));
      break;
    }
    case EditorField::ControlType::READONLY_LABEL: {
      std::unique_ptr<views::Label> readonly_label =
          std::make_unique<views::Label>(GetInitialValueForType(field.type));
      readonly_label->SetID(GetInputFieldViewId(field.type));
      readonly_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      field_view->AddChildView(std::move(readonly_label));
      field_view->SetPreferredSize(gfx::Size(field_width, kInputFieldHeight));
      break;
    }
  }

  // If an extra view needs to go alongside the input field view, add it to the
  // last column.
  std::unique_ptr<views::View> extra_view = CreateExtraViewForField(field.type);
  field_view->AddChildView(extra_view ? std::move(extra_view)
                                      : std::make_unique<views::View>());

  // Error view.
  field_view->AddRows(1, views::TableLayout::kFixedSize);
  // Skip the first label column.
  field_view->AddChildView(std::make_unique<views::View>());
  auto error_label_view = std::make_unique<views::View>();
  error_label_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  error_labels_[field.type] = error_label_view.get();
  if (IsEditingExistingItem() && !error_message.empty())
    AddOrUpdateErrorMessageForField(field.type, error_message);

  field_view->AddChildView(std::move(error_label_view));

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
    autofill::FieldType type,
    const std::u16string& error_message) {
  const auto& label_view_it = error_labels_.find(type);
  CHECK(label_view_it != error_labels_.end(), base::NotFatalUntil::M130);

  if (error_message.empty()) {
    label_view_it->second->RemoveAllChildViews();
  } else {
    if (label_view_it->second->children().empty()) {
      // If there was no error label view, add it.
      label_view_it->second->AddChildView(
          CreateErrorLabel(error_message, type));
    } else {
      // The error view is the only child, and has a Label as only child itself.
      static_cast<views::Label*>(label_view_it->second->children().front())
          ->SetText(error_message);
    }
  }
}

void EditorViewController::SaveButtonPressed(const ui::Event& event) {
  if (!ValidateModelAndSave())
    return;
  if (back_navigation_type_ == BackNavigationType::kOneStep) {
    dialog()->GoBack();
  } else {
    DCHECK_EQ(BackNavigationType::kPaymentSheet, back_navigation_type_);
    dialog()->GoBackToPaymentSheet();
  }
}

void EditorViewController::ClearViewPointers() {
  for (auto [textfield, _] : text_fields_) {
    textfield->set_controller(nullptr);
  }
  text_fields_.clear();
  comboboxes_.clear();
  initial_focus_field_view_ = nullptr;
}

}  // namespace payments
