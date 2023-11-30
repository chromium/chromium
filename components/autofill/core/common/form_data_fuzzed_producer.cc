// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data_fuzzed_producer.h"

#include <bitset>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

namespace {

// A wrapper to get |std::u16string| from a |FuzzedDataProvider|. The result is
// arbitrary bytes, not necessarily valid UTF16.
std::u16string ConsumeU16String(FuzzedDataProvider& provider) {
  // |FuzzedDataProvider| takes special steps to pick the length of the string
  // to support evolution of the fuzzed input. Let's follow whatever it does.
  const std::string s8 = provider.ConsumeRandomLengthString();
  return std::u16string(
      reinterpret_cast<const std::u16string::value_type*>(s8.data()),
      s8.size() / 2);
}

}  // namespace

FormData GenerateFormData(FuzzedDataProvider& provider) {
  FormData result;

  result.is_form_tag = provider.ConsumeBool();

  // Determine how many fields this form will have. Pick a low value because
  // after the fuzzer's seed is exhausted, all will be 0s anyway.
  const size_t number_of_fields =
      provider.ConsumeIntegralInRange<size_t>(0, 15);
  result.fields.resize(number_of_fields);

  result.name = ConsumeU16String(provider);
  result.action = GURL(provider.ConsumeRandomLengthString());
  result.url = GURL(provider.ConsumeRandomLengthString());
  result.main_frame_origin =
      url::Origin::Create(GURL(provider.ConsumeRandomLengthString()));

  int first_field_with_same_value = -1;
  for (size_t i = 0; i < number_of_fields; ++i) {
    // Batch getting bits from the FuzzedDataProvider, because calling
    // `ConsumeBool` throws out 7 bits and we need multiple Booleans for each
    // iteration.
    const std::bitset<8> bools(provider.ConsumeIntegral<uint8_t>());
    // All instances with |same_value_field| true share the same value.
    const bool same_value_field = bools[0];
    // Empty values are interesting from the parsing perspective. Ensure that
    // at least half of the cases ends up with an empty value.
    const bool force_empty_value = bools[1];
    result.fields[i].is_focusable = bools[2];

    result.fields[i].form_control_type =
        provider.ConsumeEnum<FormControlType>();
    result.fields[i].autocomplete_attribute =
        provider.ConsumeRandomLengthString();
    result.fields[i].label = ConsumeU16String(provider);
    result.fields[i].name = ConsumeU16String(provider);
    result.fields[i].name_attribute = result.fields[i].name;
    result.fields[i].id_attribute = ConsumeU16String(provider);
    result.fields[i].unique_renderer_id =
        FieldRendererId(provider.ConsumeIntegralInRange(-32, 31));

    if (same_value_field) {
      if (first_field_with_same_value == -1) {
        first_field_with_same_value = static_cast<int>(i);
      } else {
        result.fields[i].value =
            result.fields[first_field_with_same_value].value;
      }
    } else if (!force_empty_value) {
      result.fields[i].value = ConsumeU16String(provider);
    }
  }

  return result;
}

}  // namespace autofill
