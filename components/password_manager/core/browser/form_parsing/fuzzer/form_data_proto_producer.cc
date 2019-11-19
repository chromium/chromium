// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/fuzzer/form_data_proto_producer.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
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

  result.id_attribute = UTF8ToUTF16(form_proto.id());
  result.name_attribute = UTF8ToUTF16(form_proto.name());
  result.is_form_tag = form_proto.is_form_tag();
  result.is_formless_checkout = form_proto.is_formless_checkout();
  result.name = UTF8ToUTF16(form_proto.name());
  result.action = GURL(form_proto.action());
  result.url = GURL(form_proto.origin());
  result.main_frame_origin =
      url::Origin::Create(GURL(form_proto.main_frame_origin()));

  result.fields.resize(form_proto.fields_size());
  for (int i = 0; i < form_proto.fields_size(); ++i) {
    const ::form_data_fuzzer::FormField& form_data_proto = form_proto.fields(i);
    result.fields[i].id_attribute = UTF8ToUTF16(form_data_proto.id());
#if defined(OS_IOS)
    result.fields[i].unique_id = result.fields[i].id_attribute +
                                 base::UTF8ToUTF16("-") +
                                 base::NumberToString16(i);
#endif
    result.fields[i].name_attribute = UTF8ToUTF16(form_data_proto.name());
    result.fields[i].is_focusable = form_data_proto.is_focusable();
    result.fields[i].form_control_type = form_data_proto.form_control_type();
    result.fields[i].autocomplete_attribute =
        form_data_proto.autocomplete_attribute();
    result.fields[i].label = UTF8ToUTF16(form_data_proto.label());
    result.fields[i].name = UTF8ToUTF16(form_data_proto.name());
    result.fields[i].value = UTF8ToUTF16(form_data_proto.value());
  }

  return result;
}

}  // namespace password_manager
