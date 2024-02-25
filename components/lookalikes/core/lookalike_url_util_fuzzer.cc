// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include "base/i18n/icu_util.h"
#include "components/lookalikes/core/lookalike_url_util.h"

class ICUInitializer {
 public:
  ICUInitializer() { base::i18n::InitializeICU(); }
};

static ICUInitializer g_icu_initializer;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  lookalikes::LookalikeUrlMatchType match_type =
      data_provider.ConsumeEnum<lookalikes::LookalikeUrlMatchType>();
  GURL navigated_url = GURL(data_provider.ConsumeRandomLengthString());
  std::string matched_hostname = data_provider.ConsumeRemainingBytesAsString();
  // Ignore inputs matching the following, to avoid a false positive
  std::string suggested_domain = lookalikes::GetETLDPlusOne(matched_hostname);
  if (suggested_domain.empty()) {
    return 0;
  }
  lookalikes::GetSuggestedURL(match_type, navigated_url, matched_hostname);
  return 0;
}
