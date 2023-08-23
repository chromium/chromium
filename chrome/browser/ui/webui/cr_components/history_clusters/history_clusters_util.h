// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_UTIL_H_

constexpr char kIsHistoryClustersVisibleKey[] = "isHistoryClustersVisible";
constexpr char kIsHistoryClustersVisibleManagedByPolicyKey[] =
    "isHistoryClustersVisibleManagedByPolicy";
constexpr char kRenameJourneysKey[] = "renameJourneys";

class Profile;

namespace content {
class WebUIDataSource;
}

class HistoryClustersUtil {
 public:
  static void PopulateSource(content::WebUIDataSource* source,
                             Profile* profile,
                             bool in_side_panel);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_CLUSTERS_HISTORY_CLUSTERS_UTIL_H_
