// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_renderer_data.h"

#include "base/process/kill.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "content/public/browser/web_contents.h"

// static
TabRendererData TabRendererData::FromTabInModel(TabStripModel* model,
                                                int index) {
  content::WebContents* const contents = model->GetWebContentsAt(index);

  TabRendererData data;
  TabUIHelper* const tab_ui_helper = TabUIHelper::FromWebContents(contents);
  data.favicon = tab_ui_helper->GetFavicon().AsImageSkia();
  ThumbnailTabHelper* const thumbnail_tab_helper =
      ThumbnailTabHelper::FromWebContents(contents);
  if (thumbnail_tab_helper)
    data.thumbnail = thumbnail_tab_helper->thumbnail();
  data.network_state = TabNetworkStateForWebContents(contents);
  data.title = tab_ui_helper->GetTitle();
  data.visible_url = contents->GetVisibleURL();
  data.last_committed_url = contents->GetLastCommittedURL();
  data.crashed_status = contents->GetCrashedStatus();
  data.incognito = contents->GetBrowserContext()->IsOffTheRecord();
  data.pinned = model->IsTabPinned(index);
  data.show_icon = data.pinned || favicon::ShouldDisplayFavicon(contents);
  data.blocked = model->IsTabBlocked(index);
  data.should_hide_throbber = tab_ui_helper->ShouldHideThrobber();

  // TODO(crbug.com/1004983): provide all current alerts in
  // TabRendererData. Consumers that can only display 1 should handle
  // this themselves.
  auto alerts = chrome::GetTabAlertStatesForContents(contents);
  data.alert_state =
      alerts.empty() ? base::Optional<TabAlertState>() : alerts[0];

  return data;
}

TabRendererData::TabRendererData() = default;
TabRendererData::TabRendererData(const TabRendererData& other) = default;
TabRendererData::TabRendererData(TabRendererData&& other) = default;

TabRendererData& TabRendererData::operator=(const TabRendererData& other) =
    default;
TabRendererData& TabRendererData::operator=(TabRendererData&& other) = default;

TabRendererData::~TabRendererData() = default;

bool TabRendererData::operator==(const TabRendererData& other) const {
  return favicon.BackedBySameObjectAs(other.favicon) &&
         thumbnail == other.thumbnail && network_state == other.network_state &&
         title == other.title && visible_url == other.visible_url &&
         last_committed_url == other.last_committed_url &&
         crashed_status == other.crashed_status &&
         incognito == other.incognito && show_icon == other.show_icon &&
         pinned == other.pinned && blocked == other.blocked &&
         alert_state == other.alert_state &&
         should_hide_throbber == other.should_hide_throbber;
}

bool TabRendererData::IsCrashed() const {
  return (crashed_status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
#if defined(OS_CHROMEOS)
          crashed_status ==
              base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM ||
#endif
          crashed_status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
          crashed_status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          crashed_status == base::TERMINATION_STATUS_LAUNCH_FAILED);
}
