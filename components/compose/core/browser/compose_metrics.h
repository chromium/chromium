// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_

namespace compose {
// Enum for calculating the CTR of the Compose context menu item.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// ComposeContextMenuCtrEvent in src/tools/metrics/histograms/enums.xml.
enum class ComposeContextMenuCtrEvent {
  kMenuItemDisplayed = 0,
  kComposeOpened = 1,
  kMaxValue = kComposeOpened,
};

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event);
// TODO(b/308197248): add more compose metrics.
}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_METRICS_H_
