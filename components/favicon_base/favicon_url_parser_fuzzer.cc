// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "components/favicon_base/favicon_url_parser.h"

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

chrome::FaviconUrlFormat GetFaviconUrlFormatFromUint8(uint8_t value) {
  // Dummy switch to detect changes to the enum definition.
  switch (chrome::FaviconUrlFormat()) {
    case chrome::FaviconUrlFormat::kFaviconLegacy:
    case chrome::FaviconUrlFormat::kFavicon2:
      break;
  }

  return (value % 2) == 0 ? chrome::FaviconUrlFormat::kFaviconLegacy
                          : chrome::FaviconUrlFormat::kFavicon2;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2)
    return 0;

  // The first byte is used to determine the FaviconUrlFormat, and the rest of
  // the data is used in the input string to parse.
  const chrome::FaviconUrlFormat url_format =
      GetFaviconUrlFormatFromUint8(data[0]);

  const std::string string_input(reinterpret_cast<const char*>(data + 1),
                                 size - 1);
  chrome::ParsedFaviconPath parsed;
  chrome::ParseFaviconPath(string_input, url_format, &parsed);
  return 0;
}
