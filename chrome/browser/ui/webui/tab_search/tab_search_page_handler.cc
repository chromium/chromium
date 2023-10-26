// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/tutorial_service.h"
#include "ui/base/l10n/time_format.h"
#include "ui/color/color_provider.h"

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
    const std::vector<std::unique_ptr<sessions::TabRestoreService::Tab>>&
        tabs) {
  base::Time last_active_time;
  for (const auto& tab : tabs) {
    const sessions::SerializedNavigationEntry& entry =
        tab->navigations[tab->current_navigation_index];
    if (entry.timestamp() > last_active_time)
      last_active_time = entry.timestamp();
  }
  return last_active_time;
}

// If a recently closed tab is associated to a group that is no longer
// open we create a TabGroup entry with the required fields to support
// rendering the tab's associated group information in the UI.
void CreateTabGroupIfNotPresent(
    sessions::TabRestoreService::Tab* tab,
    std::set<tab_groups::TabGroupId>& tab_group_ids,
    std::vector<tab_search::mojom::TabGroupPtr>& tab_groups) {
  if (tab->group.has_value() &&
      !base::Contains(tab_group_ids, tab->group.value())) {
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

}  // namespace

TabSearchPageHandler::TabSearchPageHandler(
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver,
    mojo::PendingRemote<tab_search::mojom::Page> page,
    content::WebUI* web_ui,
    ui::MojoBubbleWebUIController* webui_controller,
    MetricsReporter* metrics_reporter)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_(web_ui),
      webui_controller_(webui_controller),
      metrics_reporter_(metrics_reporter),
      debounce_timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          kTabsChangeDelay,
          base::BindRepeating(&TabSearchPageHandler::NotifyTabsChanged,
                              base::Unretained(this)))) {
  browser_tab_strip_tracker_.Init();
}

TabSearchPageHandler::~TabSearchPageHandler() {
  base::UmaHistogramCounts1000("Tabs.TabSearch.NumTabsClosedPerInstance",
                               num_tabs_closed_);
  base::UmaHistogramEnumeration("Tabs.TabSearch.CloseAction",
                                called_switch_to_tab_
                                    ? TabSearchCloseAction::kTabSwitch
                                    : TabSearchCloseAction::kNoAction);
}

void TabSearchPageHandler::CloseTab(int32_t tab_id) {
  absl::optional<TabDetails> optional_details = GetTabDetails(tab_id);
  if (!optional_details)
    return;

  ++num_tabs_closed_;

  // CloseTab() can target the WebContents hosting Tab Search if the Tab Search
  // WebUI is open in a chrome browser tab rather than its bubble. In this case
  // CloseWebContentsAt() closes the WebContents hosting this
  // TabSearchPageHandler object, causing it to be immediately destroyed. Ensure
  // that no further actions are performed following the call to
  // CloseWebContentsAt(). See (https://crbug.com/1175507).
  auto* tab_strip_model = optional_details->tab_strip_model;
  const int tab_index = optional_details->index;
  tab_strip_model->CloseWebContentsAt(
      tab_index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  // Do not add code past this point.
}

void TabSearchPageHandler::AcceptTabOrganization(
    int32_t session_id,
    int32_t organization_id,
    const std::string& name,
    std::vector<tab_search::mojom::TabPtr> tabs) {
  // TODO(dpenning): Implement this
  Browser* browser = chrome::FindLastActive();
  browser->profile()->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabOrganizationShowFRE, false);
}

void TabSearchPageHandler::RejectTabOrganization(int32_t session_id,
                                                 int32_t organization_id) {
  // TODO(dpenning): Implement this
}

void TabSearchPageHandler::GetProfileData(GetProfileDataCallback callback) {
  TRACE_EVENT0("browser", "TabSearchPageHandler:GetProfileTabs");
  auto profile_tabs = CreateProfileData();
  // On first run record the number of windows and tabs open for the given
  // profile.
  if (!sent_initial_payload_) {
    sent_initial_payload_ = true;
    int tab_count = 0;
    for (const auto& window : profile_tabs->windows)
      tab_count += window->tabs.size();
    base::UmaHistogramCounts100("Tabs.TabSearch.NumWindowsOnOpen",
                                profile_tabs->windows.size());
    base::UmaHistogramCounts10000("Tabs.TabSearch.NumTabsOnOpen", tab_count);

    bool expand_preference =
        Profile::FromWebUI(web_ui_)->GetPrefs()->GetBoolean(
            tab_search_prefs::kTabSearchRecentlyClosedSectionExpanded);
    base::UmaHistogramEnumeration(
        "Tabs.TabSearch.RecentlyClosedSectionToggleStateOnOpen",
        expand_preference ? TabSearchRecentlyClosedToggleAction::kExpand
                          : TabSearchRecentlyClosedToggleAction::kCollapse);
  }

  std::move(callback).Run(std::move(profile_tabs));
}

