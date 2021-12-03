// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/lib_util.h"

#include <string>

#include "base/strings/escape.h"
#include "build/build_config.h"

namespace updater {

#if defined(OS_LINUX)

std::string UnescapeURLComponent(base::StringPiece escaped_text) {
  return base::UnescapeURLComponent(
      escaped_text,
      base::UnescapeRule::SPACES |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
          base::UnescapeRule::PATH_SEPARATORS);
}

#endif  // OS_LINUX

}  // namespace updater
