// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_ELIGIBILITY_READER_MODE_ELIGIBILITY_H_
#define COMPONENTS_WEB_DISTILLER_CORE_ELIGIBILITY_READER_MODE_ELIGIBILITY_H_

namespace web_distiller {

// The outcome of the Reading Mode eligibility heuristic. Recorded in UMA as
// the ReaderModeHeuristicResult enum. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ReaderModeEligibility)
enum class ReaderModeEligibility {
  kMalformedResponse = 0,
  kReaderModeEligible = 1,
  kReaderModeNotEligibleContentOnly = 2,
  kReaderModeNotEligibleContentLength = 3,
  kReaderModeNotEligibleContentAndLength = 4,
  kReaderModeNotEligibleOptimizationGuideIneligible = 5,
  kReaderModeNotEligibleOptimizationGuideUnknown = 6,
  kMaxValue = kReaderModeNotEligibleOptimizationGuideUnknown,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeHeuristicResult)

}  // namespace web_distiller

#endif  // COMPONENTS_WEB_DISTILLER_CORE_ELIGIBILITY_READER_MODE_ELIGIBILITY_H_