void TabSearchPageHandler::GetTabOrganizationSession(
    GetTabOrganizationSessionCallback callback) {
  auto session = tab_search::mojom::TabOrganizationSession::New();
  // TODO(dpenning): Fill out session
  std::move(callback).Run(std::move(session));
}

absl::optional<TabSearchPageHandler::TabDetails>
TabSearchPageHandler::GetTabDetails(int32_t tab_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (!ShouldTrackBrowser(browser)) {
      continue;
    }

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int index = 0; index < tab_strip_model->count(); ++index) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(index);
      if (extensions::ExtensionTabUtil::GetTabId(contents) == tab_id) {
        return TabDetails(browser, tab_strip_model, index);
      }
    }
  }

  return absl::nullopt;
}

void TabSearchPageHandler::SwitchToTab(
    tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) {
  absl::optional<TabDetails> optional_details =
      GetTabDetails(switch_to_tab_info->tab_id);
  if (!optional_details)
    return;

  called_switch_to_tab_ = true;

  const TabDetails& details = optional_details.value();
  details.tab_strip_model->ActivateTabAt(details.index);
  details.browser->window()->Activate();
  if (base::FeatureList::IsEnabled(features::kTabSearchUseMetricsReporter)) {
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
}

void TabSearchPageHandler::OpenRecentlyClosedEntry(int32_t session_id) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(Profile::FromWebUI(web_ui_));
  if (!tab_restore_service)
    return;
  Browser* active_browser = chrome::FindLastActive();
  if (!active_browser)
    return;
  tab_restore_service->RestoreEntryById(
      BrowserLiveTabContext::FindContextForWebContents(
          active_browser->tab_strip_model()->GetActiveWebContents()),
      SessionID::FromSerializedValue(session_id),
      WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void TabSearchPageHandler::RequestTabOrganization() {
  // TODO(dpenning): Implement this
}

void TabSearchPageHandler::SaveRecentlyClosedExpandedPref(bool expanded) {
  Profile::FromWebUI(web_ui_)->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabSearchRecentlyClosedSectionExpanded, expanded);

  base::UmaHistogramEnumeration(
      "Tabs.TabSearch.RecentlyClosedSectionToggleAction",
      expanded ? TabSearchRecentlyClosedToggleAction::kExpand
               : TabSearchRecentlyClosedToggleAction::kCollapse);
}

void TabSearchPageHandler::SetTabIndex(int32_t index) {
  Profile::FromWebUI(web_ui_)->GetPrefs()->SetInteger(
      tab_search_prefs::kTabSearchTabIndex, index);
}

void TabSearchPageHandler::StartTabGroupTutorial() {
  // Close the tab search bubble if showing.
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }

  const Browser* const browser = chrome::FindLastActive();
  auto* const user_education_service =
      UserEducationServiceFactory::GetForBrowserContext(browser->profile());
  user_education::TutorialService* const tutorial_service =
      user_education_service ? &user_education_service->tutorial_service()
                             : nullptr;
  CHECK(tutorial_service);

  const ui::ElementContext context = browser->window()->GetElementContext();
  CHECK(context);

  user_education::TutorialIdentifier tutorial_id = kTabGroupTutorialId;
  tutorial_service->StartTutorial(tutorial_id, context);
}

void TabSearchPageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder)
    embedder->ShowUI();
}

