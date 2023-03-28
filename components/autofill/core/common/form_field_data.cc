// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data.h"

#include <algorithm>
#include <tuple>

#include "base/notreached.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/logging/log_buffer.h"

// TODO(crbug/897756): Clean up the (de)serialization code.

namespace autofill {

namespace {

// Increment this anytime pickle format is modified as well as provide
// deserialization routine from previous kFormFieldDataPickleVersion format.
const int kFormFieldDataPickleVersion = 9;

void WriteSelectOption(const SelectOption& option, base::Pickle* pickle) {
  pickle->WriteString16(option.value);
  pickle->WriteString16(option.content);
}

bool ReadSelectOption(base::PickleIterator* iter, SelectOption* option) {
  std::u16string value;
  std::u16string content;
  if (!iter->ReadString16(&value) || !iter->ReadString16(&content))
    return false;
  *option = {.value = value, .content = content};
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
  return iter->ReadString16(&field_data->label) &&
         iter->ReadString16(&field_data->name) &&
         iter->ReadString16(&field_data->value) &&
         iter->ReadString(&field_data->form_control_type) &&
         iter->ReadString(&field_data->autocomplete_attribute) &&
         iter->ReadUInt64(&field_data->max_length) &&
         iter->ReadBool(&field_data->is_autofilled);
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
  return ReadAsInt(iter, &field_data->check_status);
}

bool DeserializeSection7(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  return iter->ReadBool(&field_data->is_focusable) &&
         iter->ReadBool(&field_data->should_autocomplete);
}

bool DeserializeSection3(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  std::vector<std::u16string> option_values;
  std::vector<std::u16string> option_contents;
  if (!ReadAsInt(iter, &field_data->text_direction) ||
      !ReadStringVector(iter, &option_values) ||
      !ReadStringVector(iter, &option_contents) ||
      option_values.size() != option_contents.size()) {
    return false;
  }
  for (size_t i = 0; i < option_values.size(); ++i) {
    field_data->options.push_back({.value = std::move(option_values[i]),
                                   .content = std::move(option_contents[i])});
  }
  return true;
}

bool DeserializeSection12(base::PickleIterator* iter,
                          FormFieldData* field_data) {
  return ReadAsInt(iter, &field_data->text_direction) &&
         ReadSelectOptionVector(iter, &field_data->options);
}

bool DeserializeSection2(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  return ReadAsInt(iter, &field_data->role);
}

bool DeserializeSection4(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  return iter->ReadString16(&field_data->placeholder);
}

bool DeserializeSection8(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  return iter->ReadString16(&field_data->css_classes);
}

bool DeserializeSection9(base::PickleIterator* iter,
                         FormFieldData* field_data) {
  return iter->ReadUInt32(&field_data->properties_mask);
}

bool DeserializeSection10(base::PickleIterator* iter,
                          FormFieldData* field_data) {
  return iter->ReadString16(&field_data->id_attribute);
}

bool DeserializeSection11(base::PickleIterator* iter,
                          FormFieldData* field_data) {
  return iter->ReadString16(&field_data->name_attribute);
}

// LabelInfo is used to implement that "a.label == b.label" can be weakened to
// "a.label == b.label OR a certain feature is enabled and {a,b}.label_source !=
// kLabelTag and a.label_source == b.label_source".
// Beware of the StringPiece member and resulting lifetime issues. Deleted copy
// and move ctors/operators to reduce risk potential.
struct LabelInfo {
  explicit LabelInfo(const FormFieldData& f)
      : label(f.label), source(f.label_source) {}
  LabelInfo(const LabelInfo&) = delete;
  LabelInfo& operator=(const LabelInfo&) = delete;
  LabelInfo(LabelInfo&&) = default;
  LabelInfo& operator=(LabelInfo&&) = default;

  bool operator==(const LabelInfo& that) const {
    if (label == that.label)
      return true;

    // Feature |kAutofillSkipComparingInferredLabels| weakens equivalence of
    // labels: two labels are equivalent if they were inferred from the same
    // type of tag other than a LABEL tag.
    // TODO(crbug.com/1211834): The experiment seems dead; remove?
    return base::FeatureList::IsEnabled(
               features::kAutofillSkipComparingInferredLabels) &&
           source != FormFieldData::LabelSource::kLabelTag &&
           source == that.source;
  }

  bool operator<(const LabelInfo& that) const { return label < that.label; }

