// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"

namespace tabs {

void RecordVerticalTabStripModeChanged(bool is_vertical,
                                       VerticalTabStripEntryPoint entry_point) {
  if (is_vertical) {
    base::UmaHistogramEnumeration("TabStrip.Vertical.EnteredMode", entry_point);
  } else {
    base::UmaHistogramEnumeration("TabStrip.Horizontal.EnteredMode",
                                  entry_point);
  }

  switch (entry_point) {
    case VerticalTabStripEntryPoint::kAppMenu:
      base::RecordAction(base::UserMetricsAction(
          is_vertical ? "SwitchToVerticalTabStrip_FromAppMenu"
                      : "SwitchToHorizontalTabStrip_FromAppMenu"));
      break;
    case VerticalTabStripEntryPoint::kSettings:
      base::RecordAction(base::UserMetricsAction(
          is_vertical ? "SwitchToVerticalTabStrip_FromSettings"
                      : "SwitchToHorizontalTabStrip_FromSettings"));
      break;
    case VerticalTabStripEntryPoint::kSystemContextMenu:
      base::RecordAction(base::UserMetricsAction(
          is_vertical ? "SwitchToVerticalTabStrip_FromSystemContextMenu"
                      : "SwitchToHorizontalTabStrip_FromSystemContextMenu"));
      break;
    case VerticalTabStripEntryPoint::kTabContextMenu:
      base::RecordAction(base::UserMetricsAction(
          is_vertical ? "SwitchToVerticalTabStrip_FromTabContextMenu"
                      : "SwitchToHorizontalTabStrip_FromTabContextMenu"));
      break;
    case VerticalTabStripEntryPoint::kMacViewMenu:
      base::RecordAction(base::UserMetricsAction(
          is_vertical ? "SwitchToVerticalTabStrip_FromMacMenu"
                      : "SwitchToHorizontalTabStrip_FromMacMenu"));
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace tabs
