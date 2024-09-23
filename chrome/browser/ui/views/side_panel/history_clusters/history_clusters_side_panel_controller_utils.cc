// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_controller_utils.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_controller.h"

namespace side_panel::history_clusters {

std::unique_ptr<HistoryClustersTabHelper::Delegate> CreateDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<HistoryClustersSidePanelController>(web_contents);
}

}  // namespace side_panel::history_clusters
