// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_text_util.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "url/url_constants.h"

namespace omnibox {

std::u16string StripJavascriptSchemas(const std::u16string& text) {
  const std::u16string kJsPrefix(
      base::StrCat({url::kJavaScriptScheme16, u":"}));

  bool found_JavaScript = false;
  size_t i = 0;
  // Find the index of the first character that isn't whitespace, a control
  // character, or a part of a JavaScript: scheme.
  while (i < text.size()) {
    if (base::IsUnicodeWhitespace(text[i]) || (text[i] < 0x20)) {
      ++i;
    } else {
      if (!base::EqualsCaseInsensitiveASCII(text.substr(i, kJsPrefix.length()),
                                            kJsPrefix)) {
        break;
      }

      // We've found a JavaScript scheme. Continue searching to ensure that
      // strings like "javascript:javascript:alert()" are fully stripped.
      found_JavaScript = true;
      i += kJsPrefix.length();
    }
  }

  // If we found any "JavaScript:" schemes in the text, return the text starting
  // at the first non-whitespace/control character after the last instance of
  // the scheme.
  if (found_JavaScript) {
    return text.substr(i);
  }

  return text;
}

std::u16string SanitizeTextForPaste(const std::u16string& text) {
  if (text.empty()) {
    return std::u16string();  // Nothing to do.
  }

  size_t end = text.find_first_not_of(base::kWhitespaceUTF16);
  if (end == std::u16string::npos) {
    return u" ";  // Convert all-whitespace to single space.
  }
  // Because `end` points at the first non-whitespace character, the loop
  // below will skip leading whitespace.

  // Reserve space for the sanitized output.
  std::u16string output;
  output.reserve(text.size());  // Guaranteed to be large enough.

  // Copy all non-whitespace sequences.
  // Do not copy trailing whitespace.
  // Copy all other whitespace sequences that do not contain CR/LF.
  // Convert all other whitespace sequences that do contain CR/LF to either ' '
  // or nothing, depending on whether there are any other sequences that do not
  // contain CR/LF.
  bool output_needs_lf_conversion = false;
  bool seen_non_lf_whitespace = false;
  const auto copy_range = [&text, &output](size_t begin, size_t end) {
    output +=
        text.substr(begin, (end == std::u16string::npos) ? end : (end - begin));
  };
  constexpr char16_t kNewline[] = {'\n', 0};
  constexpr char16_t kSpace[] = {' ', 0};
  while (true) {
    // Copy this non-whitespace sequence.
    size_t begin = end;
    end = text.find_first_of(base::kWhitespaceUTF16, begin + 1);
    copy_range(begin, end);

    // Now there is either a whitespace sequence, or the end of the string.
    if (end != std::u16string::npos) {
      // There is a whitespace sequence; see if it contains CR/LF.
      begin = end;
      end = text.find_first_not_of(base::kWhitespaceNoCrLfUTF16, begin);
      if ((end != std::u16string::npos) && (text[end] != '\n') &&
          (text[end] != '\r')) {
        // Found a non-trailing whitespace sequence without CR/LF. Copy it.
        seen_non_lf_whitespace = true;
        copy_range(begin, end);
        continue;
      }
    }

    // `end` either points at the end of the string or a CR/LF.
    if (end != std::u16string::npos) {
      end = text.find_first_not_of(base::kWhitespaceUTF16, end + 1);
    }
    if (end == std::u16string::npos) {
      break;  // Ignore any trailing whitespace.
    }

    // The preceding whitespace sequence contained CR/LF. Convert to a single
    // LF that we'll fix up below the loop.
    output_needs_lf_conversion = true;
    output += '\n';
  }

  // Convert LFs to ' ' or '' depending on whether there were non-LF whitespace
  // sequences.
  if (output_needs_lf_conversion) {
    base::ReplaceChars(output, kNewline,
                       seen_non_lf_whitespace ? kSpace : std::u16string(),
                       &output);
  }

  return StripJavascriptSchemas(output);
}

}  // namespace omnibox
