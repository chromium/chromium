// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_METRICS_ENUMS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_METRICS_ENUMS_H_

namespace autofill {

// Specified different events related to data list suggestions. Currently used
// for debugging purposes regarding the feature.
enum class AutofillDataListEvents {
  kDataListSuggestionsShown = 0,
  kDataListOptionsParsed = 1,
  kDataListSuggestionsUpdated = 2,
  kDataListSuggestionsInserted = 3,
  kMaxValue = kDataListSuggestionsInserted
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_METRICS_ENUMS_H_
