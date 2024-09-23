// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_SAVE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_SAVE_METRICS_H_

namespace autofill::autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillManuallyAddedAddressSurface {
  kContextMenuPrompt = 0,
  kSettings = 1,
  kMaxValue = kSettings,
};

void LogManuallyAddedAddress(AutofillManuallyAddedAddressSurface surface);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_ADDRESS_SAVE_METRICS_H_
