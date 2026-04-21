// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_api/aggregation/tab_strip_service_aggregator.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/browser_tab_strip_service_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/tutorial/tutorial_identifier.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace {
constexpr base::TimeDelta kTabsChangeDelay = base::Milliseconds(50);

std::string GetLastActiveElapsedText(
    const base::TimeTicks& last_active_time_ticks) {
  const base::TimeDelta elapsed =
      base::TimeTicks::Now() - last_active_time_ticks;
  return base::UTF16ToUTF8(ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT, elapsed));
}

std::string GetLastActiveElapsedText(const base::Time& last_active_time) {
  const base::TimeDelta elapsed = base::Time::Now() - last_active_time;
  return base::UTF16ToUTF8(ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT, elapsed));
}

// If Tab Group has no timestamp, we find the tab in the tab group with
// the most recent navigation last active time.
base::Time GetTabGroupTimeStamp(
    const std::vector<std::unique_ptr<sessions::tab_restore::Tab>>& tabs) {
  base::Time last_active_time;
  for (const auto& tab : tabs) {
    const sessions::SerializedNavigationEntry& entry =
        tab->navigations[tab->current_navigation_index];
    if (entry.timestamp() > last_active_time) {
      last_active_time = entry.timestamp();
    }
  }
  return last_active_time;
}

// If a recently closed tab is associated to a group that is no longer
// open we create a TabGroup entry with the required fields to support
// rendering the tab's associated group information in the UI.
void CreateTabGroupIfNotPresent(
    sessions::tab_restore::Tab* tab,
    std::set<tab_groups::TabGroupId>& tab_group_ids,
    std::vector<tab_search::mojom::TabGroupPtr>& tab_groups) {
  if (tab->group.has_value() && !tab_group_ids.contains(tab->group.value())) {
    tab_groups::TabGroupId tab_group_id = tab->group.value();
    const tab_groups::TabGroupVisualData* tab_group_visual_data =
        &tab->group_visual_data.value();
    auto tab_group = tab_search::mojom::TabGroup::New();
    tab_group->id = tab_group_id.token();
    tab_group->color = tab_group_visual_data->color();
    tab_group->title = base::UTF16ToUTF8(tab_group_visual_data->title());

    tab_group_ids.insert(tab_group_id);
    tab_groups.push_back(std::move(tab_group));
  }
}

// Applies theming to favicons where necessary. This is needed to handle favicon
// resources that are rasterized in a theme-unaware way. This is common of
// favicons not sourced directly from the browser.
gfx::ImageSkia ThemeFavicon(const gfx::ImageSkia& source,
                            const ui::ColorProvider& provider) {
  return favicon::ThemeFavicon(
      source, provider.GetColor(kColorTabSearchPrimaryForeground),
      provider.GetColor(kColorTabSearchBackground),
      provider.GetColor(kColorTabSearchBackground));
}

// Returns true if the browser window should be tracked by Tab Search.
bool ShouldTrackBrowser(Profile* profile, BrowserWindowInterface* browser) {
  return browser->GetProfile() == profile &&
         browser->GetType() == BrowserWindowInterface::TYPE_NORMAL;
}

// Returns true if the tab change event contains updates to tab properties
// (e.g. title, URL, favicon, etc.) that are displayed in the Tab Search UI.
// This is used to filter out tab state changes that can be noisy and not affect
// the UI such as activation or selection state.
bool HasTabSiteDataChanged(const tabs_api::mojom::TabFieldMaskPtr& mask) {
  return mask->title || mask->url || mask->favicon || mask->alert_states ||
         mask->last_active;
}

}  // namespace