  base::StringPiece16 label;
  FormFieldData::LabelSource source = FormFieldData::LabelSource::kLabelTag;
};

// CommonTuple(), SimilarityTuple(), DynamicIdentityTuple(), IdentityTuple()
// return values should be used as temporaries only because they include a
// StringPiece.

auto CommonTuple(const FormFieldData& f) {
  return std::tuple_cat(
      std::make_tuple(LabelInfo(f)),
      std::tie(f.name, f.name_attribute, f.id_attribute, f.form_control_type));
}

auto SimilarityTuple(const FormFieldData& f) {
  return std::tuple_cat(CommonTuple(f),
                        std::make_tuple(IsCheckable(f.check_status)));
}

auto DynamicIdentityTuple(const FormFieldData& f) {
  return std::tuple_cat(CommonTuple(f), std::make_tuple(f.IsFocusable()));
}

auto IdentityTuple(const FormFieldData& f) {
  // |unique_renderer_id| uniquely identifies the field, if and only if it is
  // set; the other members compared below (excluding label_source) together
  // uniquely identify the field as well.
  return std::tuple_cat(
      SimilarityTuple(f),
      std::tie(f.autocomplete_attribute, f.placeholder, f.max_length,
               f.css_classes, f.is_focusable, f.should_autocomplete, f.role,
               f.text_direction, f.options));
}

}  // namespace

bool operator==(const SelectOption& lhs, const SelectOption& rhs) {
  return std::tie(lhs.value, lhs.content) == std::tie(rhs.value, rhs.content);
}

bool operator!=(const SelectOption& lhs, const SelectOption& rhs) {
  return !(lhs == rhs);
}

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
  // TODO(crbug.com/1257141): Remove special handling of FrameTokens.
  size_t generated_frame_id =
      frame_token_ids.emplace(field.host_frame, frame_token_ids.size())
          .first->second;
  section.value_ =
      FieldIdentifier(base::UTF16ToUTF8(field.name), generated_frame_id,
                      field.unique_renderer_id);
  return section;
}

Section::Section() = default;
Section::Section(const Section& section) = default;
Section::~Section() = default;

bool operator==(const Section::Autocomplete& a,
                const Section::Autocomplete& b) {
  return std::tie(a.section, a.mode) == std::tie(b.section, b.mode);
}

bool operator!=(const Section::Autocomplete& a,
                const Section::Autocomplete& b) {
  return !(a == b);
}

bool operator<(const Section::Autocomplete& a, const Section::Autocomplete& b) {
  return std::tie(a.section, a.mode) < std::tie(b.section, b.mode);
}

bool operator==(const Section::FieldIdentifier& a,
                const Section::FieldIdentifier& b) {
  return std::tie(a.field_name, a.local_frame_id, a.field_renderer_id) ==
         std::tie(b.field_name, b.local_frame_id, b.field_renderer_id);
}

bool operator!=(const Section::FieldIdentifier& a,
                const Section::FieldIdentifier& b) {
  return !(a == b);
}

bool operator<(const Section::FieldIdentifier& a,
               const Section::FieldIdentifier& b) {
  return std::tie(a.field_name, a.local_frame_id, a.field_renderer_id) <
         std::tie(b.field_name, b.local_frame_id, b.field_renderer_id);
}

bool operator==(const Section& a, const Section& b) {
  return a.value_ == b.value_;
}

bool operator!=(const Section& a, const Section& b) {
  return !(a == b);
}

