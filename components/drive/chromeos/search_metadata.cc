// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/search_metadata.h"

#include <algorithm>

#include "base/i18n/string_search.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_system_core_util.h"
#include "net/base/escape.h"

namespace drive {
namespace internal {

namespace {

// Appends substring of |original_text| to |highlighted_text| with highlight.
void AppendStringWithHighlight(const base::string16& original_text,
                               size_t start,
                               size_t length,
                               bool highlight,
                               std::string* highlighted_text) {
  if (highlight)
    highlighted_text->append("<b>");

  highlighted_text->append(net::EscapeForHTML(
      base::UTF16ToUTF8(original_text.substr(start, length))));

  if (highlight)
    highlighted_text->append("</b>");
}

}  // namespace

bool FindAndHighlight(
    const std::string& text,
    const std::vector<std::unique_ptr<
        base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>>& queries,
    std::string* highlighted_text) {
  DCHECK(highlighted_text);
  highlighted_text->clear();

  // Check text matches with all queries.
  size_t match_start = 0;
  size_t match_length = 0;

  base::string16 text16 = base::UTF8ToUTF16(text);
  std::vector<bool> highlights(text16.size(), false);
  for (const auto& query : queries) {
    if (!query->Search(text16, &match_start, &match_length))
      return false;

    std::fill(highlights.begin() + match_start,
              highlights.begin() + match_start + match_length, true);
  }

  // Generate highlighted text.
  size_t start_current_segment = 0;

  for (size_t i = 0; i < text16.size(); ++i) {
    if (highlights[start_current_segment] == highlights[i])
      continue;

    AppendStringWithHighlight(
        text16, start_current_segment, i - start_current_segment,
        highlights[start_current_segment], highlighted_text);

    start_current_segment = i;
  }

  DCHECK_GE(text16.size(), start_current_segment);
  AppendStringWithHighlight(
      text16, start_current_segment, text16.size() - start_current_segment,
      highlights[start_current_segment], highlighted_text);

  return true;
}

}  // namespace internal
}  // namespace drive
