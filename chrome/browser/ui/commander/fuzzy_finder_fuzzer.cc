// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stddef.h>
#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"

#include <fuzzer/FuzzedDataProvider.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);
  std::vector<gfx::Range> ranges;
  std::u16string needle =
      base::UTF8ToUTF16(provider.ConsumeRandomLengthString());
  std::u16string haystack =
      base::UTF8ToUTF16(provider.ConsumeRandomLengthString());

  commander::FuzzyFinder(needle).Find(haystack, &ranges);
  return 0;
}
