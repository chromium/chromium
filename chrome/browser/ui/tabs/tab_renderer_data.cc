// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_renderer_data.h"

#include "base/process/kill.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "components/performance_manager/public/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
TabRendererData TabRendererData::FromTabInModel(const TabStripModel* model,
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
  data.favicon = tab_ui_helper->GetFavicon();

  // Tabbed web apps should use the app icon on the home tab.
  Browser* app_browser = chrome::FindBrowserWithTab(contents);

  if (app_browser && app_browser->app_controller() &&
      app_browser->app_controller()->ShouldShowAppIconOnTab(index)) {
    web_app::WebAppBrowserController* app_controller =
        app_browser->app_controller()->AsWebAppBrowserController();
    if (app_controller) {
      gfx::ImageSkia home_tab_icon = app_controller->GetHomeTabIcon();
      if (!home_tab_icon.isNull()) {
        data.is_monochrome_favicon = true;
        data.favicon = ui::ImageModel::FromImageSkia(home_tab_icon);
      } else {
        home_tab_icon = app_controller->GetFallbackHomeTabIcon();
        if (!home_tab_icon.isNull()) {
          data.favicon = ui::ImageModel::FromImageSkia(home_tab_icon);
        }
      }
    }
  }

  ThumbnailTabHelper* const thumbnail_tab_helper =
      ThumbnailTabHelper::FromWebContents(contents);
  if (thumbnail_tab_helper) {
    data.thumbnail = thumbnail_tab_helper->thumbnail();
  }
  data.is_tab_discarded = contents->WasDiscarded();
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
  data.alert_state = GetTabAlertStatesForContents(contents);

  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  data.should_themify_favicon =
      entry && favicon::ShouldThemifyFaviconForEntry(entry);

  std::optional<mojom::LifecycleUnitDiscardReason> discard_reason =
      memory_saver::GetDiscardReason(contents);

  // Only show discard status for tabs that were proactively discarded or
  // suggested by the PerformanceDetectionManager to prevent confusion to users
  // on why a tab was discarded. Also, the favicon discard animation may use
  // resources so the animation should be limited to prevent performance issues.
  data.should_show_discard_status =
      memory_saver::IsURLSupported(contents->GetURL()) &&
      contents->WasDiscarded() && discard_reason.has_value() &&
      (discard_reason.value() == mojom::LifecycleUnitDiscardReason::PROACTIVE ||
       discard_reason.value() == mojom::LifecycleUnitDiscardReason::SUGGESTED);

  if (contents->WasDiscarded()) {
    data.discarded_memory_savings_in_bytes =
        memory_saver::GetDiscardedMemorySavingsInBytes(contents);
  }

  const auto* const resource_tab_helper =
      TabResourceUsageTabHelper::FromWebContents(contents);
  if (resource_tab_helper) {
    data.tab_resource_usage = resource_tab_helper->resource_usage();
  }

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
  return favicon == other.favicon && thumbnail == other.thumbnail &&
         network_state == other.network_state && title == other.title &&
         visible_url == other.visible_url &&
         last_committed_url == other.last_committed_url &&
         should_display_url == other.should_display_url &&
         crashed_status == other.crashed_status &&
         incognito == other.incognito && show_icon == other.show_icon &&
         pinned == other.pinned && blocked == other.blocked &&
         alert_state == other.alert_state &&
         should_hide_throbber == other.should_hide_throbber &&
         is_tab_discarded == other.is_tab_discarded &&
         should_show_discard_status == other.should_show_discard_status &&
         discarded_memory_savings_in_bytes ==
             other.discarded_memory_savings_in_bytes &&
         tab_resource_usage == other.tab_resource_usage;
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