TabSearchPageHandler::TabSearchPageHandler(
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver,
    mojo::PendingRemote<tab_search::mojom::Page> page,
    content::WebUI* web_ui,
    TopChromeWebUIController* webui_controller,
    MetricsReporter* metrics_reporter)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_(web_ui),
      profile_(Profile::FromWebUI(web_ui_)),
      webui_controller_(webui_controller),
      metrics_reporter_(metrics_reporter),
      debounce_timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          kTabsChangeDelay,
          base::BindRepeating(&TabSearchPageHandler::NotifyTabsChanged,
                              base::Unretained(this)))),
      browser_window_changed_subscription_(
          webui::RegisterBrowserWindowInterfaceChanged(
              web_ui->GetWebContents(),
              base::BindRepeating(
                  &TabSearchPageHandler::BrowserWindowInterfaceChanged,
                  base::Unretained(this)))),
      aggregator_(std::make_unique<tabs_api::TabStripServiceAggregator>(
          std::make_unique<tabs_api::BrowserTabStripServiceTracker>(
              profile_,
              base::BindRepeating(&ShouldTrackBrowser, profile_)),
          base::BindRepeating(&TabSearchPageHandler::OnTabEvents,
                              base::Unretained(this)))) {
  BrowserWindowInterfaceChanged();
}

TabSearchPageHandler::~TabSearchPageHandler() {
  base::UmaHistogramCounts1000("Tabs.TabSearch.NumTabsClosedPerInstance",
                               num_tabs_closed_);
  base::UmaHistogramEnumeration("Tabs.TabSearch.CloseAction",
                                called_switch_to_tab_
                                    ? TabSearchCloseAction::kTabSwitch
                                    : TabSearchCloseAction::kNoAction);
  pref_change_registrar_.Reset();
}

void TabSearchPageHandler::CloseTab(int32_t tab_id) {
  tabs::TabInterface* const tab = GetTabInterface(tab_id);
  if (!tab) {
    return;
  }

  ++num_tabs_closed_;

  profile_->GetPrefs()->SetBoolean(tab_search_prefs::kTabSearchUsed, true);

  // CloseTab() can target the WebContents hosting Tab Search if the Tab Search
  // WebUI is open in a chrome browser tab rather than its bubble. In this case
  // CloseWebContentsAt() closes the WebContents hosting this
  // TabSearchPageHandler object, causing it to be immediately destroyed. Ensure
  // that no further actions are performed following the call to
  // CloseWebContentsAt(). See (https://crbug.com/40054717).
  tabs_api::TabStripService* const service =
      GetTabStripService(tab->GetBrowserWindowInterface());
  CHECK(service);
  auto node_id = tabs_api::NodeId::FromTabHandle(tab->GetHandle());
  const auto result = service->CloseNodes({node_id});
  DCHECK(result.has_value());
  // Do not add code past this point.
}

void TabSearchPageHandler::CloseWebUiTab() {
  tabs::TabInterface* const tab =
      tabs::TabInterface::GetFromContents(web_ui_->GetWebContents());
  if (tab) {
    CloseTab(tab->GetHandle().raw_value());
  }
  // Do not add code past this point.
}

// Tab Search UI can also hosted inside a tab and so we still need to
// be able to handle browser window changes.
void TabSearchPageHandler::BrowserWindowInterfaceChanged() {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_ui_->GetWebContents());
  browser_ = browser_window_interface
                 ? browser_window_interface->GetBrowserForMigrationOnly()
                 : nullptr;
  page_->HostWindowChanged();
}

void TabSearchPageHandler::GetProfileData(GetProfileDataCallback callback) {
  TRACE_EVENT0("browser", "TabSearchPageHandler:GetProfileTabs");
  auto profile_tabs = CreateProfileData();
  // On first run record the number of windows and tabs open for the given
  // profile.
  if (!sent_initial_payload_) {
    sent_initial_payload_ = true;
    int tab_count = 0;
    for (const auto& window : profile_tabs->windows) {
      tab_count += window->tabs.size();
    }
    base::UmaHistogramCounts100("Tabs.TabSearch.NumWindowsOnOpen",
                                profile_tabs->windows.size());
    base::UmaHistogramCounts10000("Tabs.TabSearch.NumTabsOnOpen", tab_count);

    bool expand_preference = profile_->GetPrefs()->GetBoolean(
        tab_search_prefs::kTabSearchRecentlyClosedSectionExpanded);
    base::UmaHistogramEnumeration(
        "Tabs.TabSearch.RecentlyClosedSectionToggleStateOnOpen",
        expand_preference ? TabSearchRecentlyClosedToggleAction::kExpand
                          : TabSearchRecentlyClosedToggleAction::kCollapse);
  }

  std::move(callback).Run(std::move(profile_tabs));
}

