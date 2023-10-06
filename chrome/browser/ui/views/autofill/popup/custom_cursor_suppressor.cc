// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/custom_cursor_suppressor.h"

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"

CustomCursorSuppressor::CustomCursorSuppressor() = default;

CustomCursorSuppressor::~CustomCursorSuppressor() = default;

void CustomCursorSuppressor::Start(int max_dimension_dips) {
  max_dimension_dips_ = max_dimension_dips;

  // Observe the list of browsers.
  browser_list_observation_.Observe(BrowserList::GetInstance());
  // Observe all TabStripModels of existing browsers and suppress custom cursors
  // on their active tabs.
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser->tab_strip_model()->AddObserver(this);
    if (content::WebContents* active_contents =
            browser->tab_strip_model()->GetActiveWebContents()) {
      SuppressForWebContents(*active_contents);
    }
  }
}

void CustomCursorSuppressor::Stop() {
  disallow_custom_cursor_scopes_.clear();
  TabStripModelObserver::StopObservingAll(this);
  browser_list_observation_.Reset();
}

bool CustomCursorSuppressor::IsSuppressing() const {
  return browser_list_observation_.IsObserving();
}

std::vector<content::GlobalRenderFrameHostId>
CustomCursorSuppressor::SuppressedRenderFrameHostIdsForTesting() const {
  std::vector<content::GlobalRenderFrameHostId> rfh_ids;
  for (const auto& [rfh_id, scope] : disallow_custom_cursor_scopes_) {
    rfh_ids.push_back(rfh_id);
  }
  return rfh_ids;
}

void CustomCursorSuppressor::SuppressForWebContents(
    content::WebContents& web_contents) {
  content::GlobalRenderFrameHostId main_frame_id =
      web_contents.GetPrimaryMainFrame()->GetGlobalId();
  if (disallow_custom_cursor_scopes_.contains(main_frame_id)) {
    return;
  }
  disallow_custom_cursor_scopes_.insert(
      {main_frame_id,
       web_contents.CreateDisallowCustomCursorScope(max_dimension_dips_)});
  // TODO(crbug.com/1478613): Start observing the WebContents at this point to
  // guard against navigations in it that cause a change in the RFH of the
  // primary main frame.
}

void CustomCursorSuppressor::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void CustomCursorSuppressor::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && selection.new_contents) {
    SuppressForWebContents(*selection.new_contents);
  }
}
