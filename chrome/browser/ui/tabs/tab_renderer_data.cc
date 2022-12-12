// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_renderer_data.h"

#include "base/process/kill.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
TabRendererData TabRendererData::FromTabInModel(TabStripModel* model,
                                                int index) {
  content::WebContents* const contents = model->GetWebContentsAt(index);
  // If the tab is showing a lookalike interstitial ("Did you mean example.com"
  // on éxample.com), don't show the URL in the hover card because it's
  // misleading.
  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper = security_interstitials::
          SecurityInterstitialTabHelper::FromWebContents(contents);
  bool should_display_url =
      !security_interstitial_tab_helper ||
      !security_interstitial_tab_helper->IsDisplayingInterstitial() ||
      security_interstitial_tab_helper->ShouldDisplayURL();
  TabRendererData data;
  TabUIHelper* const tab_ui_helper = TabUIHelper::FromWebContents(contents);
  data.favicon = tab_ui_helper->GetFavicon().AsImageSkia();
  ThumbnailTabHelper* const thumbnail_tab_helper =
      ThumbnailTabHelper::FromWebContents(contents);
  if (thumbnail_tab_helper) {
    data.thumbnail = thumbnail_tab_helper->thumbnail();
    data.is_tab_discarded = thumbnail_tab_helper->is_tab_discarded();
  }
  data.network_state = TabNetworkStateForWebContents(contents);
  data.title = tab_ui_helper->GetTitle();
  data.visible_url = contents->GetVisibleURL();
  // Allow empty title for chrome-untrusted:// URLs.
  if (data.title.empty() &&
      data.visible_url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    data.should_render_empty_title = true;
  }
  data.last_committed_url = contents->GetLastCommittedURL();
  data.should_display_url = should_display_url;
  data.crashed_status = contents->GetCrashedStatus();
  data.incognito = contents->GetBrowserContext()->IsOffTheRecord();
  data.pinned = model->IsTabPinned(index);
  data.show_icon =
      data.pinned || model->delegate()->ShouldDisplayFavicon(contents);
  data.blocked = model->IsTabBlocked(index);
  data.should_hide_throbber = tab_ui_helper->ShouldHideThrobber();
  data.alert_state = chrome::GetTabAlertStatesForContents(contents);

  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  data.should_themify_favicon =
      entry && favicon::ShouldThemifyFaviconForEntry(entry);

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
         should_display_url == other.should_display_url &&
         crashed_status == other.crashed_status &&
         incognito == other.incognito && show_icon == other.show_icon &&
         pinned == other.pinned && blocked == other.blocked &&
         alert_state == other.alert_state &&
         should_hide_throbber == other.should_hide_throbber;
}

bool TabRendererData::IsCrashed() const {
  return (crashed_status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
#if BUILDFLAG(IS_CHROMEOS)
          crashed_status ==
              base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM ||
#endif
          crashed_status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
          crashed_status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          crashed_status == base::TERMINATION_STATUS_LAUNCH_FAILED);
}
