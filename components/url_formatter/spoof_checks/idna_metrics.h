// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDNA_METRICS_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDNA_METRICS_H_

// Deviation character to be recorded in IDNA 2008 transition metrics.
// See idn_spoof_checker.h for details. Corresponds to the list at
// https://www.unicode.org/reports/tr46/tr46-27.html#Table_Deviation_Characters
enum class IDNA2008DeviationCharacter {
  kNone,
  kEszett,
  kGreekFinalSigma,
  kZeroWidthJoiner,
  kZeroWidthNonJoiner,
};

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDNA_METRICS_H_
