// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/filename_elider.h"

#include <string_view>

#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

FilenameElider::FilenameElider(std::unique_ptr<gfx::RenderText> render_text)
    : render_text_(std::move(render_text)) {}

FilenameElider::~FilenameElider() = default;

std::u16string FilenameElider::Elide(const std::u16string& text,
                                     const gfx::Rect& display_rect) const {
  render_text_->SetText(text);
  return ElideImpl(GetLineLengths(display_rect));
}

// static
std::u16string::size_type FilenameElider::FindImageDimensions(
    const std::u16string& text) {
  // We don't have regexes in Chrome, but we can still do a rough evaluation of
  // the line to see if it ends with the expected pattern:
  //
  // title[ (width×height)]
  //
  // We'll look for the open parenthesis, then the rest of the size. Note that
  // we don't have to worry about graphemes or combining characters because any
  // character that's not of the expected type means there is no dimension.

  // Find the start of the extension.
  const auto paren_pos = text.find_last_of(u'(');
  if (paren_pos == 0 || paren_pos == std::u16string::npos ||
      text[paren_pos - 1] != u' ') {
    return std::u16string::npos;
  }

  // Fast forward to the unicode character following the paren.
  base::i18n::UTF16CharIterator it(
      std::u16string_view(text).substr(paren_pos + 1));

  // Look for the image width.
  if (!base::IsAsciiDigit(it.get())) {
    return std::u16string::npos;
  }
  while (it.Advance() && base::IsAsciiDigit(it.get())) {
    // empty loop
  }

  // Look for the × character and the height.
  constexpr char16_t kMultiplicationSymbol = u'\u00D7';
  if (it.end() || it.get() != kMultiplicationSymbol || !it.Advance() ||
      !base::IsAsciiDigit(it.get())) {
    return std::u16string::npos;
  }
  while (it.Advance() && base::IsAsciiDigit(it.get())) {
    // empty loop
  }

  // Look for the closing parenthesis and make sure we've hit the end of the
  // string.
  if (it.end() || it.get() != u')') {
    return std::u16string::npos;
  }
  it.Advance();
  return it.end() ? paren_pos : std::u16string::npos;
}

FilenameElider::LineLengths FilenameElider::GetLineLengths(
    const gfx::Rect& display_rect) const {
  const std::u16string text = render_text_->text();
  render_text_->SetMaxLines(0);
  render_text_->SetMultiline(false);
  render_text_->SetWhitespaceElision(true);
  render_text_->SetDisplayRect(display_rect);

  // Set our temporary RenderText to the unelided text and elide the start of
  // the string to give us a guess at where the second line of the label
  // should start.
  render_text_->SetElideBehavior(gfx::ElideBehavior::ELIDE_HEAD);
  const std::u16string tentative_second_line = render_text_->GetDisplayText();

  // If there is no elision, then the text will fit on a single line and
  // there's nothing to do.
  if (tentative_second_line == text) {
    return LineLengths(text.length(), text.length());
  }

  // If there's not enough space to display even a single character, there is
  // also nothing to do; the result needs to be empty.
  if (tentative_second_line.empty()) {
    return LineLengths(0, 0);
  }

  LineLengths result;

  // Since we truncated, expect the string to start with ellipsis, then
  // calculate the length of the string sans ellipsis.
  DCHECK_EQ(gfx::kEllipsisUTF16[0], tentative_second_line[0]);

  // TODO(crbug.com/1239317): Elision is still a little flaky, so we'll make
  // sure we didn't stop in the middle of a grapheme. The +1 is to move past
  // the ellipsis which is not part of the original string.
  size_t pos = text.length() - tentative_second_line.length() + 1;
  if (!render_text_->IsGraphemeBoundary(pos)) {
    pos = render_text_->IndexOfAdjacentGrapheme(pos, gfx::CURSOR_FORWARD);
  }
  result.second = text.length() - pos;

  // Calculate the first line by aggressively truncating the text. This may
  // cut the string somewhere other than a word boundary, but for very long
  // filenames, it's probably best to fit as much of the name on the card as
  // possible, even if we sacrifice a small amount of readability.
  render_text_->SetElideBehavior(gfx::ElideBehavior::TRUNCATE);
  result.first = render_text_->GetDisplayText().length();

  // TOOD(crbug.com/1239317) Handle the case where we ended up in the middle
  // of a grapheme.
  if (!render_text_->IsGraphemeBoundary(result.first)) {
    result.first = render_text_->IndexOfAdjacentGrapheme(result.first,
                                                         gfx::CURSOR_BACKWARD);
  }

  return result;
}

