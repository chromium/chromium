// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data.h"

#include <algorithm>
#include <tuple>

#include "base/pickle.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/logging/log_buffer.h"

// TODO(crbug/897756): Clean up the (de)serialization code.

namespace autofill {

namespace {

// Increment this anytime pickle format is modified as well as provide
// deserialization routine from previous kFormFieldDataPickleVersion format.
const int kFormFieldDataPickleVersion = 8;

void AddVectorToPickle(std::vector<base::string16> strings,
                       base::Pickle* pickle) {
  pickle->WriteInt(static_cast<int>(strings.size()));
  for (size_t i = 0; i < strings.size(); ++i) {
    pickle->WriteString16(strings[i]);
  }
}

bool ReadStringVector(base::PickleIterator* iter,
                      std::vector<base::string16>* strings) {
  int size;
  if (!iter->ReadInt(&size))
    return false;

  base::string16 pickle_data;
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
  return ReadAsInt(iter, &field_data->text_direction) &&
         ReadStringVector(iter, &field_data->option_values) &&
         ReadStringVector(iter, &field_data->option_contents);
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
  return std::tuple_cat(CommonTuple(f), std::make_tuple(f.IsVisible()));
}

auto IdentityTuple(const FormFieldData& f) {
  // |unique_renderer_id| uniquely identifies the field, if and only if it is
  // set; the other members compared below (excluding label_source) together
  // uniquely identify the field as well.
  return std::tuple_cat(
      SimilarityTuple(f),
      std::tie(
// TODO(crbug.com/896689): On iOS the unique_id member uniquely addresses
// this field in the DOM.
#if defined(OS_IOS)
          f.unique_id,
#endif
          f.autocomplete_attribute, f.placeholder, f.max_length, f.css_classes,
          f.is_focusable, f.should_autocomplete, f.role, f.text_direction));
}

}  // namespace

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

bool FormFieldData::IdentityComparator::operator()(
    const FormFieldData& a,
    const FormFieldData& b) const {
  return IdentityTuple(a) < IdentityTuple(b);
}

bool FormFieldData::IsTextInputElement() const {
  return form_control_type == "text" || form_control_type == "password" ||
         form_control_type == "search" || form_control_type == "tel" ||
         form_control_type == "url" || form_control_type == "email";
}

bool FormFieldData::IsPasswordInputElement() const {
  return form_control_type == "password";
}

bool FormFieldData::DidUserType() const {
  return properties_mask & USER_TYPED;
}

bool FormFieldData::HadFocus() const {
  return properties_mask & HAD_FOCUS;
}

bool FormFieldData::WasAutofilled() const {
  return properties_mask & AUTOFILLED;
}

void SerializeFormFieldData(const FormFieldData& field_data,
                            base::Pickle* pickle) {
  pickle->WriteInt(kFormFieldDataPickleVersion);
  pickle->WriteString16(field_data.label);
  pickle->WriteString16(field_data.name);
  pickle->WriteString16(field_data.value);
  pickle->WriteString(field_data.form_control_type);
  pickle->WriteString(field_data.autocomplete_attribute);
  pickle->WriteUInt64(field_data.max_length);
  pickle->WriteBool(field_data.is_autofilled);
  pickle->WriteInt(static_cast<int>(field_data.check_status));
  pickle->WriteBool(field_data.is_focusable);
  pickle->WriteBool(field_data.should_autocomplete);
  pickle->WriteInt(static_cast<int>(field_data.role));
  pickle->WriteInt(field_data.text_direction);
  AddVectorToPickle(field_data.option_values, pickle);
  AddVectorToPickle(field_data.option_contents, pickle);
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
            << "name='" << field.name << "' "
            << "id_attribute='" << field.id_attribute << "' "
            << "name_attribute='" << field.name_attribute << "' "
            << "value='" << field.value << "' "
            << "control='" << field.form_control_type << "' "
            << "autocomplete='" << field.autocomplete_attribute << "' "
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
            << "typed_value=" << field.typed_value << " "
            << "properties_mask=" << field.properties_mask << " "
            << "label_source=" << field.label_source;
}

LogBuffer& operator<<(LogBuffer& buffer, const FormFieldData& field) {
  buffer << Tag{"table"};
  buffer << Tr{} << "Name:" << field.name;
  buffer << Tr{} << "Unique renderer Id:" << field.unique_renderer_id;
  buffer << Tr{} << "Name attribute:" << field.name_attribute;
  buffer << Tr{} << "Id attribute:" << field.id_attribute;
  constexpr size_t kMaxLabelSize = 100;
  const base::string16 truncated_label =
      field.label.substr(0, std::min(field.label.length(), kMaxLabelSize));
  buffer << Tr{} << "Label:" << truncated_label;
  buffer << Tr{} << "Form control type:" << field.form_control_type;
  buffer << Tr{} << "Autocomplete attribute:" << field.autocomplete_attribute;
  buffer << Tr{} << "Aria label:" << field.aria_label;
  buffer << Tr{} << "Aria description:" << field.aria_description;
  buffer << Tr{} << "Section:" << field.section;
  buffer << Tr{} << "Is focusable:" << field.is_focusable;
  buffer << Tr{} << "Is enabled:" << field.is_enabled;
  buffer << Tr{} << "Is readonly:" << field.is_readonly;
  buffer << CTag{"table"};
  return buffer;
}

}  // namespace autofill