tabs::TabInterface* TabSearchPageHandler::GetTabInterface(int32_t tab_id) {
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    return nullptr;
  }
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser || !ShouldTrackBrowser(profile_, browser)) {
    return nullptr;
  }
  return tab;
}

void TabSearchPageHandler::GetIsSplit(GetIsSplitCallback callback) {
  bool is_split = false;
  GURL url = web_ui_->GetWebContents()->GetURL();
  if (url.spec() == chrome::kChromeUISplitViewNewTabPageURL) {
    is_split = tabs::TabInterface::GetFromContents(web_ui_->GetWebContents())
                   ->IsSplit();
  }
  std::move(callback).Run(is_split);
}

void TabSearchPageHandler::SwitchToTab(
    tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) {
  tabs::TabInterface* const tab = GetTabInterface(switch_to_tab_info->tab_id);
  if (!tab) {
    return;
  }

  called_switch_to_tab_ = true;

  profile_->GetPrefs()->SetBoolean(tab_search_prefs::kTabSearchUsed, true);

  tabs_api::TabStripService* const service =
      GetTabStripService(tab->GetBrowserWindowInterface());
  const auto result =
      service->ActivateTab(tabs_api::NodeId::FromTabHandle(tab->GetHandle()));
  DCHECK(result.has_value());

  // Tab search shows tabs from other windows in the profile. So if a user
  // selects a tab in another window, we need to manually activate it so
  // that we can bring that window to the foreground.
  tab->GetBrowserWindowInterface()->GetWindow()->Activate();
  metrics_reporter_->Measure(
      "SwitchToTab",
      base::BindOnce(
          [](MetricsReporter* metrics_reporter, base::TimeDelta duration) {
            base::UmaHistogramTimes("Tabs.TabSearch.Mojo.SwitchToTab",
                                    duration);
            metrics_reporter->ClearMark("SwitchToTab");
          },
          metrics_reporter_));
}

void TabSearchPageHandler::OpenRecentlyClosedEntry(int32_t session_id) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  if (!tab_restore_service) {
    return;
  }

  profile_->GetPrefs()->SetBoolean(tab_search_prefs::kTabSearchUsed, true);

  tab_restore_service->RestoreEntryById(
      BrowserLiveTabContext::FindContextForWebContents(
          browser_->GetActiveTabInterface()->GetContents()),
      SessionID::FromSerializedValue(session_id),
      WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void TabSearchPageHandler::ReplaceActiveSplitTab(int32_t replacement_tab_id) {
  tabs::TabInterface* const active_tab = browser_->GetActiveTabInterface();
  if (active_tab->GetSplit().has_value()) {
    const auto result = GetTabStripService(browser_)->ReplaceTabInSplit(
        tabs_api::NodeId::FromTabHandle(active_tab->GetHandle()),
        tabs_api::NodeId::FromTabHandle(tabs::TabHandle(replacement_tab_id)));
    DCHECK(result.has_value());
  }
}

void TabSearchPageHandler::SaveRecentlyClosedExpandedPref(bool expanded) {
  profile_->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabSearchRecentlyClosedSectionExpanded, expanded);

  base::UmaHistogramEnumeration(
      "Tabs.TabSearch.RecentlyClosedSectionToggleAction",
      expanded ? TabSearchRecentlyClosedToggleAction::kExpand
               : TabSearchRecentlyClosedToggleAction::kCollapse);
}

void TabSearchPageHandler::StartTabGroupTutorial() {
  // Close the tab search bubble if showing.
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }

  auto* const user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(browser_->profile());
  user_education::TutorialService* const tutorial_service =
      user_education_service ? &user_education_service->tutorial_service()
                             : nullptr;
  CHECK(tutorial_service);

  const ui::ElementContext context =
      BrowserElements::From(browser_)->GetContext();
  CHECK(context);

  user_education::TutorialIdentifier tutorial_id = kTabGroupTutorialId;
  tutorial_service->StartTutorial(tutorial_id, context);
}

