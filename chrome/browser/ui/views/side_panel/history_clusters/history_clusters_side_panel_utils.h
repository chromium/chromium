// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UTILS_H_

class Profile;

namespace side_panel::history_clusters {

bool IsHistoryClustersSidePanelSupportedForProfile(Profile* profile);

}  // namespace side_panel::history_clusters

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UTILS_H_
