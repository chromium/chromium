// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/uma_browsing_activity_observer.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
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
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
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
  subscription_ = browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
      &UMABrowsingActivityObserver::OnAppTerminating, base::Unretained(this)));
}

UMABrowsingActivityObserver::~UMABrowsingActivityObserver() = default;

void UMABrowsingActivityObserver::OnNavigationEntryCommitted(
    content::WebContents* web_contents,
    const content::LoadCommittedDetails& load_details) const {
  // Track whether the page loaded is a search results page (SRP). Track
  // the non-SRP navigations as well so there is a control.
  base::RecordAction(base::UserMetricsAction("NavEntryCommitted"));

  if (!load_details.is_navigation_to_different_page()) {
    // Don't log for subframes or other trivial types.
    return;
  }

  LogBrowserTabCount();
}

void UMABrowsingActivityObserver::OnAppTerminating() const {
  LogTimeBeforeUpdate();

  DCHECK_EQ(this, g_uma_browsing_activity_observer_instance);
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

void UMABrowsingActivityObserver::LogBrowserTabCount() const {
  int tab_count = 0;
  int tab_group_count = 0;
  int collapsed_tab_group_count = 0;
  int customized_tab_group_count = 0;
  int pinned_tab_count = 0;

  for (Browser* browser : *BrowserList::GetInstance()) {
    // Record how many tabs each window has open.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerWindow",
                                browser->tab_strip_model()->count(), 1, 200,
                                50);
    TabStripModel* const tab_strip_model = browser->tab_strip_model();
    tab_count += tab_strip_model->count();

    for (int i = 0; i < tab_strip_model->count(); ++i) {
      if (tab_strip_model->IsTabPinned(i)) {
        pinned_tab_count++;
      }
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
      const std::optional<tab_groups::TabGroupId> active_group =
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
}

UMABrowsingActivityObserver::TabHelper::TabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TabHelper>(*web_contents) {}

UMABrowsingActivityObserver::TabHelper::~TabHelper() = default;

void UMABrowsingActivityObserver::TabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // This is null in unit tests. Crash reports suggest it's possible for it to
  // be null in production. See https://crbug.com/1510023 and
  // https://crbug.com/1523758
  if (!g_uma_browsing_activity_observer_instance) {
    return;
  }

  g_uma_browsing_activity_observer_instance->OnNavigationEntryCommitted(
      web_contents(), load_details);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UMABrowsingActivityObserver::TabHelper);

}  // namespace chrome
