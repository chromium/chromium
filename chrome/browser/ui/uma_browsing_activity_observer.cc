// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/uma_browsing_activity_observer.h"

#include "base/cxx17_backports.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/search_engines/template_url_service.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "ui/gfx/range/range.h"

namespace chrome {
namespace {

UMABrowsingActivityObserver* g_uma_browsing_activity_observer_instance =
    nullptr;

}  // namespace

// static
void UMABrowsingActivityObserver::Init() {
  DCHECK(!g_uma_browsing_activity_observer_instance);
  // Must be created before any Browsers are.
  DCHECK_EQ(0U, chrome::GetTotalBrowserCount());
  g_uma_browsing_activity_observer_instance = new UMABrowsingActivityObserver;
}

UMABrowsingActivityObserver::UMABrowsingActivityObserver() {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
  subscription_ = browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
      &UMABrowsingActivityObserver::OnAppTerminating, base::Unretained(this)));
}

UMABrowsingActivityObserver::~UMABrowsingActivityObserver() {}

void UMABrowsingActivityObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    const content::LoadCommittedDetails load =
        *content::Details<content::LoadCommittedDetails>(details).ptr();

    content::NavigationController* controller =
        content::Source<content::NavigationController>(source).ptr();
    // Track whether the page loaded is a search results page (SRP). Track
    // the non-SRP navigations as well so there is a control.
    base::RecordAction(base::UserMetricsAction("NavEntryCommitted"));

    CHECK(load.entry);
    // If the user is allowed to do searches in this profile (e.g., it's a
    // regular profile, not something like a "system" profile), then record if
    // this navigation appeared to go the default search engine.
    auto* turl_service = TemplateURLServiceFactory::GetForProfile(
        Profile::FromBrowserContext(controller->GetBrowserContext()));
    if (turl_service) {
      if (turl_service->IsSearchResultsPageFromDefaultSearchProvider(
              load.entry->GetURL())) {
        base::RecordAction(base::UserMetricsAction("NavEntryCommitted.SRP"));
      }
    }

    if (!load.is_navigation_to_different_page())
      return;  // Don't log for subframes or other trivial types.

    LogRenderProcessHostCount();
    LogBrowserTabCount();
  }
}

void UMABrowsingActivityObserver::OnAppTerminating() const {
  LogTimeBeforeUpdate();
  delete g_uma_browsing_activity_observer_instance;
  g_uma_browsing_activity_observer_instance = nullptr;
}

void UMABrowsingActivityObserver::LogTimeBeforeUpdate() const {
  const base::Time upgrade_detected_time =
      UpgradeDetector::GetInstance()->upgrade_detected_time();
  if (upgrade_detected_time.is_null())
    return;
  const base::TimeDelta time_since_upgrade =
      base::Time::Now() - upgrade_detected_time;
  constexpr int kMaxDays = 30;
  base::UmaHistogramExactLinear("UpgradeDetector.DaysBeforeUpgrade",
                                base::TimeDelta(time_since_upgrade).InDays(),
                                kMaxDays);
  base::UmaHistogramCounts1000("UpgradeDetector.HoursBeforeUpgrade",
                               base::TimeDelta(time_since_upgrade).InHours());
}

void UMABrowsingActivityObserver::LogRenderProcessHostCount() const {
  int hosts_count = 0;
  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    ++hosts_count;
  UMA_HISTOGRAM_CUSTOM_COUNTS("MPArch.RPHCountPerLoad", hosts_count, 1, 50, 50);
}

