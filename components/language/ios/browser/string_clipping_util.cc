// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/ios/browser/string_clipping_util.h"

#include <stddef.h>

#include "base/strings/string_util.h"

std::u16string GetStringByClippingLastWord(const std::u16string& contents,
                                           size_t length) {
  if (contents.size() < length)
    return contents;

  std::u16string clipped_contents = contents.substr(0, length);
  size_t last_space_index =
      clipped_contents.find_last_of(base::kWhitespaceUTF16);
  if (last_space_index != std::u16string::npos)
    clipped_contents.resize(last_space_index);
  return clipped_contents;
}
