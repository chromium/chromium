// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DATA_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DATA_H_

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

class TabData {
 public:
  // TODO(1476012) replace with opaque tab handle
  using TabID = int;

  TabData(TabID id, TabStripModel* model, GURL url, absl::optional<int> index);
  TabData(TabStripModel* model, content::WebContents* web_contents);
  TabData(const TabData&) = default;
  TabData& operator=(const TabData& other) = default;
  const TabID& tab_id() const { return tab_id_; }
  const TabStripModel* original_tab_strip_model() const {
    return original_tab_strip_model_;
  }
  const GURL& original_url() const { return original_url_; }
  const absl::optional<int> original_index() const { return original_index_; }

 private:
  TabID tab_id_;
  raw_ptr<TabStripModel> original_tab_strip_model_;
  GURL original_url_;
  absl::optional<int> original_index_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_METRICS_H_