tab_search::mojom::ProfileDataPtr TabSearchPageHandler::CreateProfileData() {
  auto profile_data = tab_search::mojom::ProfileData::New();
  Browser* active_browser = chrome::FindLastActive();
  if (!active_browser)
    return profile_data;

  std::set<DedupKey> tab_dedup_keys;
  std::set<tab_groups::TabGroupId> tab_group_ids;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (!ShouldTrackBrowser(browser))
      continue;
    TabStripModel* tab_strip_model = browser->tab_strip_model();

    auto window = tab_search::mojom::Window::New();
    window->active = (browser == active_browser);
    window->height = browser->window()->GetContentsSize().height();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      auto* web_contents = tab_strip_model->GetWebContentsAt(i);
      // A Tab can potentially be in a state where it has no committed entries
      // during loading and thus has no title/URL. Skip any such pending tabs.
      // These tabs will be added to the list later on once loading has
      // finished.
      if (!web_contents->GetController().GetLastCommittedEntry())
        continue;
      tab_search::mojom::TabPtr tab = GetTab(tab_strip_model, web_contents, i);
      tab_dedup_keys.insert(DedupKey(tab->url, tab->group_id));
      window->tabs.push_back(std::move(tab));
    }
    profile_data->windows.push_back(std::move(window));

    if (tab_strip_model->group_model())
      for (auto tab_group_id :
           tab_strip_model->group_model()->ListTabGroups()) {
        const tab_groups::TabGroupVisualData* tab_group_visual_data =
            tab_strip_model->group_model()
                ->GetTabGroup(tab_group_id)
                ->visual_data();

        auto tab_group = tab_search::mojom::TabGroup::New();
        tab_group->id = tab_group_id.token();
        tab_group->title = base::UTF16ToUTF8(tab_group_visual_data->title());
        tab_group->color = tab_group_visual_data->color();

        tab_group_ids.insert(tab_group_id);
        profile_data->tab_groups.push_back(std::move(tab_group));
      }
  }

  AddRecentlyClosedEntries(profile_data->recently_closed_tabs,
                           profile_data->recently_closed_tab_groups,
                           tab_group_ids, profile_data->tab_groups,
                           tab_dedup_keys);
  DCHECK(features::kTabSearchRecentlyClosedTabCountThreshold.Get() >= 0);

  profile_data->recently_closed_section_expanded =
      Profile::FromWebUI(web_ui_)->GetPrefs()->GetBoolean(
          tab_search_prefs::kTabSearchRecentlyClosedSectionExpanded);
  return profile_data;
}

