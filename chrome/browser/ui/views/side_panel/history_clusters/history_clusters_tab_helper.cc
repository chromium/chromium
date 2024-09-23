// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_tab_helper.h"

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_controller_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace side_panel {

HistoryClustersTabHelper::HistoryClustersTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<HistoryClustersTabHelper>(*web_contents),
      delegate_(side_panel::history_clusters::CreateDelegate(web_contents)) {}

HistoryClustersTabHelper::~HistoryClustersTabHelper() = default;

void HistoryClustersTabHelper::ShowJourneysSidePanel(const std::string& query) {
  DCHECK(delegate_);
  delegate_->ShowJourneysSidePanel(query);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryClustersTabHelper);

}  // namespace side_panel
