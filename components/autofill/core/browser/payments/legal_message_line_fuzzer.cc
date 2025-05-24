// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/legal_message_line.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <optional>

#include "base/json/json_reader.h"
#include "base/values.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Prepare fuzzed parameters.
  std::optional<base::Value::Dict> legal_message =
      base::JSONReader::ReadDict(provider.ConsumeRandomLengthString());
  if (!legal_message) {
    return 0;
  }
  bool escape_apostrophes = provider.ConsumeBool();

  // Run tested code.
  autofill::LegalMessageLines lines;
  autofill::LegalMessageLine::Parse(*legal_message, &lines, escape_apostrophes);

  return 0;
}