void TabSearchPageHandler::AddRecentlyClosedEntries(
    std::vector<tab_search::mojom::RecentlyClosedTabPtr>& recently_closed_tabs,
    std::vector<tab_search::mojom::RecentlyClosedTabGroupPtr>&
        recently_closed_tab_groups,
    std::set<tab_groups::TabGroupId>& tab_group_ids,
    std::vector<tab_search::mojom::TabGroupPtr>& tab_groups,
    std::set<DedupKey>& tab_dedup_keys) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(Profile::FromWebUI(web_ui_));
  if (!tab_restore_service)
    return;

  const int kRecentlyClosedTabCountThreshold = static_cast<size_t>(
      features::kTabSearchRecentlyClosedTabCountThreshold.Get());
  int recently_closed_tab_count = 0;
  // The minimum number of desired recently closed items (tab or group) to be
  // shown in the 'Recently Closed' section of the UI.
  const int kMinRecentlyClosedItemDisplayCount = static_cast<size_t>(
      features::kTabSearchRecentlyClosedDefaultItemDisplayCount.Get());
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

    if (entry->type == sessions::TabRestoreService::Type::WINDOW) {
      sessions::TabRestoreService::Window* window =
          static_cast<sessions::TabRestoreService::Window*>(entry.get());

      for (auto& window_tab : window->tabs) {
        sessions::TabRestoreService::Tab* tab =
            static_cast<sessions::TabRestoreService::Tab*>(window_tab.get());
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
    } else if (entry->type == sessions::TabRestoreService::Type::TAB) {
      sessions::TabRestoreService::Tab* tab =
          static_cast<sessions::TabRestoreService::Tab*>(entry.get());

      if (AddRecentlyClosedTab(tab, entry->timestamp, recently_closed_tabs,
                               tab_dedup_keys, tab_group_ids, tab_groups)) {
        recently_closed_tab_count += 1;
        recently_closed_item_count += 1;
      }
    } else if (entry->type == sessions::TabRestoreService::Type::GROUP) {
      sessions::TabRestoreService::Group* group =
          static_cast<sessions::TabRestoreService::Group*>(entry.get());

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
    sessions::TabRestoreService::Tab* tab,
    const base::Time& close_time,
    std::vector<tab_search::mojom::RecentlyClosedTabPtr>& recently_closed_tabs,
    std::set<DedupKey>& tab_dedup_keys,
    std::set<tab_groups::TabGroupId>& tab_group_ids,
    std::vector<tab_search::mojom::TabGroupPtr>& tab_groups) {
  if (tab->navigations.size() == 0)
    return false;

  tab_search::mojom::RecentlyClosedTabPtr recently_closed_tab =
      GetRecentlyClosedTab(tab, close_time);

  DedupKey dedup_id(recently_closed_tab->url, recently_closed_tab->group_id);
  // Ignore NTP entries, duplicate entries and tabs with invalid URLs such as
  // empty URLs.
  if (base::Contains(tab_dedup_keys, dedup_id) ||
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
    TabStripModel* tab_strip_model,
    content::WebContents* contents,
    int index) {
  auto tab_data = tab_search::mojom::Tab::New();

  tab_data->active = tab_strip_model->active_index() == index;
  tab_data->tab_id = extensions::ExtensionTabUtil::GetTabId(contents);
  tab_data->index = index;
  const absl::optional<tab_groups::TabGroupId> group_id =
      tab_strip_model->GetTabGroupForTab(index);
  if (group_id.has_value()) {
    tab_data->group_id = group_id.value().token();
  }
  TabRendererData tab_renderer_data =
      TabRendererData::FromTabInModel(tab_strip_model, index);
  tab_data->pinned = tab_renderer_data.pinned;
  tab_data->title = base::UTF16ToUTF8(tab_renderer_data.title);
  const auto& last_committed_url = tab_renderer_data.last_committed_url;
  tab_data->url =
      !last_committed_url.is_valid() || last_committed_url.is_empty()
          ? tab_renderer_data.visible_url
          : last_committed_url;

  if (tab_renderer_data.favicon.IsEmpty()) {
    tab_data->is_default_favicon = true;
  } else {
    const ui::ColorProvider& provider =
        web_ui_->GetWebContents()->GetColorProvider();
    const gfx::ImageSkia default_favicon =
        favicon::GetDefaultFaviconModel().Rasterize(&provider);
    gfx::ImageSkia raster_favicon =
        tab_renderer_data.favicon.Rasterize(&provider);

    if (tab_renderer_data.should_themify_favicon) {
      raster_favicon = ThemeFavicon(raster_favicon, provider);
    }

    tab_data->favicon_url = GURL(webui::EncodePNGAndMakeDataURI(
        raster_favicon, web_ui_->GetDeviceScaleFactor()));
    tab_data->is_default_favicon =
        raster_favicon.BackedBySameObjectAs(default_favicon);
  }

  tab_data->show_icon = tab_renderer_data.show_icon;

  const base::TimeTicks last_active_time_ticks = contents->GetLastActiveTime();
  tab_data->last_active_time_ticks = last_active_time_ticks;
  tab_data->last_active_elapsed_text =
      GetLastActiveElapsedText(last_active_time_ticks);

    std::vector<TabAlertState> alert_states =
        chrome::GetTabAlertStatesForContents(contents);
    // Currently, we only report media alert states.
    base::ranges::copy_if(alert_states.begin(), alert_states.end(),
                          std::back_inserter(tab_data->alert_states),
                          [](TabAlertState alert) {
                            return alert == TabAlertState::MEDIA_RECORDING ||
                                   alert == TabAlertState::AUDIO_PLAYING ||
                                   alert == TabAlertState::AUDIO_MUTING;
                          });

    return tab_data;
}

tab_search::mojom::RecentlyClosedTabPtr
TabSearchPageHandler::GetRecentlyClosedTab(
    sessions::TabRestoreService::Tab* tab,
    const base::Time& close_time) {
  auto recently_closed_tab = tab_search::mojom::RecentlyClosedTab::New();
  DCHECK(tab->navigations.size() > 0);
  sessions::SerializedNavigationEntry& entry =
      tab->navigations[tab->current_navigation_index];
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

void TabSearchPageHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!IsWebContentsVisible() ||
      browser_tab_strip_tracker_.is_processing_initial_browsers()) {
    return;
  }
  if (change.type() == TabStripModelChange::kRemoved) {
    std::vector<int> tab_ids;
    std::set<SessionID> tab_restore_ids;
    for (auto& content_with_index : change.GetRemove()->contents) {
      tab_ids.push_back(
          extensions::ExtensionTabUtil::GetTabId(content_with_index.contents));

      if (content_with_index.session_id.has_value() &&
          content_with_index.session_id.value().is_valid()) {
        tab_restore_ids.insert(content_with_index.session_id.value());
      }
    }

    auto tabs_removed_info = tab_search::mojom::TabsRemovedInfo::New();
    tabs_removed_info->tab_ids = std::move(tab_ids);

    sessions::TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfile(Profile::FromWebUI(web_ui_));
    if (tab_restore_service) {
      // Loops through at most (TabRestoreServiceHelper) kMaxEntries.
      // Recently closed entries appear first in the list.
      for (auto& entry : tab_restore_service->entries()) {
        if (entry->type == sessions::TabRestoreService::Type::TAB &&
            base::Contains(tab_restore_ids, entry->id)) {
          // The associated tab group visual data for the recently closed tab is
          // already present at the client side from the initial GetProfileData
          // call.
          sessions::TabRestoreService::Tab* tab =
              static_cast<sessions::TabRestoreService::Tab*>(entry.get());
          tab_search::mojom::RecentlyClosedTabPtr recently_closed_tab =
              GetRecentlyClosedTab(tab, entry->timestamp);
          tabs_removed_info->recently_closed_tabs.push_back(
              std::move(recently_closed_tab));
        }
      }
    }

    page_->TabsRemoved(std::move(tabs_removed_info));
    return;
  }
  ScheduleDebounce();
}

void TabSearchPageHandler::TabChangedAt(content::WebContents* contents,
                                        int index,
                                        TabChangeType change_type) {
  if (!IsWebContentsVisible())
    return;
  // TODO(crbug.com/1112496): Support more values for TabChangeType and filter
  // out the changes we are not interested in.
  if (change_type != TabChangeType::kAll)
    return;
  Browser* browser = chrome::FindBrowserWithTab(contents);
  if (!browser)
    return;
  Browser* active_browser = chrome::FindLastActive();
  TRACE_EVENT0("browser", "TabSearchPageHandler:TabChangedAt");

  if (base::FeatureList::IsEnabled(features::kTabSearchUseMetricsReporter)) {
    bool is_mark_overlap = metrics_reporter_->HasLocalMark("TabUpdated");
    base::UmaHistogramBoolean("Tabs.TabSearch.Mojo.TabUpdated.IsOverlap",
                              is_mark_overlap);
    if (!is_mark_overlap)
      metrics_reporter_->Mark("TabUpdated");
  }

  auto tab_update_info = tab_search::mojom::TabUpdateInfo::New();
  tab_update_info->in_active_window = (browser == active_browser);
  tab_update_info->tab = GetTab(browser->tab_strip_model(), contents, index);
  page_->TabUpdated(std::move(tab_update_info));
}

void TabSearchPageHandler::OnTabOrganizationSessionChanged() {
  auto session = tab_search::mojom::TabOrganizationSession::New();
  // TODO(dpenning): Fill out session
  session->state = tab_search::mojom::TabOrganizationState::kNotStarted;
  page_->TabOrganizationSessionUpdated(std::move(session));
}

void TabSearchPageHandler::ScheduleDebounce() {
  if (!debounce_timer_->IsRunning())
    debounce_timer_->Reset();
}

void TabSearchPageHandler::NotifyTabsChanged() {
  if (!IsWebContentsVisible())
    return;
  page_->TabsChanged(CreateProfileData());
  debounce_timer_->Stop();
}

bool TabSearchPageHandler::IsWebContentsVisible() {
  auto visibility = web_ui_->GetWebContents()->GetVisibility();
  return visibility == content::Visibility::VISIBLE ||
         visibility == content::Visibility::OCCLUDED;
}

bool TabSearchPageHandler::ShouldTrackBrowser(Browser* browser) {
  return browser->profile() == Profile::FromWebUI(web_ui_) &&
         browser->type() == Browser::Type::TYPE_NORMAL;
}

void TabSearchPageHandler::SetTimerForTesting(
    std::unique_ptr<base::RetainingOneShotTimer> timer) {
  debounce_timer_ = std::move(timer);
  debounce_timer_->Start(
      FROM_HERE, kTabsChangeDelay,
      base::BindRepeating(&TabSearchPageHandler::NotifyTabsChanged,
                          base::Unretained(this)));
}
