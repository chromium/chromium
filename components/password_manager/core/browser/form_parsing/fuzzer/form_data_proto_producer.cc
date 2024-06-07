// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/fuzzer/form_data_proto_producer.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/form_parsing/fuzzer/form_data_essentials.pb.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FormData;
using autofill::FormFieldData;
using base::UTF8ToUTF16;

namespace password_manager {

FormData GenerateWithProto(const ::form_data_fuzzer::Form& form_proto) {
  FormData result;

  // |kMaxParseableFields| is the largest number of fields the parser will
  // accept. Generating significantly more than that is just wasting fuzzing
  // resources. Allow exceeding this number though, to test that the parser
  // indeed handles such inputs.
  if (base::checked_cast<size_t>(form_proto.fields_size()) >
      FormDataParser::kMaxParseableFields + 1) {
    return result;
  }

  result.set_id_attribute(UTF8ToUTF16(form_proto.id()));
  result.set_name_attribute(UTF8ToUTF16(form_proto.name()));
  result.set_name(UTF8ToUTF16(form_proto.name()));
  result.set_action(GURL(form_proto.action()));
  result.set_url(GURL(form_proto.origin()));
  result.set_main_frame_origin(
      url::Origin::Create(GURL(form_proto.main_frame_origin())));

  std::vector<FormFieldData> fields;
  fields.reserve(form_proto.fields_size());
  for (const ::form_data_fuzzer::FormField& form_data_proto :
       form_proto.fields()) {
    FormFieldData& field = fields.emplace_back();
    field.set_id_attribute(UTF8ToUTF16(form_data_proto.id()));
    field.set_name_attribute(UTF8ToUTF16(form_data_proto.name()));
    field.set_is_focusable(form_data_proto.is_focusable());
    field.set_form_control_type(static_cast<autofill::FormControlType>(
        form_data_proto.form_control_type()));
    field.set_autocomplete_attribute(form_data_proto.autocomplete_attribute());
    field.set_label(UTF8ToUTF16(form_data_proto.label()));
    field.set_name(UTF8ToUTF16(form_data_proto.name()));
    field.set_value(UTF8ToUTF16(form_data_proto.value()));
  }
  result.set_fields(std::move(fields));

  return result;
}

}  // namespace password_manager
