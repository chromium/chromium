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

FormData::FormData() : is_form_tag(true), is_formless_checkout(false) {}

FormData::FormData(const FormData& data)
    : name(data.name),
      button_title(data.button_title),
      origin(data.origin),
      action(data.action),
      main_frame_origin(data.main_frame_origin),
      is_form_tag(data.is_form_tag),
      is_formless_checkout(data.is_formless_checkout),
      unique_renderer_id(data.unique_renderer_id),
      fields(data.fields),
      username_predictions(data.username_predictions) {}

FormData::~FormData() {
}

bool FormData::SameFormAs(const FormData& form) const {
  if (name != form.name || origin != form.origin || action != form.action ||
      is_form_tag != form.is_form_tag ||
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
  if (name != form.name || origin != form.origin || action != form.action ||
      is_form_tag != form.is_form_tag ||
      is_formless_checkout != form.is_formless_checkout ||
      fields.size() != form.fields.size())
    return false;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (!fields[i].SimilarFieldAs(form.fields[i]))
      return false;
  }
  return true;
}

bool FormData::DynamicallySameFormAs(const FormData& form) const {
  if (name != form.name || fields.size() != form.fields.size())
    return false;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (!fields[i].DynamicallySameFieldAs(form.fields[i]))
      return false;
  }
  return true;
}

bool FormData::operator==(const FormData& form) const {
  return name == form.name && origin == form.origin && action == form.action &&
         unique_renderer_id == form.unique_renderer_id &&
         is_form_tag == form.is_form_tag &&
         is_formless_checkout == form.is_formless_checkout &&
         fields == form.fields &&
         username_predictions == form.username_predictions;
}

bool FormData::operator!=(const FormData& form) const {
  return !(*this == form);
}

bool FormData::operator<(const FormData& form) const {
  return std::tie(name, origin, action, is_form_tag, is_formless_checkout,
                  fields) < std::tie(form.name, form.origin, form.action,
                                     form.is_form_tag,
                                     form.is_formless_checkout, form.fields);
}

std::ostream& operator<<(std::ostream& os, const FormData& form) {
  os << base::UTF16ToUTF8(form.name) << " " << form.origin << " " << form.action
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
  pickle->WriteString(form_data.origin.spec());
  pickle->WriteString(form_data.action.spec());
  SerializeFormFieldDataVector(form_data.fields, pickle);
  pickle->WriteBool(form_data.is_form_tag);
  pickle->WriteBool(form_data.is_formless_checkout);
  pickle->WriteString(form_data.main_frame_origin.Serialize());
}

void SerializeFormDataToBase64String(const FormData& form_data,
                                     std::string* output) {
  base::Pickle pickle;
  SerializeFormData(form_data, &pickle);
  Base64Encode(
      base::StringPiece(static_cast<const char*>(pickle.data()), pickle.size()),
      output);
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
  if (!ReadGURL(iter, &temp_form_data.origin) ||
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

bool DeserializeFormDataFromBase64String(const base::StringPiece& input,
                                         FormData* form_data) {
  if (input.empty())
    return false;
  std::string pickle_data;
  Base64Decode(input, &pickle_data);
  base::Pickle pickle(pickle_data.data(), static_cast<int>(pickle_data.size()));
  base::PickleIterator iter(pickle);
  return DeserializeFormData(&iter, form_data);
}

}  // namespace autofill
