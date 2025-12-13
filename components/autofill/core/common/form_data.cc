// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data.h"

#include <stddef.h>

#include <string_view>
#include <tuple>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/stream_operator_util.h"

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

// static
bool FormData::IdenticalAndEquivalentDomElements(
    const FormData& a,
    const FormData& b,
    DenseSet<FormFieldData::Exclusion> exclusions) {
  if (!base::FeatureList::IsEnabled(features::kAutofillFixFormEquality)) {
    // We compare all unique identifiers first, including the field renderer
    // IDs, because we expect most inequalities to be due to them.
    if (a.global_id() != b.global_id() ||
        a.child_frames() != b.child_frames() ||
        !std::ranges::equal(a.fields(), b.fields(), {},
                            &FormFieldData::global_id,
                            &FormFieldData::global_id)) {
      return false;
    }

    if (a.name() != b.name() || a.id_attribute() != b.id_attribute() ||
        a.name_attribute() != b.name_attribute() || a.url() != b.url() ||
        a.action() != b.action() ||
        a.likely_contains_captcha() != b.likely_contains_captcha() ||
        !std::ranges::equal(
            a.fields_, b.fields_,
            [&](const FormFieldData& f, const FormFieldData& g) {
              return FormFieldData::IdenticalAndEquivalentDomElements(
                  f, g, exclusions);
            })) {
      return false;
    }
    return true;
  }

  // LINT.IfChange(IdenticalAndEquivalentDomElements)
  // clang-format off
  return  // As optimization, compare the form and field IDs first.
      a.host_frame_ == b.host_frame_ &&
      a.renderer_id_ == b.renderer_id_ &&
      std::ranges::equal(a.fields_, b.fields_, {}, &FormFieldData::global_id,
                         &FormFieldData::global_id) &&
      // Now compare the other members.
      a.child_frames_ == b.child_frames_ &&
      a.id_attribute_ == b.id_attribute_ &&
      a.name_attribute_ == b.name_attribute_ &&
      a.name_ == b.name_ &&
      a.button_titles_ == b.button_titles_ &&
      a.url_ == b.url_ &&
      a.full_url_ == b.full_url_ &&
      a.action_ == b.action_ &&
      a.is_action_empty_ == b.is_action_empty_ &&
      // main_frame_origin_ is not compared because by it is initialized to an
      // opaque origin (a random number).
      a.submission_event_ == b.submission_event_ &&
      a.username_predictions_ == b.username_predictions_ &&
      a.is_gaia_with_skip_save_password_form_ ==
          b.is_gaia_with_skip_save_password_form_ &&
      a.likely_contains_captcha_ == b.likely_contains_captcha_ &&
      // version_ is not compared because it does not depend on the DOM.
      std::ranges::equal(a.fields_, b.fields_,
                         [&](const FormFieldData& f, const FormFieldData& g) {
                           return FormFieldData::
                               IdenticalAndEquivalentDomElements(
                                   f, g, exclusions);
                         });
  // clang-format on
  // LINT.ThenChange(form_data.h:FormDataMembers)
}

const FormFieldData* FormData::FindFieldByGlobalId(
    const FieldGlobalId& global_id) const {
  auto fields_it =
      std::ranges::find(fields(), global_id, &FormFieldData::global_id);

  // If the field is found, return a pointer to the field, otherwise return
  // nullptr.
  return fields_it != fields().end() ? &*fields_it : nullptr;
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
  return internal::PrintWithIndentation(os, form, /*indentation=*/0);
}

namespace internal {

std::ostream& PrintWithIndentation(std::ostream& os,
                                   const FormData& form,
                                   int indentation,
                                   std::string_view title) {
  std::string space = std::string(indentation, ' ');
  os << space << "{";
  if (!title.empty()) {
    os << " /*" << title << "*/";
  }
  os << '\n';
#define PRINT_PROPERTY(property)                                            \
  os << space << "  " << #property << ": " << PrintWrapper(form.property()) \
     << ",\n"
  PRINT_PROPERTY(global_id);
  PRINT_PROPERTY(name);
  PRINT_PROPERTY(url);
  PRINT_PROPERTY(main_frame_origin);
#undef PRINT_PROPERTY
  os << space << "  fields: [ /*length " << form.fields().size() << "*/\n";
  for (size_t i = 0; i < form.fields().size(); ++i) {
    internal::PrintWithIndentation(
        os, form.fields()[i], /*indentation=*/indentation + 4,
        base::StrCat({"FormFieldData index ", base::NumberToString(i)}));
    if (i < form.fields().size()) {
      os << ",";
    }
    os << '\n';
  }
  os << space << "  ]\n";
  os << space << "}";
  return os;
}

}  // namespace internal

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
