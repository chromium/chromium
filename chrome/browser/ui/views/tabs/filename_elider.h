// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_FILENAME_ELIDER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_FILENAME_ELIDER_H_

#include <string>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"

// Helper class used to elide local filenames with a RenderText object that
// is provided with the correct setup and formatting.
class FilenameElider {
 public:
  using LineLengths = std::pair<size_t, size_t>;

  explicit FilenameElider(std::unique_ptr<gfx::RenderText> render_text);
  ~FilenameElider();

  // Returns the elided text. Equivalent to:
  //   Elide(GetLineLengths(display_rect))
  // See those methods for a detailed description.
  std::u16string Elide(const std::u16string& text,
                       const gfx::Rect& display_rect) const;

  // Returns the start of the image dimensions as typically formatted by
  // Chrome in page titles, as a hint at how to potentially elide or split
  // the title. Expects something in the format "title (width×height)".
  // Returns std::u16string::npos if this pattern isn't found, otherwise
  // returns the index of the opening parenthesis in the string.
  //
  // If the result isn't npos, then the character previous to the open paren
  // character is guaranteed to be whitespace.
  static std::u16string::size_type FindImageDimensions(
      const std::u16string& text);

 private:
  friend class TabHoverCardBubbleViewFilenameEliderTest;

  // Given the current text and a rectangle to display text in, returns the
  // maximum length in characters of the first and second lines.
  //
  // The first value is the number of characters from the beginning of the
  // text that will fit on the line. The second value is the number of
  // characters from the end of the text that will fit on a line, minus
  // enough space to insert an ellipsis.
  //
  // Note that the sum of the two values may be greater than the length of
  // the text. Both segments are guaranteed to end at grapheme boundaries.
  LineLengths GetLineLengths(const gfx::Rect& display_rect) const;

  // Returns a string formatted for two-line elision given the last string
  // passed to SetText() and the maximum extent of the first and second
  // lines. The resulting string will either be the original text (if it fits
  // on one line) or the first line, followed by a newline, an ellipsis, and
  // the second line. The cut points passed in must be at grapheme
  // boundaries.
  //
  // If the two lines overlap (that is, if the line lengths sum to more than
  // the length of the original text), an optimum breakpoint will be chosen
  // to insert the newline:
  //  * If possible, the extension (and if it's an image, the image
  //    dimensions) will be placed alone on the second line.
  //  * Otherwise, as many characters as possible will be placed on the first
  //    line.
  // TODO(dfried): consider optimizing to break at natural breaks: spaces,
  // punctuation, etc.
  //
  // Note that if the extension is isolated on the second line or an ellipsis
  // is inserted, the second line will be marked as a bidirectional isolate,
  // so that its direction is determined by the leading text on the line
  // rather than whatever is "left over" from the first line. We find this
  // produces a much more visually appealing and less confusing result than
  // inheriting the preceding directionality.
  std::u16string ElideImpl(LineLengths line_lengths) const;

  std::unique_ptr<gfx::RenderText> render_text_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_FILENAME_ELIDER_H_