void TabSearchPageHandler::MaybeShowUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

tab_search::mojom::ProfileDataPtr TabSearchPageHandler::CreateProfileData() {
  auto profile_data = tab_search::mojom::ProfileData::New();

  std::set<DedupKey> tab_dedup_keys;
  std::set<tab_groups::TabGroupId> tab_group_ids;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, &profile_data, &tab_dedup_keys,
       &tab_group_ids](BrowserWindowInterface* browser) {
        if (!ShouldTrackBrowser(profile_, browser)) {
          return true;
        }

        auto* service = GetTabStripService(browser);
        CHECK(service);
        auto get_tabs_result = service->GetTabsWithoutObservation();
        if (!get_tabs_result.has_value()) {
          VLOG(1) << "Failed to get tabs";
          return true;
        }

        auto window = tab_search::mojom::Window::New();
        window->active = browser->IsActive();
        window->is_host_window = browser == browser_;
        window->height = browser->GetBrowserForMigrationOnly()
                             ->window()
                             ->GetContentsSize()
                             .height();

        WalkContainer(get_tabs_result.value(), window.get(), profile_data.get(),
                      tab_dedup_keys, tab_group_ids);

        profile_data->windows.push_back(std::move(window));

        return true;
      });

  AddRecentlyClosedEntries(profile_data->recently_closed_tabs,
                           profile_data->recently_closed_tab_groups,
                           tab_group_ids, profile_data->tab_groups,
                           tab_dedup_keys);
  profile_data->recently_closed_section_expanded =
      profile_->GetPrefs()->GetBoolean(
          tab_search_prefs::kTabSearchRecentlyClosedSectionExpanded);
  return profile_data;
}

void TabSearchPageHandler::WalkContainer(
    const tabs_api::mojom::ContainerPtr& container,
    tab_search::mojom::Window* window,
    tab_search::mojom::ProfileData* profile_data,
    std::set<DedupKey>& tab_dedup_keys,
    std::set<tab_groups::TabGroupId>& tab_group_ids) {
  if (container->data->is_tab()) {
    const std::optional<tabs::TabHandle> handle =
        container->data->get_tab()->id.ToTabHandle();
    if (handle) {
      tabs::TabInterface* const tab_interface = handle->Get();
      // A Tab can potentially be in a state where it has no committed
      // entries during loading and thus has no title/URL. Skip any such
      // pending tabs. These tabs will be added to the list later on once
      // loading has finished.
      if (tab_interface && tab_interface->GetContents()
                               ->GetController()
                               .GetLastCommittedEntry()) {
        tab_search::mojom::TabPtr tab = GetTab(tab_interface);
        tab_dedup_keys.insert(DedupKey(tab->url, tab->group_id));
        window->tabs.push_back(std::move(tab));
      }
    }
  } else if (container->data->is_tab_group()) {
    const auto& group_data = container->data->get_tab_group();
    auto tab_group = tab_search::mojom::TabGroup::New();
    tab_group->title = base::UTF16ToUTF8(group_data->data.title());
    tab_group->color = group_data->data.color();

    std::optional<tabs::TabCollectionHandle> collection_handle =
        group_data->id.ToTabCollectionHandle();
    if (collection_handle.has_value()) {
      const tab_groups::TabGroupId& group_id =
          static_cast<tabs::TabGroupTabCollection*>(
              collection_handle.value().Get())
              ->GetTabGroupId();
      tab_group->id = group_id.token();
      tab_group_ids.insert(group_id);
      profile_data->tab_groups.push_back(std::move(tab_group));
    }
  }

  for (const auto& child : container->children) {
    WalkContainer(child, window, profile_data, tab_dedup_keys, tab_group_ids);
  }
}

