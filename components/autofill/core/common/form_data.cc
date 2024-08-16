// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data.h"

#include <stddef.h>
#include <string_view>
#include <tuple>

#include "base/base64.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

namespace {

const int kFormDataPickleVersion = 8;

bool ReadGURL(base::PickleIterator* iter, GURL* url) {
  std::string spec;
  if (!iter->ReadString(&spec))
    return false;

  *url = GURL(spec);
  return true;
}

bool ReadOrigin(base::PickleIterator* iter, url::Origin* origin) {
  std::string spec;
  if (!iter->ReadString(&spec))
    return false;

  *origin = url::Origin::Create(GURL(spec));
  return true;
}

void SerializeFormFieldDataVector(const std::vector<FormFieldData>& fields,
                                  base::Pickle* pickle) {
  pickle->WriteInt(static_cast<int>(fields.size()));
  for (const FormFieldData& field : fields) {
    SerializeFormFieldData(field, pickle);
  }
}

bool DeserializeFormFieldDataVector(base::PickleIterator* iter,
                                    std::vector<FormFieldData>* fields) {
  int size;
  if (!iter->ReadInt(&size))
    return false;

  FormFieldData temp;
  for (int i = 0; i < size; ++i) {
    if (!DeserializeFormFieldData(iter, &temp))
      return false;

    fields->push_back(temp);
  }
  return true;
}

void LogDeserializationError(int version) {
  DVLOG(1) << "Could not deserialize version " << version
           << " FormData from pickle.";
}

}  // namespace

FrameTokenWithPredecessor::FrameTokenWithPredecessor() = default;
FrameTokenWithPredecessor::FrameTokenWithPredecessor(
    const FrameTokenWithPredecessor&) = default;
FrameTokenWithPredecessor::FrameTokenWithPredecessor(
    FrameTokenWithPredecessor&&) = default;
FrameTokenWithPredecessor& FrameTokenWithPredecessor::operator=(
    const FrameTokenWithPredecessor&) = default;
FrameTokenWithPredecessor& FrameTokenWithPredecessor::operator=(
    FrameTokenWithPredecessor&&) = default;
FrameTokenWithPredecessor::~FrameTokenWithPredecessor() = default;

FormData::FormData() = default;

FormData::FormData(const FormData&) = default;

FormData& FormData::operator=(const FormData&) = default;

FormData::FormData(FormData&&) = default;

FormData& FormData::operator=(FormData&&) = default;

FormData::~FormData() = default;

bool FormData::SameFormAs(const FormData& form) const {
  if (name() != form.name() || id_attribute() != form.id_attribute() ||
      name_attribute() != form.name_attribute() || url() != form.url() ||
      action() != form.action() ||
      likely_contains_captcha() != form.likely_contains_captcha() ||
      renderer_id().is_null() != form.renderer_id().is_null() ||
      fields_.size() != form.fields_.size()) {
    return false;
  }
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (!fields_[i].SameFieldAs(form.fields_[i])) {
      return false;
    }
  }
  return true;
}

// static
bool FormData::DeepEqual(const FormData& a, const FormData& b) {
  // We compare all unique identifiers first, including the field renderer IDs,
  // because we expect most inequalities to be due to them.
  if (a.renderer_id() != b.renderer_id() ||
      a.child_frames() != b.child_frames() ||
      !base::ranges::equal(a.fields(), b.fields(), {},
                           &FormFieldData::renderer_id,
                           &FormFieldData::renderer_id)) {
    return false;
  }

  if (a.name() != b.name() || a.id_attribute() != b.id_attribute() ||
      a.name_attribute() != b.name_attribute() || a.url() != b.url() ||
      a.action() != b.action() ||
      a.likely_contains_captcha() != b.likely_contains_captcha() ||
      !base::ranges::equal(a.fields(), b.fields(), &FormFieldData::DeepEqual)) {
    return false;
  }
  return true;
}

bool FormHasNonEmptyPasswordField(const FormData& form) {
  for (const auto& field : form.fields()) {
    if (field.IsPasswordInputElement()) {
      if (!field.value().empty() || !field.user_input().empty()) {
        return true;
      }
    }
  }
  return false;
}

std::ostream& operator<<(std::ostream& os, const FormData& form) {
  os << base::UTF16ToUTF8(form.name()) << " " << form.url() << " "
     << form.action() << " " << form.main_frame_origin() << " " << "Fields:";
  for (const FormFieldData& field : form.fields()) {
    os << field << ",";
  }
  return os;
}

