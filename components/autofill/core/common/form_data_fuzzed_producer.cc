// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data_fuzzed_producer.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <bitset>
#include <string>

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

FieldRendererId FindNthUnusedRendererId(
    std::vector<FieldRendererId>& unused_ids,
    size_t n) {
  FieldRendererId result = unused_ids[n];
  std::swap(unused_ids[n], unused_ids.back());
  unused_ids.pop_back();
  return result;
}

FormData GenerateFormData(FuzzedDataProvider& provider) {
  FormData result;

  result.set_name(ConsumeU16String(provider));
  result.set_action(GURL(provider.ConsumeRandomLengthString()));
  result.set_url(GURL(provider.ConsumeRandomLengthString()));
  result.set_main_frame_origin(
      url::Origin::Create(GURL(provider.ConsumeRandomLengthString())));

  // Determine how many fields this form will have. Pick a low value because
  // after the fuzzer's seed is exhausted, all will be 0s anyway.
  const size_t number_of_fields =
      provider.ConsumeIntegralInRange<size_t>(0, 15);
  std::vector<FieldRendererId> unused_renderer_ids;
  unused_renderer_ids.push_back(FieldRendererId());
  for (size_t i = 1; i <= number_of_fields; ++i) {
    unused_renderer_ids.push_back(FieldRendererId(i));
  }
  std::vector<FormFieldData> fields;
  fields.resize(number_of_fields);
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
    fields[i].set_is_focusable(bools[2]);

    fields[i].set_form_control_type(provider.ConsumeEnum<FormControlType>());
    fields[i].set_autocomplete_attribute(provider.ConsumeRandomLengthString());
    fields[i].set_label(ConsumeU16String(provider));
    fields[i].set_name(ConsumeU16String(provider));
    fields[i].set_name_attribute(fields[i].name());
    fields[i].set_id_attribute(ConsumeU16String(provider));
    fields[i].set_renderer_id(FindNthUnusedRendererId(
        /*unused_ids=*/unused_renderer_ids,
        /*n=*/provider.ConsumeIntegralInRange<size_t>(
            /*min=*/0, /*max=*/unused_renderer_ids.size() - 1)));

    if (same_value_field) {
      if (first_field_with_same_value == -1) {
        first_field_with_same_value = static_cast<int>(i);
      } else {
        fields[i].set_value(fields[first_field_with_same_value].value());
      }
    } else if (!force_empty_value) {
      fields[i].set_value(ConsumeU16String(provider));
    }
  }
  result.set_fields(std::move(fields));

  return result;
}

}  // namespace autofill