void TabSearchPageHandler::AddRecentlyClosedEntries(
    std::vector<tab_search::mojom::RecentlyClosedTabPtr>& recently_closed_tabs,
    std::vector<tab_search::mojom::RecentlyClosedTabGroupPtr>&
        recently_closed_tab_groups,
    std::set<tab_groups::TabGroupId>& tab_group_ids,
    std::vector<tab_search::mojom::TabGroupPtr>& tab_groups,
    std::set<DedupKey>& tab_dedup_keys) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  if (!tab_restore_service) {
    return;
  }

  const int kRecentlyClosedTabCountThreshold = 100;
  int recently_closed_tab_count = 0;
  // The minimum number of desired recently closed items (tab or group) to be
  // shown in the 'Recently Closed' section of the UI.
  int recently_closed_item_count = 0;

  // Attempt to add as many recently closed items as necessary to support the
  // default item display count. On reaching this minimum, keep adding
  // items until we have reached or exceeded a tab count threshold value.
  // Ignore any entries that match URLs that are currently open.
  for (auto& entry : tab_restore_service->entries()) {
    if (recently_closed_item_count >= kMinRecentlyClosedItemDisplayCount &&
        recently_closed_tab_count >= kRecentlyClosedTabCountThreshold) {
      return;
    }

    if (entry->type == sessions::tab_restore::Type::WINDOW) {
      sessions::tab_restore::Window* window =
          static_cast<sessions::tab_restore::Window*>(entry.get());

      for (auto& window_tab : window->tabs) {
        sessions::tab_restore::Tab* tab =
            static_cast<sessions::tab_restore::Tab*>(window_tab.get());
        if (AddRecentlyClosedTab(tab, entry->timestamp, recently_closed_tabs,
                                 tab_dedup_keys, tab_group_ids, tab_groups)) {
          recently_closed_tab_count += 1;
          recently_closed_item_count += 1;
        }

        if (recently_closed_item_count >= kMinRecentlyClosedItemDisplayCount &&
            recently_closed_tab_count >= kRecentlyClosedTabCountThreshold) {
          return;
        }
      }
    } else if (entry->type == sessions::tab_restore::Type::TAB) {
      sessions::tab_restore::Tab* tab =
          static_cast<sessions::tab_restore::Tab*>(entry.get());

      if (AddRecentlyClosedTab(tab, entry->timestamp, recently_closed_tabs,
                               tab_dedup_keys, tab_group_ids, tab_groups)) {
        recently_closed_tab_count += 1;
        recently_closed_item_count += 1;
      }
    } else if (entry->type == sessions::tab_restore::Type::GROUP) {
      sessions::tab_restore::Group* group =
          static_cast<sessions::tab_restore::Group*>(entry.get());

      const tab_groups::TabGroupVisualData* tab_group_visual_data =
          &group->visual_data;
      auto recently_closed_tab_group =
          tab_search::mojom::RecentlyClosedTabGroup::New();
      recently_closed_tab_group->session_id = entry->id.id();
      recently_closed_tab_group->id = group->group_id.token();
      recently_closed_tab_group->color = tab_group_visual_data->color();
      recently_closed_tab_group->title =
          base::UTF16ToUTF8(tab_group_visual_data->title());
      recently_closed_tab_group->tab_count = group->tabs.size();
      const base::Time last_active_time =
          (entry->timestamp).is_null() ? GetTabGroupTimeStamp(group->tabs)
                                       : entry->timestamp;
      recently_closed_tab_group->last_active_time = last_active_time;
      recently_closed_tab_group->last_active_elapsed_text =
          GetLastActiveElapsedText(last_active_time);

      for (auto& tab : group->tabs) {
        if (AddRecentlyClosedTab(tab.get(), last_active_time,
                                 recently_closed_tabs, tab_dedup_keys,
                                 tab_group_ids, tab_groups)) {
          recently_closed_tab_count += 1;
        }
      }

      recently_closed_tab_groups.push_back(
          std::move(recently_closed_tab_group));
      // Restored recently closed tab groups map to a single display item.
      recently_closed_item_count += 1;
    }
  }
}

