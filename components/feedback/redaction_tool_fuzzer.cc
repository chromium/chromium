// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/redaction_tool/redaction_tool.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"

namespace {

constexpr int kArbitraryMaxNameLength = 4096;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  int first_party_extension_id_count = provider.ConsumeIntegralInRange(-1, 50);
  // This is the storage for the strings inside
  // `unowned_first_party_extension_ids`. This is to make sure the
  // char *'s we pass to the RedactionTool constructor are deleted
  // correctly -- they must be deleted after redactor is destructed, but
  // not leaked.
  std::vector<std::string> first_party_extension_id_store;

  // The first_party_extension_ids we pass to the RedactionTool constructor.
  // This owns the array but not the pointed-to strings. Note that if
  // first_party_extension_id_count is -1, this is not set so we pass an empty
  // span to the constructor; that's deliberate.
  base::HeapArray<std::string_view> unowned_first_party_extension_ids;

  if (first_party_extension_id_count >= 0) {
    first_party_extension_id_store.reserve(first_party_extension_id_count);
    unowned_first_party_extension_ids =
        base::HeapArray<std::string_view>::WithSize(
            first_party_extension_id_count);

    for (int i = 0; i < first_party_extension_id_count; ++i) {
      first_party_extension_id_store.emplace_back(
          provider.ConsumeRandomLengthString(kArbitraryMaxNameLength));
      unowned_first_party_extension_ids[i] = first_party_extension_id_store[i];
    }
  }

  redaction::RedactionTool redactor(
      unowned_first_party_extension_ids.as_span());
  redactor.EnableCreditCardRedaction(true);
  redactor.Redact(provider.ConsumeRemainingBytesAsString());
  return 0;
}
