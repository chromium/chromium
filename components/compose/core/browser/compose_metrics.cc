// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace compose {

const char kComposeResponseDurationOk[] = "Compose.Response.Duration.Ok";
const char kComposeResponseDurationError[] = "Compose.Response.Duration.Error";
const char kComposeResponseStatus[] = "Compose.Response.Status";

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Compose.ContextMenu.CTR", event);
}

void LogComposeContextMenuShowStatus(ComposeShowStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Compose.ContextMenu.ShowStatus", status);
}

void LogComposeRequestDuration(base::TimeDelta duration, bool is_valid) {
  base::UmaHistogramMediumTimes(
      is_valid ? kComposeResponseDurationOk : kComposeResponseDurationError,
      duration);
}
}  // namespace compose
