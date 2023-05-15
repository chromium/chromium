// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>

#include "components/feedback/redaction_tool/redaction_tool.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  int first_party_extension_id_count = provider.ConsumeIntegralInRange(-1, 50);
  // This is the storage for the strings inside first_party_extension_ids. This
  // is to make sure the char *'s we pass to the RedactionTool constructor are
  // deleted correctly -- they must be deleted after redactor is destructed, but
  // not leaked.
  std::vector<std::string> first_party_extension_id_store;
  // The first_party_extension_ids we pass to the RedactionTool constructor.
  // This owns the array but not the pointed-to strings. Note that if
  // first_party_extension_id_count is -1, this is not set so we pass nullptr to
  // the constructor; that's deliberate.
  std::unique_ptr<const char*[]> first_party_extension_ids;
  if (first_party_extension_id_count >= 0) {
    first_party_extension_id_store.reserve(first_party_extension_id_count);
    first_party_extension_ids =
        std::make_unique<const char*[]>(first_party_extension_id_count + 1);
    for (int i = 0; i < first_party_extension_id_count; ++i) {
      constexpr int kArbitraryMaxNameLength = 4096;
      first_party_extension_id_store.emplace_back(
          provider.ConsumeRandomLengthString(kArbitraryMaxNameLength));
      first_party_extension_ids[i] = first_party_extension_id_store[i].c_str();
    }
    first_party_extension_ids[first_party_extension_id_count] = nullptr;
  }

  redaction::RedactionTool redactor(first_party_extension_ids.get());
  redactor.EnableCreditCardRedaction(true);
  redactor.Redact(provider.ConsumeRemainingBytesAsString());
  return 0;
}
