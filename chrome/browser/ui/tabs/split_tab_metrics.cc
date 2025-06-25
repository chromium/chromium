// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace split_tabs {
void RecordSplitTabCreated(SplitTabCreatedSource source) {
  base::UmaHistogramEnumeration("TabStrip.SplitView.Created", source);
}
}  // namespace split_tabs
