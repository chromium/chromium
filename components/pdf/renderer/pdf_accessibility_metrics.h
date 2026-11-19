// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_METRICS_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_METRICS_H_

namespace pdf {

// LINT.IfChange(HeadingClassifier)
// Heuristic rules used to classify a text block as a heading.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HeadingClassifier {
  // Not classified as a heading. This value is not logged.
  kNone = 0,
  // Classified based on font size.
  kFontSize = 1,
  // Classified based on bold styling.
  kBoldStyle = 2,
  // Classified because all characters are uppercase.
  kAllUppercase = 3,
  // Classified due to semi-bold font weight (600).
  kSemiBoldWeight = 4,
  // Classified by looking at the font name to determine styling.
  kFontName = 5,
  // Classified by looking at text color when it differs from body text color.
  kTextColor = 6,

  kMaxValue = kTextColor,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PdfAccessibilityHeadingClassifier)

// LINT.IfChange(PdfAccessibilityListStartReason)
// Heuristic rules that started a list.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ListStartReason {
  kUnorderedMarker = 0,
  kNestedOnlyMarker = 1,
  kDecimalStartsAtOne = 2,
  kDecimalStartsAboveOne = 3,
  kLowerAlphaStartsAtA = 4,
  kLowerAlphaStartsAboveA = 5,

  kMaxValue = kLowerAlphaStartsAboveA,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PdfAccessibilityListStartReason)

// LINT.IfChange(PdfAccessibilityListEndReason)
// Heuristic conditions that ended a list.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ListEndReason {
  kPageEnd = 0,
  kNewListMarker = 1,
  kShallowerMarker = 2,
  kOutdent = 3,
  kOutOfSequenceMarker = 4,
  kGraphic = 5,
  kHeaderFooter = 6,
  kParagraph = 7,
  kHeading = 8,
  kTable = 9,

  kMaxValue = kTable,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PdfAccessibilityListEndReason)

// LINT.IfChange(HeaderFooterRepetition)
// How a heuristically detected header or footer's text repeats across pages in
// an untagged PDF. Headers are compared only with headers, and footers only
// with footers.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HeaderFooterRepetition {
  // Text matches no other header or footer in the document.
  kUnique = 0,
  // Text exactly matches another header or footer in the document.
  kRepeating = 1,
  // Text matches another header or footer only after digits are normalized,
  // e.g. "Page 3" and "Page 4".
  kIncremental = 2,

  kMaxValue = kIncremental,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PdfAccessibilityHeaderFooterRepetition)

// LINT.IfChange(PdfAccessibilityTableCandidateOutcome)
// Whether a candidate table in an untagged PDF was kept, or the first check
// that rejected it. Each table type is recorded in its own histogram, and some
// values apply only to some table types.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TableCandidateOutcome {
  kAccepted = 0,

  // Rejection reasons.

  // Grid only: a ruling line extends well past the grid, e.g. a chart axis.
  kOvershootingLines = 1,
  // No text in the candidate, e.g. a drawing.
  kEmpty = 2,
  // Text in the candidate is covered by a link, highlight, or form graphic.
  kHasAnnotation = 3,
  // Text in the candidate is right-to-left, e.g. Arabic or Hebrew. Cells are
  // ordered left to right, so they'd be read in the wrong order.
  // TODO(crbug.com/574033561): Accept these once right-to-left tables are
  // supported.
  kRightToLeftText = 4,
  // Grid only: too many text runs cross a column line, e.g. chart gridlines.
  kRunsCrossingColumns = 5,
  // Mostly vertical text, e.g. a sideways table.
  kVerticalText = 6,
  // Grid only: smaller than 2x2 after trimming empty outer rows and columns.
  kTooSmallAfterTrimming = 7,
  // Too few cells with text.
  kTooSparse = 8,
  // Shares text with another table that was kept, e.g. a nested grid.
  kOverlappingTable = 9,

  kMaxValue = kOverlappingTable,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PdfAccessibilityTableCandidateOutcome)

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_METRICS_H_
