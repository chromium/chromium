// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_view.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string16.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // This fuzzer creates a random UTF16 string to represent clipboard contents.
  base::string16 s(reinterpret_cast<const base::string16::value_type*>(data),
                   size / sizeof(base::string16::value_type));
  OmniboxView::SanitizeTextForPaste(s);
  return 0;
}
