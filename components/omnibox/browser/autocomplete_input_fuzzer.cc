// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_input.h"

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

// From crbug.com/774858
struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // Used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment icu_env;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Enforce a reasonable bound on what we believe it takes to trigger
  // an error.
  if (size > 4096)
    return 0;
  // This fuzzer creates a random UTF16 string, for testing primarily against
  // AutocompleteInput::Parse().
  base::string16 s(reinterpret_cast<const base::string16::value_type*>(data),
                   size / sizeof(base::string16::value_type));
  // Some characters are considered illegal and, while our code handles them
  // fine, fuzzing runs with DCHECKs enabled which will trigger on them.
  for (auto c : s) {
    if (!base::IsValidCharacter(c))
      return 0;
  }
  AutocompleteInput input(s, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  return 0;
}
