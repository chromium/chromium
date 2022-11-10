// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter/at_most_once_enum_uma_deferrer.h"

#include "base/metrics/histogram_functions.h"

namespace internal {

// Helper functions for AtMostOnceEnumUmaDeferrer

base::HistogramBase* GetHistogramExactLinear(const char* name,
                                             int exclusive_max) {
  return base::LinearHistogram::FactoryGet(
      name, 1, exclusive_max, static_cast<size_t>(exclusive_max + 1),
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

}  // namespace internal
