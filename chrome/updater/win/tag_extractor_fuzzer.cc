// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "chrome/updater/win/tag_extractor.h"
#include "chrome/updater/win/tag_extractor_impl.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::vector<uint8_t> buffer(data, data + size);

  std::string tag_utf8 =
      updater::ExtractTagFromBuffer(buffer, updater::TagEncoding::kUtf8);
  std::string tag_utf16 =
      updater::ExtractTagFromBuffer(buffer, updater::TagEncoding::kUtf16);

  return 0;
}
