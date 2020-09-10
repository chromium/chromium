// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data.h"

#include <stddef.h>
#include <tuple>

#include "base/base64.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

namespace {

const int kFormDataPickleVersion = 6;

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
  for (size_t i = 0; i < fields.size(); ++i) {
    SerializeFormFieldData(fields[i], pickle);
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

FormData::FormData() = default;

FormData::FormData(const FormData&) = default;

FormData& FormData::operator=(const FormData&) = default;

FormData::FormData(FormData&&) = default;

FormData& FormData::operator=(FormData&&) = default;

FormData::~FormData() = default;

bool FormData::SameFormAs(const FormData& form) const {
  if (name != form.name || id_attribute != form.id_attribute ||
      name_attribute != form.name_attribute || url != form.url ||
      action != form.action || is_form_tag != form.is_form_tag ||
      is_formless_checkout != form.is_formless_checkout ||
      fields.size() != form.fields.size())
    return false;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (!fields[i].SameFieldAs(form.fields[i]))
      return false;
  }
  return true;
}

bool FormData::SimilarFormAs(const FormData& form) const {
  if (name != form.name || id_attribute != form.id_attribute ||
      name_attribute != form.name_attribute || url != form.url ||
      action != form.action || is_action_empty != form.is_action_empty ||
      is_form_tag != form.is_form_tag ||
      is_formless_checkout != form.is_formless_checkout ||
      fields.size() != form.fields.size()) {
    return false;
  }
  for (size_t i = 0; i < fields.size(); ++i) {
    if (!fields[i].SimilarFieldAs(form.fields[i]))
      return false;
  }
  return true;
}

bool FormData::DynamicallySameFormAs(const FormData& form) const {
  if (name != form.name || id_attribute != form.id_attribute ||
      name_attribute != form.name_attribute ||
      fields.size() != form.fields.size())
    return false;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (!fields[i].DynamicallySameFieldAs(form.fields[i]))
      return false;
  }
  return true;
}

bool FormData::IdentityComparator::operator()(const FormData& a,
                                              const FormData& b) const {
  // |unique_renderer_id| uniquely identifies the form, if and only if it is
  // set; the other members compared below together uniquely identify the form
  // as well.
  auto tie = [](const FormData& f) {
    return std::tie(f.unique_renderer_id, f.name, f.id_attribute,
                    f.name_attribute, f.url, f.action, f.is_form_tag,
                    f.is_formless_checkout);
  };
  if (tie(a) < tie(b))
    return true;
  if (tie(b) < tie(a))
    return false;
  return std::lexicographical_compare(a.fields.begin(), a.fields.end(),
                                      b.fields.begin(), b.fields.end(),
                                      FormFieldData::IdentityComparator());
}

bool FormHasNonEmptyPasswordField(const FormData& form) {
  for (const auto& field : form.fields) {
    if (field.IsPasswordInputElement()) {
      if (!field.value.empty() || !field.typed_value.empty())
        return true;
    }
  }
  return false;
}

std::ostream& operator<<(std::ostream& os, const FormData& form) {
  os << base::UTF16ToUTF8(form.name) << " " << form.url << " " << form.action
     << " " << form.main_frame_origin << " " << form.is_form_tag << " "
     << form.is_formless_checkout << " "
     << "Fields:";
  for (size_t i = 0; i < form.fields.size(); ++i) {
    os << form.fields[i] << ",";
  }
  return os;
}

void SerializeFormData(const FormData& form_data, base::Pickle* pickle) {
  pickle->WriteInt(kFormDataPickleVersion);
  pickle->WriteString16(form_data.name);
  pickle->WriteString(form_data.url.spec());
  pickle->WriteString(form_data.action.spec());
  SerializeFormFieldDataVector(form_data.fields, pickle);
  pickle->WriteBool(form_data.is_form_tag);
  pickle->WriteBool(form_data.is_formless_checkout);
  pickle->WriteString(form_data.main_frame_origin.Serialize());
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

  if (!iter->ReadString16(&temp_form_data.name)) {
    LogDeserializationError(version);
    return false;
  }

  if (version == 1) {
    base::string16 method;
    if (!iter->ReadString16(&method)) {
      LogDeserializationError(version);
      return false;
    }
  }

  bool unused_user_submitted;
  if (!ReadGURL(iter, &temp_form_data.url) ||
      !ReadGURL(iter, &temp_form_data.action) ||
      // user_submitted was removed/no longer serialized in version 4.
      (version < 4 && !iter->ReadBool(&unused_user_submitted)) ||
      !DeserializeFormFieldDataVector(iter, &temp_form_data.fields)) {
    LogDeserializationError(version);
    return false;
  }

  if (version >= 3) {
    if (!iter->ReadBool(&temp_form_data.is_form_tag)) {
      LogDeserializationError(version);
      return false;
    }
  } else {
    form_data->is_form_tag = true;
  }

  if (version >= 5) {
    if (!iter->ReadBool(&temp_form_data.is_formless_checkout)) {
      LogDeserializationError(version);
      return false;
    }
  }

  if (version >= 6) {
    if (!ReadOrigin(iter, &temp_form_data.main_frame_origin)) {
      LogDeserializationError(version);
      return false;
    }
  }

  *form_data = temp_form_data;
  return true;
}

LogBuffer& operator<<(LogBuffer& buffer, const FormData& form) {
  buffer << Tag{"div"} << Attrib{"class", "form"};
  buffer << Tag{"table"};
  buffer << Tr{} << "Form name:" << form.name;
  buffer << Tr{} << "Unique renderer Id:" << form.unique_renderer_id.value();
  buffer << Tr{} << "URL:" << form.url;
  buffer << Tr{} << "Action:" << form.action;
  buffer << Tr{} << "Is action empty:" << form.is_action_empty;
  buffer << Tr{} << "Is <form> tag:" << form.is_form_tag;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    buffer << Tag{"tr"};
    buffer << Tag{"td"} << "Field " << i << ": " << CTag{};
    buffer << Tag{"td"};
    buffer << Tag{"table"} << form.fields.at(i) << CTag{"table"};
    buffer << CTag{"td"};
    buffer << CTag{"tr"};
  }
  buffer << CTag{"table"};
  buffer << CTag{"div"};
  return buffer;
}

bool FormDataEqualForTesting(const FormData& lhs, const FormData& rhs) {
  FormData::IdentityComparator less;
  return !less(lhs, rhs) && !less(rhs, lhs);
}

}  // namespace autofill