bool TabSearchPageHandler::AddRecentlyClosedTab(
    sessions::tab_restore::Tab* tab,
    const base::Time& close_time,
    std::vector<tab_search::mojom::RecentlyClosedTabPtr>& recently_closed_tabs,
    std::set<DedupKey>& tab_dedup_keys,
    std::set<tab_groups::TabGroupId>& tab_group_ids,
    std::vector<tab_search::mojom::TabGroupPtr>& tab_groups) {
  if (tab->navigations.size() == 0) {
    return false;
  }

  tab_search::mojom::RecentlyClosedTabPtr recently_closed_tab =
      GetRecentlyClosedTab(tab, close_time);

  DedupKey dedup_id(recently_closed_tab->url, recently_closed_tab->group_id);
  // Ignore NTP entries, duplicate entries and tabs with invalid URLs such as
  // empty URLs.
  if (tab_dedup_keys.contains(dedup_id) ||
      recently_closed_tab->url == GURL(chrome::kChromeUINewTabPageURL) ||
      !recently_closed_tab->url.is_valid()) {
    return false;
  }
  tab_dedup_keys.insert(dedup_id);

  if (tab->group.has_value()) {
    recently_closed_tab->group_id = tab->group.value().token();
    CreateTabGroupIfNotPresent(tab, tab_group_ids, tab_groups);
  }

  recently_closed_tabs.push_back(std::move(recently_closed_tab));
  return true;
}

tab_search::mojom::TabPtr TabSearchPageHandler::GetTab(
    tabs::TabInterface* tab) const {
  auto tab_mojom_data = tab_search::mojom::Tab::New();
  content::WebContents* contents = tab->GetContents();

  tab_mojom_data->active = tab->IsActivated();
  tab_mojom_data->visible = tab->IsVisible();
  tab_mojom_data->tab_id = tab->GetHandle().raw_value();
  const std::optional<tab_groups::TabGroupId> group_id = tab->GetGroup();
  if (group_id.has_value()) {
    tab_mojom_data->group_id = group_id.value().token();
  }
  tab_mojom_data->pinned = tab->IsPinned();
  tab_mojom_data->split = tab->IsSplit();

  TabUIHelper* const tab_ui_helper = TabUIHelper::From(tab);
  CHECK(tab_ui_helper);
  tab_mojom_data->title = base::UTF16ToUTF8(tab_ui_helper->GetTitle());
  const auto& last_committed_url = tab_ui_helper->GetLastCommittedURL();
  // A visible URL is used when the a new tab is still loading.
  // If it is cancelled during loading the visible URL becomes empty.
  // We will display an empty URL as about:blank in Javascript.
  if (!last_committed_url.is_valid() || last_committed_url.is_empty()) {
    tab_mojom_data->url = tab_ui_helper->ShouldDisplayURL()
                              ? tab_ui_helper->GetVisibleURL()
                              : GURL(url::kAboutBlankURL);
  } else {
    tab_mojom_data->url = last_committed_url;
  }

  const ui::ImageModel favicon = tab_ui_helper->GetFavicon();
  if (favicon.IsEmpty()) {
    tab_mojom_data->is_default_favicon = true;
  } else {
    const ui::ColorProvider& provider =
        web_ui_->GetWebContents()->GetColorProvider();
    const gfx::ImageSkia default_favicon =
        favicon::GetDefaultFaviconModel().Rasterize(&provider);
    gfx::ImageSkia raster_favicon = favicon.Rasterize(&provider);

    if (tab_ui_helper->ShouldThemifyFavicon()) {
      raster_favicon = ThemeFavicon(raster_favicon, provider);
    }

    tab_mojom_data->favicon_url = GURL(webui::EncodePNGAndMakeDataURI(
        raster_favicon, web_ui_->GetDeviceScaleFactor()));
    tab_mojom_data->is_default_favicon =
        raster_favicon.BackedBySameObjectAs(default_favicon);
  }

  tab_mojom_data->show_icon = tab_ui_helper->ShouldDisplayFavicon();

  // https://crbug.com/435697558: Use the max value of
  // GetLastInteractionTimeTicks and GetLastActiveTimeTicks to account for
  // interaction without across multiple windows without switching tabs.
  const base::TimeTicks last_active_time_ticks =
      std::max(contents->GetLastInteractionTimeTicks(),
               contents->GetLastActiveTimeTicks());
  tab_mojom_data->last_active_time_ticks = last_active_time_ticks;

  // last_active_time_for_testing can affect pixel tests depending on when the
  // view pops up. To make it consistent, override the string to something
  // constant.
  tab_mojom_data->last_active_elapsed_text =
      disable_last_active_time_for_testing_
          ? "0"
          : GetLastActiveElapsedText(last_active_time_ticks);

  std::vector<tabs::TabAlert> alert_states =
      tabs::TabAlertController::From(tab)->GetAllActiveAlerts();
  // Currently, we only report media alert states.
  std::ranges::copy_if(alert_states.begin(), alert_states.end(),
                       std::back_inserter(tab_mojom_data->alert_states),
                       [](tabs::TabAlert alert) {
                         return alert == tabs::TabAlert::kMediaRecording ||
                                alert == tabs::TabAlert::kAudioRecording ||
                                alert == tabs::TabAlert::kVideoRecording ||
                                alert == tabs::TabAlert::kAudioPlaying ||
                                alert == tabs::TabAlert::kAudioMuting ||
                                alert == tabs::TabAlert::kGlicAccessing;
                       });

  return tab_mojom_data;
}

