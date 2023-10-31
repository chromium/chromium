// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace compose {
void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Compose.ContextMenu.CTR", event);
}
}  // namespace compose
