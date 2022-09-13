// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_view.h"

#include <stddef.h>
#include <stdint.h>

#include <string>


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // This fuzzer creates a random UTF16 string to represent clipboard contents.
  std::u16string s(reinterpret_cast<const std::u16string::value_type*>(data),
                   size / sizeof(std::u16string::value_type));
  OmniboxView::SanitizeTextForPaste(s);
  return 0;
}