void UMABrowsingActivityObserver::LogBrowserTabCount() const {
  int tab_count = 0;
  int tab_group_count = 0;
  int collapsed_tab_group_count = 0;
  int customized_tab_group_count = 0;
  int app_window_count = 0;
  int popup_window_count = 0;
  int tabbed_window_count = 0;
  int pinned_tab_count = 0;
  std::map<base::StringPiece, int> unique_domain;

  for (auto* browser : *BrowserList::GetInstance()) {
    // Record how many tabs each window has open.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerWindow",
                                browser->tab_strip_model()->count(), 1, 200,
                                50);
    TabStripModel* const tab_strip_model = browser->tab_strip_model();
    tab_count += tab_strip_model->count();

    for (int i = 0; i < tab_strip_model->count(); ++i) {
      base::StringPiece domain = tab_strip_model->GetWebContentsAt(i)
                                     ->GetLastCommittedURL()
                                     .host_piece();
      unique_domain[domain]++;

      if (tab_strip_model->IsTabPinned(i))
        pinned_tab_count++;
    }

    if (tab_strip_model->group_model()) {
      const std::vector<tab_groups::TabGroupId>& groups =
          tab_strip_model->group_model()->ListTabGroups();
      tab_group_count += groups.size();
      for (const tab_groups::TabGroupId& group_id : groups) {
        const TabGroup* const tab_group =
            tab_strip_model->group_model()->GetTabGroup(group_id);
        if (tab_group->IsCustomized() ||
            !tab_group->visual_data()->title().empty()) {
          ++customized_tab_group_count;
        }
        if (tab_group->visual_data()->is_collapsed()) {
          ++collapsed_tab_group_count;
        }
      }
    }

    if (browser->window()->IsActive()) {
      // Record how many tabs the active window has open.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountActiveWindow",
                                  browser->tab_strip_model()->count(), 1, 200,
                                  50);
    }
    if (browser->is_type_app() || browser->is_type_app_popup() ||
        browser->is_type_devtools())
      app_window_count++;
    else if (browser->is_type_popup())
      popup_window_count++;
    else if (browser->is_type_normal())
      tabbed_window_count++;
  }

  // Record how many tabs share a domain based on the total number of tabs open.
  const std::string tab_count_per_domain_histogram_name =
      AppendTabBucketCountToHistogramName(tab_count);
  for (auto domain : unique_domain) {
    base::UmaHistogramSparse(tab_count_per_domain_histogram_name,
                             base::clamp(domain.second, 0, 200));
  }

  // Record how many tabs total are open (across all windows).
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);

  // Record how many tab groups (including zero) are open across all windows.
  UMA_HISTOGRAM_COUNTS_100("TabGroups.UserGroupCountPerLoad", tab_group_count);

  UMA_HISTOGRAM_COUNTS_100("TabGroups.UserPinnedTabCountPerLoad",
                           std::min(pinned_tab_count, 100));

  // Record how many tabs are in the current group. Records 0 if the active tab
  // is not in a group.
  const Browser* current_browser = BrowserList::GetInstance()->GetLastActive();
  if (current_browser) {
    TabStripModel* const tab_strip_model = current_browser->tab_strip_model();
    if (tab_strip_model->group_model()) {
      const absl::optional<tab_groups::TabGroupId> active_group =
          tab_strip_model->GetTabGroupForTab(tab_strip_model->active_index());
      UMA_HISTOGRAM_COUNTS_100("Tabs.TabCountInGroupPerLoad",
                               active_group.has_value()
                                   ? tab_strip_model->group_model()
                                         ->GetTabGroup(active_group.value())
                                         ->ListTabs()
                                         .length()
                                   : 0);
    }
  }

  // Record how many tab groups with a user-set name or color are open across
  // all windows.
  UMA_HISTOGRAM_COUNTS_100("TabGroups.UserCustomizedGroupCountPerLoad",
                           customized_tab_group_count);

  // Record how many tab groups are collapsed across all windows.
  UMA_HISTOGRAM_COUNTS_100("TabGroups.CollapsedGroupCountPerLoad",
                           collapsed_tab_group_count);

  // Record how many windows are open, by type.
  UMA_HISTOGRAM_COUNTS_100("WindowManager.AppWindowCountPerLoad",
                           app_window_count);
  UMA_HISTOGRAM_COUNTS_100("WindowManager.PopUpWindowCountPerLoad",
                           popup_window_count);
  UMA_HISTOGRAM_COUNTS_100("WindowManager.TabbedWindowCountPerLoad",
                           tabbed_window_count);
}

std::string UMABrowsingActivityObserver::AppendTabBucketCountToHistogramName(
    int total_tab_count) const {
  const char* bucket = nullptr;
  if (total_tab_count < 6) {
    bucket = "0to5";
  } else if (total_tab_count < 11) {
    bucket = "6to10";
  } else if (total_tab_count < 16) {
    bucket = "10to15";
  } else if (total_tab_count < 21) {
    bucket = "16to20";
  } else if (total_tab_count < 31) {
    bucket = "21to30";
  } else if (total_tab_count < 41) {
    bucket = "31to40";
  } else if (total_tab_count < 61) {
    bucket = "41to60";
  } else if (total_tab_count < 81) {
    bucket = "61to80";
  } else if (total_tab_count < 101) {
    bucket = "81to100";
  } else if (total_tab_count < 151) {
    bucket = "101to150";
  } else if (total_tab_count < 201) {
    bucket = "151to200";
  } else if (total_tab_count < 301) {
    bucket = "201to300";
  } else if (total_tab_count < 401) {
    bucket = "301to400";
  } else if (total_tab_count < 501) {
    bucket = "401to500";
  } else {
    bucket = "501+";
  }
  const char kHistogramBaseName[] = "Tabs.TabCountPerDomainPerLoad";
  return base::StringPrintf("%s.%s", kHistogramBaseName, bucket);
}

}  // namespace chrome
