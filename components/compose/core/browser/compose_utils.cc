// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_utils.h"

#include "base/strings/string_tokenizer.h"

namespace compose {

namespace {
const std::string kWhitespace = " ,.\r\n\t\f\v";
}

bool IsWordCountWithinBounds(const std::string& prompt,
                             unsigned int minimum,
                             unsigned int maximum) {
  base::StringTokenizer tokenizer(
      prompt, kWhitespace, base::StringTokenizer::WhitespacePolicy::kSkipOver);

  unsigned int word_count = 0;
  while (tokenizer.GetNext()) {
    ++word_count;
    if (word_count > maximum) {
      return false;
    }
  }

  if (word_count < minimum) {
    return false;
  }
  return true;
}

}  // namespace compose
