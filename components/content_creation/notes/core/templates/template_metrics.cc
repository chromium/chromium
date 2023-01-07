// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace content_creation {

void LogTemplateFetcherMetrics(bool success) {
  base::UmaHistogramBoolean("NoteCreation.TemplateFetchSuccess", success);
}

}  // namespace content_creation
