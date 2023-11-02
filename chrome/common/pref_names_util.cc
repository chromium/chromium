// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names_util.h"

#include <stddef.h>

#include "base/strings/string_util.h"

namespace pref_names_util {

const char kWebKitFontPrefPrefix[] = "webkit.webprefs.fonts.";

bool ParseFontNamePrefPath(const std::string& pref_path,
                           std::string* generic_family,
                           std::string* script) {
  if (!base::StartsWith(pref_path, kWebKitFontPrefPrefix,
                        base::CompareCase::SENSITIVE))
    return false;

  size_t start = strlen(kWebKitFontPrefPrefix);
  size_t pos = pref_path.find('.', start);
  if (pos == std::string::npos || pos + 1 == pref_path.length())
    return false;
  if (generic_family)
    *generic_family = pref_path.substr(start, pos - start);
  if (script)
    *script = pref_path.substr(pos + 1);
  return true;
}

}  // namespace pref_names_util
