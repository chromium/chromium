// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data.h"

#include <algorithm>
#include <optional>
#include <tuple>

#include "base/notreached.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/logging/log_buffer.h"

// TODO(crbug.com/41422062): Clean up the (de)serialization code.

namespace autofill {

namespace {

// Increment this anytime pickle format is modified as well as provide
// deserialization routine from previous kFormFieldDataPickleVersion format.
const int kFormFieldDataPickleVersion = 9;

void WriteSelectOption(const SelectOption& option, base::Pickle* pickle) {
  pickle->WriteString16(option.value);
  pickle->WriteString16(option.text);
}

bool ReadSelectOption(base::PickleIterator* iter, SelectOption* option) {
  std::u16string value;
  std::u16string content;
  if (!iter->ReadString16(&value) || !iter->ReadString16(&content))
    return false;
  *option = {.value = value, .text = content};
  return true;
}

void WriteSelectOptionVector(const std::vector<SelectOption>& options,
                             base::Pickle* pickle) {
  pickle->WriteInt(static_cast<int>(options.size()));
  for (const SelectOption& option : options)
    WriteSelectOption(option, pickle);
}

bool ReadSelectOptionVector(base::PickleIterator* iter,
                            std::vector<SelectOption>* options) {
  int size;
  if (!iter->ReadInt(&size))
    return false;

  for (int i = 0; i < size; i++) {
    SelectOption pickle_data;
    if (!ReadSelectOption(iter, &pickle_data))
      return false;
    options->push_back(pickle_data);
  }
  return true;
}

bool ReadStringVector(base::PickleIterator* iter,
                      std::vector<std::u16string>* strings) {
  int size;
  if (!iter->ReadInt(&size))
    return false;

  std::u16string pickle_data;
  for (int i = 0; i < size; i++) {
    if (!iter->ReadString16(&pickle_data))
      return false;
    strings->push_back(pickle_data);
  }
  return true;
}

template <typename T>
bool ReadAsInt(base::PickleIterator* iter, T* target_value) {
  int pickle_data;
  if (!iter->ReadInt(&pickle_data))
    return false;

  *target_value = static_cast<T>(pickle_data);
  return true;
}

bool DeserializeSection1(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  std::string form_control_type;
  std::u16string label;
  std::u16string name;
  std::u16string value;
  std::string autocomplete_attribute;
  uint64_t max_length = 0;
  bool is_autofilled = false;
  bool success =
      iter->ReadString16(&label) && iter->ReadString16(&name) &&
      iter->ReadString16(&value) && iter->ReadString(&form_control_type) &&
      iter->ReadString(&autocomplete_attribute) &&
      iter->ReadUInt64(&max_length) && iter->ReadBool(&is_autofilled);
  if (success) {
    field_data->set_label(std::move(label));
    field_data->set_name(std::move(name));
    field_data->set_value(std::move(value));
    field_data->set_autocomplete_attribute(std::move(autocomplete_attribute));
    field_data->set_max_length(max_length);
    field_data->set_is_autofilled(std::move(is_autofilled));
    // Form control types are serialized as strings for legacy reasons.
    // TODO(crbug.com/1353392,crbug.com/1482526): Why does the Password Manager
    // (de)serialize form control types? Remove it or migrate it to the enum
    // values.
    field_data->set_form_control_type(StringToFormControlTypeDiscouraged(
        form_control_type, /*fallback=*/FormControlType::kInputText));
  }
  return success;
}

bool DeserializeSection5(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  bool is_checked = false;
  bool is_checkable = false;
  const bool success =
      iter->ReadBool(&is_checked) && iter->ReadBool(&is_checkable);

  if (success)
    SetCheckStatus(field_data, is_checkable, is_checked);

  return success;
}

bool DeserializeSection6(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  FormFieldData::CheckStatus check_status =
      FormFieldData::CheckStatus::kNotCheckable;
  if (!ReadAsInt(iter, &check_status)) {
    return false;
  }
  field_data->set_check_status(check_status);
  return true;
}

bool DeserializeSection7(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  bool is_focusable = false;
  bool should_autocomplete = false;
  if (!iter->ReadBool(&is_focusable) || !iter->ReadBool(&should_autocomplete)) {
    return false;
  }
  field_data->set_is_focusable(std::move(is_focusable));
  field_data->set_should_autocomplete(std::move(should_autocomplete));
  return true;
}

bool DeserializeSection3(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  std::vector<std::u16string> option_values;
  std::vector<std::u16string> option_texts;
  base::i18n::TextDirection text_direction = base::i18n::UNKNOWN_DIRECTION;
  if (!ReadAsInt(iter, &text_direction) ||
      !ReadStringVector(iter, &option_values) ||
      !ReadStringVector(iter, &option_texts) ||
      option_values.size() != option_texts.size()) {
    return false;
  }
  field_data->set_text_direction(text_direction);
  std::vector<SelectOption> options;
  for (size_t i = 0; i < option_values.size(); ++i) {
    options.push_back({.value = std::move(option_values[i]),
                       .text = std::move(option_texts[i])});
  }
  field_data->set_options(std::move(options));
  return true;
}

bool DeserializeSection12(base::PickleIterator* iter,
                          FormFieldData* field_data) {
  base::i18n::TextDirection text_direction;
  std::vector<SelectOption> options;
  if (!ReadAsInt(iter, &text_direction) ||
      !ReadSelectOptionVector(iter, &options)) {
    return false;
  }
  field_data->set_text_direction(std::move(text_direction));
  field_data->set_options(std::move(options));
  return true;
}

bool DeserializeSection2(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  FormFieldData::RoleAttribute role = FormFieldData::RoleAttribute::kOther;
  if (!ReadAsInt(iter, &role)) {
    return false;
  }
  field_data->set_role(role);
  return true;
}

bool DeserializeSection4(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  std::u16string placeholder;
  if (!iter->ReadString16(&placeholder)) {
    return false;
  }
  field_data->set_placeholder(std::move(placeholder));
  return true;
}

bool DeserializeSection8(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  std::u16string css_classes;
  if (!iter->ReadString16(&css_classes)) {
    return false;
  }
  field_data->set_css_classes(std::move(css_classes));
  return true;
}

bool DeserializeSection9(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  FieldPropertiesMask properties_mask;
  if (!iter->ReadUInt32(&properties_mask)) {
    return false;
  }
  field_data->set_properties_mask(std::move(properties_mask));
  return true;
}

bool DeserializeSection10(base::PickleIterator* iter,
                          FormFieldData* field_data) {
  std::u16string id_attribute;
  if (!iter->ReadString16(&id_attribute)) {
    return false;
  }
  field_data->set_id_attribute(std::move(id_attribute));
  return true;
}

bool DeserializeSection11(base::PickleIterator* iter,
                          FormFieldData* field_data) {
  std::u16string name_attribute;
  if (!iter->ReadString16(&name_attribute)) {
    return false;
  }
  field_data->set_name_attribute(std::move(name_attribute));
  return true;
}

}  // namespace

Section Section::FromAutocomplete(Section::Autocomplete autocomplete) {
  Section section;
  if (autocomplete.section.empty() && autocomplete.mode == HtmlFieldMode::kNone)
    return section;
  section.value_ = std::move(autocomplete);
  return section;
}

Section Section::FromFieldIdentifier(
    const FormFieldData& field,
    base::flat_map<LocalFrameToken, size_t>& frame_token_ids) {
  Section section;
  // Set the section's value based on the field identifiers: the field's name,
  // mapped frame id, renderer id. We do not use LocalFrameTokens but instead
  // map them to consecutive integers using `frame_token_ids`, which uniquely
  // identify a frame within a given FormStructure. Since we do not intend to
  // compare sections from different FormStructures, this is sufficient.
  //
  // We intentionally do not include the LocalFrameToken in the section
  // because frame tokens should not be sent to a renderer.
  //
  // TODO(crbug.com/40200532): Remove special handling of FrameTokens.
  size_t generated_frame_id =
      frame_token_ids.emplace(field.host_frame(), frame_token_ids.size())
          .first->second;
  section.value_ = FieldIdentifier(base::UTF16ToUTF8(field.name()),
                                   generated_frame_id, field.renderer_id());
  return section;
}

Section::Section() = default;

Section::Section(const Section& section) = default;

Section::~Section() = default;

Section::operator bool() const {
  return !is_default();
}

bool Section::is_from_autocomplete() const {
  return absl::holds_alternative<Autocomplete>(value_);
}

bool Section::is_from_fieldidentifier() const {
  return absl::holds_alternative<FieldIdentifier>(value_);
}

bool Section::is_default() const {
  return absl::holds_alternative<Default>(value_);
}

std::string Section::ToString() const {
  static constexpr char kDefaultSection[] = "-default";

  std::string section_name;
  if (const Autocomplete* autocomplete = absl::get_if<Autocomplete>(&value_)) {
    // To prevent potential section name collisions, append `kDefaultSection`
    // suffix to fields without a `HtmlFieldMode`. Without this, 'autocomplete'
    // attribute values "section--shipping street-address" and "shipping
    // street-address" would have the same prefix.
    section_name = autocomplete->section +
                   (autocomplete->mode != HtmlFieldMode::kNone
                        ? "-" + HtmlFieldModeToString(autocomplete->mode)
                        : kDefaultSection);
  } else if (const FieldIdentifier* f =
                 absl::get_if<FieldIdentifier>(&value_)) {
    FieldIdentifier field_identifier = *f;
    section_name = base::StrCat(
        {field_identifier.field_name, "_",
         base::NumberToString(field_identifier.local_frame_id), "_",
         base::NumberToString(field_identifier.field_renderer_id.value())});
  }

  return section_name.empty() ? kDefaultSection : section_name;
}

LogBuffer& operator<<(LogBuffer& buffer, const Section& section) {
  return buffer << section.ToString();
}

std::ostream& operator<<(std::ostream& os, const Section& section) {
  return os << section.ToString();
}

LogBuffer& operator<<(LogBuffer& buffer, FormControlType type) {
  return buffer << FormControlTypeToString(type);
}

FormFieldData::FormFieldData() = default;

FormFieldData::FormFieldData(const FormFieldData&) = default;

FormFieldData& FormFieldData::operator=(const FormFieldData&) = default;

FormFieldData::FormFieldData(FormFieldData&&) = default;

FormFieldData& FormFieldData::operator=(FormFieldData&&) = default;

FormFieldData::~FormFieldData() = default;

base::optional_ref<const SelectOption> FormFieldData::selected_option() const {
  for (const SelectOption& option : options()) {
    if (option.value == value()) {
      return option;
    }
  }
  return std::nullopt;
}

bool FormFieldData::SameFieldAs(const FormFieldData& field) const {
  auto equality_tuple = [](const FormFieldData& f) {
    return std::tuple_cat(
        std::tie(f.label_, f.name_, f.name_attribute_, f.id_attribute_,
                 f.form_control_type_, f.autocomplete_attribute_,
                 f.placeholder_, f.max_length_, f.css_classes_, f.is_focusable_,
                 f.should_autocomplete_, f.role_, f.text_direction_,
                 f.options_),
        std::make_tuple(IsCheckable(f.check_status_)));
  };
  return equality_tuple(*this) == equality_tuple(field);
}

bool FormFieldData::IsTextInputElement() const {
  return form_control_type() == FormControlType::kInputText ||
         form_control_type() == FormControlType::kInputPassword ||
         form_control_type() == FormControlType::kInputSearch ||
         form_control_type() == FormControlType::kInputTelephone ||
         form_control_type() == FormControlType::kInputUrl ||
         form_control_type() == FormControlType::kInputEmail ||
         form_control_type() == FormControlType::kInputNumber;
}

bool FormFieldData::IsPasswordInputElement() const {
  return form_control_type() == FormControlType::kInputPassword;
}

bool FormFieldData::IsSelectElement() const {
  return form_control_type() == FormControlType::kSelectOne;
}

bool FormFieldData::DidUserType() const {
  return properties_mask() & kUserTyped;
}

bool FormFieldData::HadFocus() const {
  return properties_mask() & kHadFocus;
}

bool FormFieldData::WasPasswordAutofilled() const {
  return properties_mask() & kAutofilled;
}

// static
bool FormFieldData::DeepEqual(const FormFieldData& a, const FormFieldData& b) {
  return a.renderer_id() == b.renderer_id() && a.SameFieldAs(b);
}

FormFieldData::FillData::FillData() = default;

FormFieldData::FillData::~FillData() = default;

FormFieldData::FillData::FillData(const FormFieldData& field)
    : value(field.value()),
      renderer_id(field.renderer_id()),
      host_form_id(field.host_form_id()),
      section(field.section()),
      is_autofilled(field.is_autofilled()),
      force_override(field.force_override()) {}

FormFieldData::FillData::FillData(const FillData&) = default;

FormFieldData::FillData& FormFieldData::FillData::operator=(const FillData&) =
    default;

std::string_view FormControlTypeToString(FormControlType type) {
  switch (type) {
    case FormControlType::kContentEditable:
      return "contenteditable";
    case FormControlType::kInputCheckbox:
      return "checkbox";
    case FormControlType::kInputEmail:
      return "email";
    case FormControlType::kInputMonth:
      return "month";
    case FormControlType::kInputNumber:
      return "number";
    case FormControlType::kInputPassword:
      return "password";
    case FormControlType::kInputRadio:
      return "radio";
    case FormControlType::kInputSearch:
      return "search";
    case FormControlType::kInputTelephone:
      return "tel";
    case FormControlType::kInputText:
      return "text";
    case FormControlType::kInputUrl:
      return "url";
    case FormControlType::kSelectOne:
      return "select-one";
    case FormControlType::kSelectMultiple:
      return "select-multiple";
    case FormControlType::kTextArea:
      return "textarea";
  }
  NOTREACHED();
}

FormControlType StringToFormControlTypeDiscouraged(
    std::string_view type_string,
    std::optional<FormControlType> fallback) {
  for (auto i = base::to_underlying(FormControlType::kMinValue);
       i <= base::to_underlying(FormControlType::kMaxValue); ++i) {
    FormControlType type = static_cast<FormControlType>(i);
    if (mojom::IsKnownEnumValue(type) &&
        type_string == autofill::FormControlTypeToString(type)) {
      return type;
    }
  }
  if (fallback) {
    return *fallback;
  }
  NOTREACHED();
}

void SerializeFormFieldData(const FormFieldData& field_data,
                            base::Pickle* pickle) {
  pickle->WriteInt(kFormFieldDataPickleVersion);
  pickle->WriteString16(field_data.label());
  pickle->WriteString16(field_data.name());
  pickle->WriteString16(field_data.value());
  pickle->WriteString(FormControlTypeToString(field_data.form_control_type()));
  // We don't serialize the `parsed_autocomplete`. See http://crbug.com/1353392.
  pickle->WriteString(field_data.autocomplete_attribute());
  pickle->WriteUInt64(field_data.max_length());
  pickle->WriteBool(field_data.is_autofilled());
  pickle->WriteInt(static_cast<int>(field_data.check_status()));
  pickle->WriteBool(field_data.is_focusable());
  pickle->WriteBool(field_data.should_autocomplete());
  pickle->WriteInt(static_cast<int>(field_data.role()));
  pickle->WriteInt(field_data.text_direction());
  WriteSelectOptionVector(field_data.options(), pickle);
  pickle->WriteString16(field_data.placeholder());
  pickle->WriteString16(field_data.css_classes());
  pickle->WriteUInt32(field_data.properties_mask());
  pickle->WriteString16(field_data.id_attribute());
  pickle->WriteString16(field_data.name_attribute());
}

bool DeserializeFormFieldData(base::PickleIterator* iter,
                              FormFieldData* field_data) {
  int version;
  FormFieldData temp_form_field_data;
  if (!iter->ReadInt(&version)) {
    LOG(ERROR) << "Bad pickle of FormFieldData, no version present";
    return false;
  }

  switch (version) {
    case 1: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection5(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection3(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 2: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection5(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection2(iter, &temp_form_field_data) ||
          !DeserializeSection3(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 3: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection5(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection2(iter, &temp_form_field_data) ||
          !DeserializeSection3(iter, &temp_form_field_data) ||
          !DeserializeSection4(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 4: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection6(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection2(iter, &temp_form_field_data) ||
          !DeserializeSection3(iter, &temp_form_field_data) ||
          !DeserializeSection4(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 5: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection6(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection2(iter, &temp_form_field_data) ||
          !DeserializeSection3(iter, &temp_form_field_data) ||
          !DeserializeSection4(iter, &temp_form_field_data) ||
          !DeserializeSection8(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 6: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection6(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection2(iter, &temp_form_field_data) ||
          !DeserializeSection3(iter, &temp_form_field_data) ||
          !DeserializeSection4(iter, &temp_form_field_data) ||
          !DeserializeSection8(iter, &temp_form_field_data) ||
          !DeserializeSection9(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 7: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection6(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection2(iter, &temp_form_field_data) ||
          !DeserializeSection3(iter, &temp_form_field_data) ||
          !DeserializeSection4(iter, &temp_form_field_data) ||
          !DeserializeSection8(iter, &temp_form_field_data) ||
          !DeserializeSection9(iter, &temp_form_field_data) ||
          !DeserializeSection10(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 8: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection6(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection2(iter, &temp_form_field_data) ||
          !DeserializeSection3(iter, &temp_form_field_data) ||
          !DeserializeSection4(iter, &temp_form_field_data) ||
          !DeserializeSection8(iter, &temp_form_field_data) ||
          !DeserializeSection9(iter, &temp_form_field_data) ||
          !DeserializeSection10(iter, &temp_form_field_data) ||
          !DeserializeSection11(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 9: {
      if (!DeserializeSection1(iter, &temp_form_field_data) ||
          !DeserializeSection6(iter, &temp_form_field_data) ||
          !DeserializeSection7(iter, &temp_form_field_data) ||
          !DeserializeSection2(iter, &temp_form_field_data) ||
          !DeserializeSection12(iter, &temp_form_field_data) ||
          !DeserializeSection4(iter, &temp_form_field_data) ||
          !DeserializeSection8(iter, &temp_form_field_data) ||
          !DeserializeSection9(iter, &temp_form_field_data) ||
          !DeserializeSection10(iter, &temp_form_field_data) ||
          !DeserializeSection11(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    default: {
      LOG(ERROR) << "Unknown FormFieldData pickle version " << version;
      return false;
    }
  }
  *field_data = temp_form_field_data;
  return true;
}

std::ostream& operator<<(std::ostream& os, const FormFieldData& field) {
  return os << "label='" << field.label() << "' "
            << "unique_Id=" << field.global_id() << " " << "origin='"
            << field.origin().Serialize() << "' " << "name='" << field.name()
            << "' " << "id_attribute='" << field.id_attribute() << "' "
            << "name_attribute='" << field.name_attribute() << "' " << "value='"
            << field.value() << "' " << "control='" << field.form_control_type()
            << "' " << "autocomplete='" << field.autocomplete_attribute()
            << "' " << "parsed_autocomplete='"
            << (field.parsed_autocomplete()
                    ? field.parsed_autocomplete()->ToString()
                    : "")
            << "' " << "placeholder='" << field.placeholder() << "' "
            << "max_length=" << field.max_length() << " " << "css_classes='"
            << field.css_classes() << "' "
            << "autofilled=" << field.is_autofilled() << " "
            << "check_status=" << field.check_status() << " "
            << "is_focusable=" << field.is_focusable() << " "
            << "should_autocomplete=" << field.should_autocomplete() << " "
            << "role=" << field.role() << " "
            << "text_direction=" << field.text_direction() << " "
            << "is_enabled=" << field.is_enabled() << " "
            << "is_readonly=" << field.is_readonly() << " "
            << "user_input=" << field.user_input() << " "
            << "properties_mask=" << field.properties_mask() << " "
            << "label_source=" << field.label_source() << " "
            << "bounds=" << field.bounds().ToString();
}

LogBuffer& operator<<(LogBuffer& buffer, const FormFieldData& field) {
  buffer << Tag{"table"};
  buffer << Tr{} << "Name:" << field.name();
  buffer << Tr{} << "Identifiers:"
         << base::StrCat({"renderer id: ",
                          base::NumberToString(field.renderer_id().value()),
                          ", host frame: ",
                          field.renderer_form_id().frame_token.ToString(), " (",
                          field.origin().Serialize(),
                          "), host form renderer id: ",
                          base::NumberToString(field.host_form_id().value())});
  buffer << Tr{} << "Origin:" << field.origin().Serialize();
  buffer << Tr{} << "Name attribute:" << field.name_attribute();
  buffer << Tr{} << "Id attribute:" << field.id_attribute();
  constexpr size_t kMaxLabelSize = 100;
  const std::u16string truncated_label =
      field.label().substr(0, std::min(field.label().length(), kMaxLabelSize));
  buffer << Tr{} << "Label:" << truncated_label;
  buffer << Tr{} << "Form control type:" << field.form_control_type();
  buffer << Tr{} << "Autocomplete attribute:" << field.autocomplete_attribute();
  buffer << Tr{} << "Parsed autocomplete attribute:"
         << (field.parsed_autocomplete()
                 ? field.parsed_autocomplete()->ToString()
                 : "");
  buffer << Tr{} << "Aria label:" << field.aria_label();
  buffer << Tr{} << "Aria description:" << field.aria_description();
  buffer << Tr{} << "Section:" << field.section();
  buffer << Tr{} << "Is focusable:" << field.is_focusable();
  buffer << Tr{} << "Is enabled:" << field.is_enabled();
  buffer << Tr{} << "Is readonly:" << field.is_readonly();
  buffer << Tr{} << "Is empty:" << (field.value().empty() ? "Yes" : "No");
  buffer << Tr{} << "Value:" << field.value() << SetParentTagContainsPII{};
  buffer << CTag{"table"};
  return buffer;
}

}  // namespace autofill
