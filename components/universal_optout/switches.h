// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIVERSAL_OPTOUT_SWITCHES_H_
#define COMPONENTS_UNIVERSAL_OPTOUT_SWITCHES_H_

namespace universal_optout::switches {

// Specifies a path to a JSON file containing custom eligibility history to
// override `prefs::kUniversalOptOutEligibilityHistory` for testing.
// The JSON file must contain a dictionary with the following format:
// {
//   "universal_optout.eligibility_history": {
//     "YYYY-MM-DD": true,
//     "YYYY-MM-DD": false,
//     ...
//   }
// }
inline constexpr char kUniversalOptOutHistoryJsonPath[] =
    "universal-optout-history-json-path";

}  // namespace universal_optout::switches

#endif  // COMPONENTS_UNIVERSAL_OPTOUT_SWITCHES_H_
