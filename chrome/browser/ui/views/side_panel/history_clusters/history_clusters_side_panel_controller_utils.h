// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_CONTROLLER_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_CONTROLLER_UTILS_H_

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_tab_helper.h"

namespace side_panel::history_clusters {

std::unique_ptr<HistoryClustersTabHelper::Delegate> CreateDelegate(
    content::WebContents* web_contents);

}  // namespace side_panel::history_clusters

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_CONTROLLER_UTILS_H_
