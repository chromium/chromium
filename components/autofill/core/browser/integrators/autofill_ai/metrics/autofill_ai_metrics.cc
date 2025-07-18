// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill {

void LogOptInFunnelEvent(AutofillAiOptInFunnelEvents event) {
  base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Funnel", event);
  // TODO(crbug.com/408380915): Remove after M141.
  base::UmaHistogramEnumeration("Autofill.Ai.OptInFunnel", event);
}

}  // namespace autofill
