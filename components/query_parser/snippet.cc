// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_parser/snippet.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/brkiter.h"
#include "third_party/icu/source/common/unicode/utext.h"
#include "third_party/icu/source/common/unicode/utf8.h"

namespace query_parser {
namespace {

bool PairFirstLessThan(const Snippet::MatchPosition& a,
                       const Snippet::MatchPosition& b) {
  return a.first < b.first;
}

// Combines all pairs after offset in match_positions that are contained
// or touch the pair at offset.
void CoalescePositionsFrom(size_t offset,
                           Snippet::MatchPositions* match_positions) {
  DCHECK(offset < match_positions->size());
  Snippet::MatchPosition& pair((*match_positions)[offset]);
  ++offset;
  while (offset < match_positions->size() &&
         pair.second >= (*match_positions)[offset].first) {
    pair.second = std::max(pair.second, (*match_positions)[offset].second);
    match_positions->erase(match_positions->begin() + offset);
  }
}

// Makes sure there is a pair in match_positions that contains the specified
// range. This keeps the pairs ordered in match_positions by first, and makes
// sure none of the pairs in match_positions touch each other.
void AddMatch(size_t start,
              size_t end,
              Snippet::MatchPositions* match_positions) {
  DCHECK(start < end);
  DCHECK(match_positions);
  Snippet::MatchPosition pair(start, end);
  if (match_positions->empty()) {
    match_positions->push_back(pair);
    return;
  }
  // There's at least one match. Find the position of the new match,
  // potentially extending pairs around it.
  auto i = std::lower_bound(match_positions->begin(), match_positions->end(),
                            pair, &PairFirstLessThan);
  if (i != match_positions->end() && i->first == start) {
    // Match not at the end and there is already a pair with the same
    // start.
    if (end > i->second) {
      // New pair extends beyond existing pair. Extend existing pair and
      // coalesce matches after it.
      i->second = end;
      CoalescePositionsFrom(i - match_positions->begin(), match_positions);
    }  // else case, new pair completely contained in existing pair, nothing
       // to do.
  } else if (i == match_positions->begin()) {
    // Match at the beginning and the first pair doesn't have the same
    // start. Insert new pair and coalesce matches after it.
    match_positions->insert(i, pair);
    CoalescePositionsFrom(0, match_positions);
  } else {
    // Not at the beginning (but may be at the end).
    --i;
    if (start <= i->second && end > i->second) {
      // Previous element contains match. Extend it and coalesce.
      i->second = end;
      CoalescePositionsFrom(i - match_positions->begin(), match_positions);
    } else if (end > i->second) {
      // Region doesn't touch previous element. See if region touches current
      // element.
      ++i;
      if (i == match_positions->end() || end < i->first) {
        match_positions->insert(i, pair);
      } else {
        i->first = start;
        i->second = end;
        CoalescePositionsFrom(i - match_positions->begin(), match_positions);
      }
    }
  }
}

// Converts an index in a utf8 string into the index in the corresponding utf16
// string and returns the utf16 index. This is intended to be called in a loop
// iterating through a utf8 string.
//
// utf8_string: the utf8 string.
// utf8_length: length of the utf8 string.
// offset: the utf8 offset to convert.
// utf8_pos: current offset in the utf8 string. This is modified and on return
//           matches offset.
// wide_pos: current index in the wide string. This is the same as the return
//           value.
size_t AdvanceAndReturnUTF16Pos(const char* utf8_string,
                                int32_t utf8_length,
                                int32_t offset,
                                int32_t* utf8_pos,
                                size_t* utf16_pos) {
  DCHECK(offset >= *utf8_pos && offset <= utf8_length);

  UChar32 wide_char;
  while (*utf8_pos < offset) {
    U8_NEXT(utf8_string, *utf8_pos, utf8_length, wide_char);
    *utf16_pos += (wide_char <= 0xFFFF) ? 1 : 2;
  }
  return *utf16_pos;
}

// Given a character break iterator over a UTF-8 string, set the iterator
// position to |*utf8_pos| and move by |count| characters. |count| can
// be either positive or negative.
void MoveByNGraphemes(icu::BreakIterator* bi, int count, size_t* utf8_pos) {
  // Ignore the return value. A side effect of the current position
  // being set at or following |*utf8_pos| is exploited here.
  // It's simpler than calling following(n) and then previous().
  // isBoundary() is not very fast, but should be good enough for the
  // snippet generation. If not, revisit the way we scan in ComputeSnippet.
  bi->isBoundary(static_cast<int32_t>(*utf8_pos));
  bi->next(count);
  *utf8_pos = static_cast<size_t>(bi->current());
}

// The amount of context to include for a given hit. Note that it's counted
// in terms of graphemes rather than bytes.
const int kSnippetContext = 50;

// Returns true if next match falls within a snippet window
// from the previous match. The window size is counted in terms
// of graphemes rather than bytes in UTF-8.
bool IsNextMatchWithinSnippetWindow(icu::BreakIterator* bi,
                                    size_t previous_match_end,
                                    size_t next_match_start) {
  // If it's within a window in terms of bytes, it's certain
  // that it's within a window in terms of graphemes as well.
  if (next_match_start < previous_match_end + kSnippetContext)
    return true;
  bi->isBoundary(static_cast<int32_t>(previous_match_end));
  // An alternative to this is to call |bi->next()| at most
  // kSnippetContext times, compare |bi->current()| with |next_match_start|
  // after each call and return early if possible. There are other
  // heuristics to speed things up if necessary, but it's not likely that
  // we need to bother.
  bi->next(kSnippetContext);
  int64_t current = bi->current();
  return (next_match_start < static_cast<uint64_t>(current) ||
          current == icu::BreakIterator::DONE);
}

}  // namespace

// static
void Snippet::ExtractMatchPositions(const std::string& offsets_str,
                                    const std::string& column_num,
                                    MatchPositions* match_positions) {
  DCHECK(match_positions);
  if (offsets_str.empty())
    return;
  std::vector<std::string> offsets = base::SplitString(
      offsets_str, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // SQLite offsets are sets of four integers:
  //   column, query term, match offset, match length
  // Matches within a string are marked by (start, end) pairs.
  for (size_t i = 0; i < offsets.size() - 3; i += 4) {
    if (offsets[i] != column_num)
      continue;
    const size_t start = atoi(offsets[i + 2].c_str());
    const size_t end = start + atoi(offsets[i + 3].c_str());
    // Switch to DCHECK after debugging http://crbug.com/15261.
    CHECK(end >= start);
    AddMatch(start, end, match_positions);
  }
}

// static
void Snippet::ConvertMatchPositionsToWide(
    const std::string& utf8_string,
    Snippet::MatchPositions* match_positions) {
  DCHECK(match_positions);
  int32_t utf8_pos = 0;
  size_t utf16_pos = 0;
  const char* utf8_cstring = utf8_string.c_str();
  const int32_t utf8_length = static_cast<int32_t>(utf8_string.size());
  for (auto i = match_positions->begin(); i != match_positions->end(); ++i) {
    i->first = AdvanceAndReturnUTF16Pos(utf8_cstring, utf8_length,
                                        static_cast<int32_t>(i->first),
                                        &utf8_pos, &utf16_pos);
    i->second = AdvanceAndReturnUTF16Pos(utf8_cstring, utf8_length,
                                         static_cast<int32_t>(i->second),
                                         &utf8_pos, &utf16_pos);
  }
}

Snippet::Snippet() {
}

Snippet::Snippet(const Snippet& other) = default;

Snippet::Snippet(Snippet&& other) noexcept = default;

Snippet::~Snippet() {
}

Snippet& Snippet::operator=(const Snippet&) = default;

void Snippet::ComputeSnippet(const MatchPositions& match_positions,
                             const std::string& document) {
  // The length of snippets we try to produce.
  // We can generate longer snippets but stop once we cross kSnippetMaxLength.
  const size_t kSnippetMaxLength = 200;
  const base::string16 kEllipsis = base::ASCIIToUTF16(" ... ");

  UText* document_utext = nullptr;
  UErrorCode status = U_ZERO_ERROR;
  document_utext = utext_openUTF8(document_utext, document.data(),
                                  document.size(), &status);
  // Locale does not matter because there's no per-locale customization
  // for character iterator.
  std::unique_ptr<icu::BreakIterator> bi(
      icu::BreakIterator::createCharacterInstance(icu::Locale::getDefault(),
                                                  status));
  bi->setText(document_utext, status);
  DCHECK(U_SUCCESS(status));

  // We build the snippet by iterating through the matches and then grabbing
  // context around each match.  If matches are near enough each other (within
  // kSnippetContext), we skip the "..." between them.
  base::string16 snippet;
  size_t start = 0;
  for (size_t i = 0; i < match_positions.size(); ++i) {
    // Some shorter names for the current match.
    const size_t match_start = match_positions[i].first;
    const size_t match_end = match_positions[i].second;

    // Switch to DCHECK after debugging http://crbug.com/15261.
    CHECK(match_end > match_start);
    CHECK(match_end <= document.size());

    // Add the context, if any, to show before the match.
    size_t context_start = match_start;
    MoveByNGraphemes(bi.get(), -kSnippetContext, &context_start);
    start = std::max(start, context_start);
    if (start < match_start) {
      if (start > 0)
        snippet += kEllipsis;
      // Switch to DCHECK after debugging http://crbug.com/15261.
      CHECK(start < document.size());
      snippet += base::UTF8ToUTF16(document.substr(start, match_start - start));
    }

    // Add the match.
    const size_t first = snippet.size();
    snippet += base::UTF8ToUTF16(document.substr(match_start,
                                                 match_end - match_start));
    matches_.push_back(std::make_pair(first, snippet.size()));

    // Compute the context, if any, to show after the match.
    size_t end;
    // Check if the next match falls within our snippet window.
    if (i + 1 < match_positions.size() &&
        IsNextMatchWithinSnippetWindow(bi.get(), match_end,
            match_positions[i + 1].first)) {
      // Yes, it's within the window.  Make the end context extend just up
      // to the next match.
      end = match_positions[i + 1].first;
      // Switch to DCHECK after debugging http://crbug.com/15261.
      CHECK(end >= match_end);
      CHECK(end <= document.size());
      snippet += base::UTF8ToUTF16(document.substr(match_end, end - match_end));
    } else {
      // No, there's either no next match or the next match is too far away.
      end = match_end;
      MoveByNGraphemes(bi.get(), kSnippetContext, &end);
      // Switch to DCHECK after debugging http://crbug.com/15261.
      CHECK(end >= match_end);
      CHECK(end <= document.size());
      snippet += base::UTF8ToUTF16(document.substr(match_end, end - match_end));
      if (end < document.size())
        snippet += kEllipsis;
    }
    start = end;

    // Stop here if we have enough snippet computed.
    if (snippet.size() >= kSnippetMaxLength)
      break;
  }

  utext_close(document_utext);
  swap(text_, snippet);
}

void Snippet::Swap(Snippet* other) {
  text_.swap(other->text_);
  matches_.swap(other->matches_);
}

}  // namespace query_parser