std::u16string FilenameElider::ElideImpl(
    FilenameElider::LineLengths line_lengths) const {
  const std::u16string& text = render_text_->text();

  // Validate the inputs. All of these are base assumptions.
  DCHECK_LE(line_lengths.first, text.length());
  DCHECK_LE(line_lengths.second, text.length());
  DCHECK(render_text_->IsGraphemeBoundary(line_lengths.first));
  DCHECK(render_text_->IsGraphemeBoundary(text.length() - line_lengths.second));

  // If the entire text fits on a single line, use it as-is.
  if (line_lengths.first == text.length() ||
      line_lengths.second == text.length()) {
    return text;
  }

  // If no characters will fit on one of the lines, return an empty string.
  if (line_lengths.first == 0 || line_lengths.second == 0) {
    return std::u16string();
  }

  // Let's figure out where to actually start the second line. Strings that
  // are too long for one line but fit on two lines tend to create some
  // overlap between the first and second line, so take the maximum of the
  // second line cut and the end of the first line.
  const size_t second_line_cut = text.length() - line_lengths.second;
  size_t cut_point = std::max(second_line_cut, line_lengths.first);

  // We got the whole line if the cut point is the character immediately
  // after the first line cuts off (otherwise we've truncated and need to
  // show an ellipsis in the final string).
  const bool is_whole_string = (cut_point == line_lengths.first);

  // If there is some flexibility in where we make our cut point (that is, the
  // potential first and second lines overlap), there are a few specific places
  // we preferentially want to separate the lines.
  bool adjusted_cut_point = false;
  if (is_whole_string && cut_point >= second_line_cut) {
    // First, if there are image dimensions, preferentially put those on the
    // second line.
    const auto paren_pos = FindImageDimensions(text);
    if (paren_pos != std::u16string::npos && paren_pos >= second_line_cut &&
        paren_pos <= cut_point) {
      cut_point = paren_pos;
      adjusted_cut_point = true;
    }

    // Second, we can break at the start of the file extension.
    if (!adjusted_cut_point) {
      const size_t dot_pos = text.find_last_of(u'.');
      if (dot_pos != std::u16string::npos && dot_pos >= second_line_cut &&
          dot_pos <= cut_point) {
        cut_point = dot_pos;
        adjusted_cut_point = true;
      }
    }
  }

  // TODO(dfried): possibly handle the case where we chop a section with bidi
  // delimiters out or split it between lines.

  // If we didn't put the extension on its own line, eliminate whitespace
  // from the start of the second line (it looks weird).
  if (!adjusted_cut_point) {
    cut_point =
        gfx::FindValidBoundaryAfter(text, cut_point, /*trim_whitespace =*/true);
  }

  // Reassemble the string. Start with the first line up to `cut_point` or the
  // end of the line, whichever comes sooner.
  std::u16string result =
      text.substr(0, std::min(line_lengths.first, cut_point));
  result.push_back(u'\n');

  // If we're starting the second line with a file extension hint that the
  // directionality of the text might change by using an FSI mark. Allowing
  // the renderer to re-infer RTL-ness produces much better results in text
  // rendering when an RTL filename has an ASCII extension.
  //
  // TODO(dfried): Currently we do put an FSI before an ellipsis; this
  // results in the ellipsis being placed with the text that immediately
  // follows it (making the point of elision more obvious). If the text
  // following the cut is LTR it goes on the left, and if the text is RTL it
  // goes on the right. Reconsider if/how we should set text direction
  // following an ellipsis:
  // - No FSI would cause the ellipsis to align with the preceding rather
  //   than the following text. It would provide a bit more visual continuity
  //   between lines, but might be confusing as to where the text picks back
  //   up (as the next character might be on the opposite side of the line).
  // - We could preserve elided directionality markers, but they could end up
  //   aligning the ellipsis with text that is not present at all on the
  //   label.
  // - We could also force direction to match the start of the first line for
  //   consistency but that could result in an ellipsis that matches neither
  //   the preceding nor following text.
  //
  // TODO(dfried): move these declarations to rtl.h alongside e.g.
  // base::i18n::kRightToLeftMark
  constexpr char16_t kFirstStrongIsolateMark = u'\u2068';
  constexpr char16_t kPopDirectionalIsolateMark = u'\u2069';
  if (adjusted_cut_point || !is_whole_string) {
    result += kFirstStrongIsolateMark;
  }
  if (!is_whole_string) {
    result.push_back(gfx::kEllipsisUTF16[0]);
  }
  result.append(text.substr(cut_point));
  // If we added an FSI, we should bracket it with a PDI.
  if (adjusted_cut_point || !is_whole_string) {
    result += kPopDirectionalIsolateMark;
  }
  return result;
}