tab_search::mojom::RecentlyClosedTabPtr
TabSearchPageHandler::GetRecentlyClosedTab(sessions::tab_restore::Tab* tab,
                                           const base::Time& close_time) {
  auto recently_closed_tab = tab_search::mojom::RecentlyClosedTab::New();
  DCHECK(tab->navigations.size() > 0);
  sessions::SerializedNavigationEntry& entry =
      tab->navigations[tab->current_navigation_index];
  // N.B. Recently closed tabs use session ids, not TabHandle ids.
  recently_closed_tab->tab_id = tab->id.id();
  recently_closed_tab->url = entry.virtual_url();
  recently_closed_tab->title = entry.title().empty()
                                   ? recently_closed_tab->url.spec()
                                   : base::UTF16ToUTF8(entry.title());
  // Fall back to the navigation last active time if the restore entry has no
  // associated timestamp.
  const base::Time last_active_time =
      close_time.is_null() ? entry.timestamp() : close_time;
  recently_closed_tab->last_active_time = last_active_time;
  recently_closed_tab->last_active_elapsed_text =
      GetLastActiveElapsedText(last_active_time);

  if (tab->group.has_value()) {
    recently_closed_tab->group_id = tab->group.value().token();
  }

  return recently_closed_tab;
}

tabs_api::TabStripService* TabSearchPageHandler::GetTabStripService(
    BrowserWindowInterface* browser) const {
  return browser->GetFeatures()
      .tab_strip_service_feature()
      ->GetTabStripService();
}

void TabSearchPageHandler::OnTabEvents(
    const std::vector<tabs_api::mojom::TabsEventPtr>& events) {
  const auto* preload_state =
      WebUIContentsPreloadState::FromWebContents(web_ui_->GetWebContents());
  if (!IsWebContentsVisible() ||
      (preload_state && preload_state->pending_request)) {
    return;
  }

  for (const auto& event : events) {
    if (event->is_nodes_closed_event()) {
      OnNodesRemoved(event->get_nodes_closed_event());
    } else if (event->is_data_changed_event()) {
      const auto& data_changed_event = event->get_data_changed_event();
      if (data_changed_event->is_tab()) {
        OnTabDataChanged(*data_changed_event->get_tab());
      }
    } else {
      ScheduleDebounce();
    }
  }
}

void TabSearchPageHandler::OnNodesRemoved(
    const tabs_api::mojom::OnNodesClosedEventPtr& event) {
  std::vector<int> tab_ids;
  std::set<SessionID> tab_restore_ids;

  for (const auto& node_id : event->node_ids) {
    if (node_id.Type() == tabs_api::NodeId::Type::kContent) {
      int32_t tab_id;
      if (base::StringToInt(node_id.Id(), &tab_id)) {
        tab_ids.push_back(tab_id);
        std::optional<int32_t> session_id =
            tabs::SessionMappedTabHandleFactory::GetInstance()
                .GetSessionIdForHandle(tab_id);
        if (session_id.has_value()) {
          tab_restore_ids.insert(
              SessionID::FromSerializedValue(session_id.value()));
        }
      }
    } else if (node_id.Type() == tabs_api::NodeId::Type::kCollection) {
      OnSplitTabRemoved();
    }
  }

  if (!tab_ids.empty() || !tab_restore_ids.empty()) {
    OnTabsRemoved(std::move(tab_ids), std::move(tab_restore_ids));
  }
}