const FormFieldData* FormData::FindFieldByGlobalId(
    const FieldGlobalId& global_id) const {
  auto fields_it =
      base::ranges::find(fields(), global_id, &FormFieldData::global_id);

  // If the field is found, return a pointer to the field, otherwise return
  // nullptr.
  return fields_it != fields().end() ? &*fields_it : nullptr;
}

FormFieldData* FormData::FindFieldByNameForTest(
    std::u16string_view name_or_id) {
  auto fields_it =
      base::ranges::find(fields_, name_or_id, &FormFieldData::name);

  // If the field is found, return a pointer to the field, otherwise return
  // nullptr.
  return fields_it != fields_.end() ? &*fields_it : nullptr;
}

void SerializeFormData(const FormData& form_data, base::Pickle* pickle) {
  pickle->WriteInt(kFormDataPickleVersion);
  pickle->WriteString16(form_data.name());
  pickle->WriteString(form_data.url().spec());
  pickle->WriteString(form_data.action().spec());
  SerializeFormFieldDataVector(form_data.fields(), pickle);
  pickle->WriteString(form_data.main_frame_origin().Serialize());
}

bool DeserializeFormData(base::PickleIterator* iter, FormData* form_data) {
  int version;
  FormData temp_form_data;
  if (!iter->ReadInt(&version)) {
    DVLOG(1) << "Bad pickle of FormData, no version present";
    return false;
  }

  if (version < 1 || version > kFormDataPickleVersion) {
    DVLOG(1) << "Unknown FormData pickle version " << version;
    return false;
  }

  {
    std::u16string name;
    if (!iter->ReadString16(&name)) {
      LogDeserializationError(version);
      return false;
    }
    temp_form_data.set_name(std::move(name));
  }

  if (version == 1) {
    std::u16string method;
    if (!iter->ReadString16(&method)) {
      LogDeserializationError(version);
      return false;
    }
  }

  bool unused_user_submitted;
  {
    GURL url;
    GURL action;
    std::vector<FormFieldData> fields;
    if (!ReadGURL(iter, &url) || !ReadGURL(iter, &action) ||
        // user_submitted was removed/no longer serialized in version 4.
        (version < 4 && !iter->ReadBool(&unused_user_submitted)) ||
        !DeserializeFormFieldDataVector(iter, &fields)) {
      LogDeserializationError(version);
      return false;
    }
    temp_form_data.set_url(std::move(url));
    temp_form_data.set_action(std::move(action));
    temp_form_data.set_fields(std::move(fields));
  }

  if (version >= 3 && version <= 7) {
    bool temp_bool = false;
    if (!iter->ReadBool(&temp_bool)) {
      LogDeserializationError(version);
      return false;
    }
  }

  if (version >= 5 && version <= 6) {
    bool is_formless_checkout;
    if (!iter->ReadBool(&is_formless_checkout)) {
      LogDeserializationError(version);
      return false;
    }
  }

  if (version >= 6) {
    url::Origin main_frame_origin;
    if (!ReadOrigin(iter, &main_frame_origin)) {
      LogDeserializationError(version);
      return false;
    }
    temp_form_data.set_main_frame_origin(std::move(main_frame_origin));
  }

  *form_data = temp_form_data;
  return true;
}

LogBuffer& operator<<(LogBuffer& buffer, const FormData& form) {
  buffer << Tag{"div"} << Attrib{"class", "form"};
  buffer << Tag{"table"};
  buffer << Tr{} << "Form name:" << form.name();
  buffer << Tr{} << "Renderer id:"
         << base::NumberToString(form.global_id().renderer_id.value());
  buffer << Tr{} << "Host frame: "
         << base::StrCat({form.global_id().frame_token.ToString(), " (",
                          url::Origin::Create(form.url()).Serialize(), ")"});
  buffer << Tr{} << "URL:" << form.url();
  buffer << Tr{} << "Action:" << form.action();
  buffer << Tr{} << "Is action empty:" << form.is_action_empty();
  for (size_t i = 0; i < form.fields().size(); ++i) {
    buffer << Tag{"tr"};
    buffer << Tag{"td"} << "Field " << i << ": " << CTag{};
    buffer << Tag{"td"};
    buffer << Tag{"table"} << form.fields().at(i) << CTag{"table"};
    buffer << CTag{"td"};
    buffer << CTag{"tr"};
  }
  buffer << CTag{"table"};
  buffer << CTag{"div"};
  return buffer;
}

}  // namespace autofill