bool operator<(const Section& a, const Section& b) {
  return a.value_ < b.value_;
}

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
  constexpr char kDefaultSection[] = "-default";

  std::string section_name;
  if (const Autocomplete* autocomplete = absl::get_if<Autocomplete>(&value_)) {
    // To prevent potential section name collisions, append `kDefaultSection`
    // suffix to fields without a `HtmlFieldMode`. Without this, 'autocomplete'
    // attribute values "section--shipping street-address" and "shipping
    // street-address" would have the same prefix.
    section_name =
        autocomplete->section +
        (autocomplete->mode != HtmlFieldMode::kNone
             ? "-" + std::string(HtmlFieldModeToStringPiece(autocomplete->mode))
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

FormFieldData::FormFieldData() = default;

FormFieldData::FormFieldData(const FormFieldData&) = default;

FormFieldData& FormFieldData::operator=(const FormFieldData&) = default;

FormFieldData::FormFieldData(FormFieldData&&) = default;

FormFieldData& FormFieldData::operator=(FormFieldData&&) = default;

FormFieldData::~FormFieldData() = default;

bool FormFieldData::SameFieldAs(const FormFieldData& field) const {
  return IdentityTuple(*this) == IdentityTuple(field);
}

bool FormFieldData::SimilarFieldAs(const FormFieldData& field) const {
  return SimilarityTuple(*this) == SimilarityTuple(field);
}

bool FormFieldData::DynamicallySameFieldAs(const FormFieldData& field) const {
  return DynamicIdentityTuple(*this) == DynamicIdentityTuple(field);
}

bool FormFieldData::IsTextInputElement() const {
  return form_control_type == "text" || form_control_type == "password" ||
         form_control_type == "search" || form_control_type == "tel" ||
         form_control_type == "url" || form_control_type == "email" ||
         form_control_type == "number";
}

bool FormFieldData::IsPasswordInputElement() const {
  return form_control_type == "password";
}

bool FormFieldData::IsSelectOrSelectMenuElement() const {
  return form_control_type == "select-one" || form_control_type == "selectmenu";
}

bool FormFieldData::DidUserType() const {
  return properties_mask & kUserTyped;
}

bool FormFieldData::HadFocus() const {
  return properties_mask & kHadFocus;
}

bool FormFieldData::WasPasswordAutofilled() const {
  return properties_mask & kAutofilled;
}

// static
bool FormFieldData::DeepEqual(const FormFieldData& a, const FormFieldData& b) {
  return a.unique_renderer_id == b.unique_renderer_id &&
         IdentityTuple(a) == IdentityTuple(b);
}

void SerializeFormFieldData(const FormFieldData& field_data,
                            base::Pickle* pickle) {
  pickle->WriteInt(kFormFieldDataPickleVersion);
  pickle->WriteString16(field_data.label);
  pickle->WriteString16(field_data.name);
  pickle->WriteString16(field_data.value);
  pickle->WriteString(field_data.form_control_type);
  // We don't serialize the `parsed_autocomplete`. See http://crbug.com/1353392.
  pickle->WriteString(field_data.autocomplete_attribute);
  pickle->WriteUInt64(field_data.max_length);
  pickle->WriteBool(field_data.is_autofilled);
  pickle->WriteInt(static_cast<int>(field_data.check_status));
  pickle->WriteBool(field_data.is_focusable);
  pickle->WriteBool(field_data.should_autocomplete);
  pickle->WriteInt(static_cast<int>(field_data.role));
  pickle->WriteInt(field_data.text_direction);
  WriteSelectOptionVector(field_data.options, pickle);
  pickle->WriteString16(field_data.placeholder);
  pickle->WriteString16(field_data.css_classes);
  pickle->WriteUInt32(field_data.properties_mask);
  pickle->WriteString16(field_data.id_attribute);
  pickle->WriteString16(field_data.name_attribute);
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
  return os << "label='" << field.label << "' "
            << "unique_Id=" << field.global_id() << " "
            << "origin='" << field.origin.Serialize() << "' "
            << "name='" << field.name << "' "
            << "id_attribute='" << field.id_attribute << "' "
            << "name_attribute='" << field.name_attribute << "' "
            << "value='" << field.value << "' "
            << "control='" << field.form_control_type << "' "
            << "autocomplete='" << field.autocomplete_attribute << "' "
            << "parsed_autocomplete='"
            << (field.parsed_autocomplete
                    ? field.parsed_autocomplete->ToString()
                    : "")
            << "' "
            << "placeholder='" << field.placeholder << "' "
            << "max_length=" << field.max_length << " "
            << "css_classes='" << field.css_classes << "' "
            << "autofilled=" << field.is_autofilled << " "
            << "check_status=" << field.check_status << " "
            << "is_focusable=" << field.is_focusable << " "
            << "should_autocomplete=" << field.should_autocomplete << " "
            << "role=" << field.role << " "
            << "text_direction=" << field.text_direction << " "
            << "is_enabled=" << field.is_enabled << " "
            << "is_readonly=" << field.is_readonly << " "
            << "user_input=" << field.user_input << " "
            << "properties_mask=" << field.properties_mask << " "
            << "label_source=" << field.label_source << " "
            << "bounds=" << field.bounds.ToString();
}

LogBuffer& operator<<(LogBuffer& buffer, const FormFieldData& field) {
  buffer << Tag{"table"};
  buffer << Tr{} << "Name:" << field.name;
  buffer << Tr{} << "Identifiers:"
         << base::StrCat(
                {"renderer id: ",
                 base::NumberToString(field.unique_renderer_id.value()),
                 ", host frame: ",
                 field.renderer_form_id().frame_token.ToString(), " (",
                 field.origin.Serialize(), "), host form renderer id: ",
                 base::NumberToString(field.host_form_id.value())});
  buffer << Tr{} << "Origin:" << field.origin.Serialize();
  buffer << Tr{} << "Name attribute:" << field.name_attribute;
  buffer << Tr{} << "Id attribute:" << field.id_attribute;
  constexpr size_t kMaxLabelSize = 100;
  const std::u16string truncated_label =
      field.label.substr(0, std::min(field.label.length(), kMaxLabelSize));
  buffer << Tr{} << "Label:" << truncated_label;
  buffer << Tr{} << "Form control type:" << field.form_control_type;
  buffer << Tr{} << "Autocomplete attribute:" << field.autocomplete_attribute;
  buffer << Tr{} << "Parsed autocomplete attribute:"
         << (field.parsed_autocomplete ? field.parsed_autocomplete->ToString()
                                       : "");
  buffer << Tr{} << "Aria label:" << field.aria_label;
  buffer << Tr{} << "Aria description:" << field.aria_description;
  buffer << Tr{} << "Section:" << field.section;
  buffer << Tr{} << "Is focusable:" << field.is_focusable;
  buffer << Tr{} << "Is enabled:" << field.is_enabled;
  buffer << Tr{} << "Is readonly:" << field.is_readonly;
  buffer << Tr{} << "Is empty:" << (field.value.empty() ? "Yes" : "No");
  buffer << CTag{"table"};
  return buffer;
}

}  // namespace autofill