void TabSearchPageHandler::OnTabsRemoved(std::vector<int> tab_ids,
                                         std::set<SessionID> tab_restore_ids) {
  auto tabs_removed_info = tab_search::mojom::TabsRemovedInfo::New();
  tabs_removed_info->tab_ids = std::move(tab_ids);

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  if (tab_restore_service) {
    // Loops through at most (TabRestoreServiceHelper) kMaxEntries.
    // Recently closed entries appear first in the list.
    for (auto& entry : tab_restore_service->entries()) {
      if (entry->type == sessions::tab_restore::Type::TAB &&
          tab_restore_ids.contains(entry->id)) {
        // The associated tab group visual data for the recently closed tab
        // is already present at the client side from the initial
        // GetProfileData call.
        sessions::tab_restore::Tab* tab =
            static_cast<sessions::tab_restore::Tab*>(entry.get());
        tabs_removed_info->recently_closed_tabs.push_back(
            GetRecentlyClosedTab(tab, entry->timestamp));
      }
    }
  }

  page_->TabsRemoved(std::move(tabs_removed_info));
}

void TabSearchPageHandler::OnTabDataChanged(
    const tabs_api::mojom::TabChange& event) {
  // Ignore if the UI is hidden or the event doesn't contain
  // relevant tab data changes.
  const std::optional<tabs::TabHandle> handle = event.data->id.ToTabHandle();
  if (!IsWebContentsVisible() || !HasTabSiteDataChanged(event.mask) ||
      !handle) {
    return;
  }

  tabs::TabInterface* const tab = handle->Get();
  if (!tab) {
    return;
  }

  TRACE_EVENT0("browser", "TabSearchPageHandler:OnTabDataChanged");
  const bool is_mark_overlap = metrics_reporter_->HasLocalMark("TabUpdated");
  base::UmaHistogramBoolean("Tabs.TabSearch.Mojo.TabUpdated.IsOverlap",
                            is_mark_overlap);
  if (!is_mark_overlap) {
    metrics_reporter_->Mark("TabUpdated");
  }
  auto tab_update_info = tab_search::mojom::TabUpdateInfo::New();
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  tab_update_info->in_active_window = browser->IsActive();
  tab_update_info->in_host_window = browser == browser_;
  tab_update_info->tab = GetTab(tab);
  page_->TabUpdated(std::move(tab_update_info));
}

void TabSearchPageHandler::OnSplitTabRemoved() {
  GURL url = web_ui_->GetWebContents()->GetURL();
  if (url.spec() != chrome::kChromeUISplitViewNewTabPageURL) {
    return;
  }
  if (!tabs::TabInterface::GetFromContents(web_ui_->GetWebContents())
           ->IsSplit()) {
    page_->TabUnsplit();
  }
}

void TabSearchPageHandler::ScheduleDebounce() {
  if (!debounce_timer_->IsRunning()) {
    debounce_timer_->Reset();
  }
}

void TabSearchPageHandler::NotifyTabsChanged() {
  if (!IsWebContentsVisible()) {
    return;
  }
  page_->TabsChanged(CreateProfileData());
  debounce_timer_->Stop();
}

bool TabSearchPageHandler::IsWebContentsVisible() {
  auto visibility = web_ui_->GetWebContents()->GetVisibility();
  return visibility == content::Visibility::VISIBLE ||
         visibility == content::Visibility::OCCLUDED;
}

void TabSearchPageHandler::BeforeBubbleWidgetShowed() {
  NotifyTabsChanged();
}

void TabSearchPageHandler::SetTimerForTesting(
    std::unique_ptr<base::RetainingOneShotTimer> timer) {
  debounce_timer_ = std::move(timer);
  debounce_timer_->Start(
      FROM_HERE, kTabsChangeDelay,
      base::BindRepeating(&TabSearchPageHandler::NotifyTabsChanged,
                          base::Unretained(this)));
}
