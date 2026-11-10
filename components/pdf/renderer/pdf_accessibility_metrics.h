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

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_METRICS_H_
