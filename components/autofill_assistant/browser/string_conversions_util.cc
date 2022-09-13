// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/string_conversions_util.h"

#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/utf8.h"

namespace autofill_assistant {

std::vector<UChar32> UTF8ToUnicode(const std::string& text) {
  std::vector<UChar32> codepoints;
  codepoints.reserve(text.length());  // upper bound
  for (base::i18n::UTF8CharIterator iter(text); !iter.end(); iter.Advance()) {
    codepoints.emplace_back(iter.get());
  }
  return codepoints;
}

bool UnicodeToUTF8(const std::vector<UChar32>& source, std::string* target) {
  target->reserve(target->size() + source.size() * 4);
  for (auto codepoint : source) {
    if (!AppendUnicodeToUTF8(codepoint, target)) {
      DVLOG(1) << __func__ << ": Failed to convert codepoint " << codepoint
               << " to UTF-8";
      return false;
    }
  }
  return true;
}

// Converts a single unicode codepoint to UTF-8 and appends the result to
// |target|.
bool AppendUnicodeToUTF8(const UChar32 source, std::string* target) {
  char bytes[4];
  UBool error = false;
  size_t offset = 0;
  U8_APPEND(bytes, offset, std::size(bytes), source, error);
  if (error == false) {
    target->append(bytes, offset);
  }
  return !error;
}

}  // namespace autofill_assistant
