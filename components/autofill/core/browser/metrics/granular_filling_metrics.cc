// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogDeleteAddressProfileDialogClosed(bool user_accepted_delete) {
  base::UmaHistogramBoolean("Autofill.ExtendedMenu.DeleteAddress",
                            user_accepted_delete);
}

}  // namespace autofill::autofill_metrics
