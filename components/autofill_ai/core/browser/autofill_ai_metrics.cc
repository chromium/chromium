// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill_ai {

namespace {
constexpr char kOptinMetricsPrefix[] = "Autofill.Ai.OptInFunnel";
}

// static
void LogOptInFunnelEvent(AutofillAiOptInFunnelEvents event) {
  base::UmaHistogramEnumeration(kOptinMetricsPrefix, event);
}

}  // namespace autofill_ai
