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
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
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
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/tutorial/tutorial_identifier.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
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
                  base::Unretained(this)))) {
  browser_tab_strip_tracker_.Init();
  Profile* profile = Profile::FromWebUI(web_ui_);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      tab_search_prefs::kTabSearchTabIndex,
      base::BindRepeating(&TabSearchPageHandler::NotifyTabIndexPrefChanged,
                          base::Unretained(this), profile));
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
  std::optional<TabDetails> details = GetTabDetails(tab_id);
  if (!details) {
    return;
  }

  ++num_tabs_closed_;

  Profile::FromWebUI(web_ui_)->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabSearchUsed, true);

  // CloseTab() can target the WebContents hosting Tab Search if the Tab Search
  // WebUI is open in a chrome browser tab rather than its bubble. In this case
  // CloseWebContentsAt() closes the WebContents hosting this
  // TabSearchPageHandler object, causing it to be immediately destroyed. Ensure
  // that no further actions are performed following the call to
  // CloseWebContentsAt(). See (https://crbug.com/1175507).
  TabStripModel* const tab_strip_model =
      details->tab->GetBrowserWindowInterface()->GetTabStripModel();
  CHECK(tab_strip_model);
  const int index = details->GetIndex();
  // Don't dangle a tabs::TabInterface* in `details`.
  details.reset();
  tab_strip_model->CloseWebContentsAt(
      index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  // Do not add code past this point.
}

void TabSearchPageHandler::CloseWebUiTab() {
  // CloseTab() can target the WebContents hosting Tab Search if the Tab Search
  // WebUI is open in a chrome browser tab rather than its bubble. In this case
  // CloseWebContentsAt() closes the WebContents hosting this
  // TabSearchPageHandler object, causing it to be immediately destroyed. Ensure
  // that no further actions are performed following the call to
  // CloseWebContentsAt(). See (https://crbug.com/1175507).
  TabStripModel* const tab_strip_model = browser_->tab_strip_model();
  CHECK(tab_strip_model);
  Profile::FromWebUI(web_ui_)->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabSearchUsed, true);
  const int index =
      tab_strip_model->GetIndexOfWebContents(web_ui_->GetWebContents());
  tab_strip_model->CloseWebContentsAt(
      index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
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

void TabSearchPageHandler::GetTabSearchSection(
    GetTabSearchSectionCallback callback) {
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  tab_search::mojom::TabSearchSection section =
      tab_search_prefs::GetTabSearchSectionFromInt(
          prefs->GetInteger(tab_search_prefs::kTabSearchTabIndex));
  std::move(callback).Run(section);
}

std::optional<TabSearchPageHandler::TabDetails>
TabSearchPageHandler::GetTabDetails(int32_t tab_id) {
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    return std::nullopt;
  }
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser || !ShouldTrackBrowser(browser)) {
    return std::nullopt;
  }
  return TabDetails(tab);
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
  const std::optional<TabDetails> details =
      GetTabDetails(switch_to_tab_info->tab_id);
  if (!details) {
    return;
  }

  called_switch_to_tab_ = true;

  Profile::FromWebUI(web_ui_)->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabSearchUsed, true);

  details->tab->GetBrowserWindowInterface()->GetTabStripModel()->ActivateTabAt(
      details->GetIndex());
  // Tab search shows tabs from other windows in the profile. So if a user
  // selects a tab in another window, we need to manually activate it so
  // that we can bring that window to the foreground.
  details->tab->GetBrowserWindowInterface()->GetWindow()->Activate();
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
      TabRestoreServiceFactory::GetForProfile(Profile::FromWebUI(web_ui_));
  if (!tab_restore_service) {
    return;
  }

  Profile::FromWebUI(web_ui_)->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabSearchUsed, true);

  tab_restore_service->RestoreEntryById(
      BrowserLiveTabContext::FindContextForWebContents(
          browser_->tab_strip_model()->GetActiveWebContents()),
      SessionID::FromSerializedValue(session_id),
      WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

void TabSearchPageHandler::ReplaceActiveSplitTab(int32_t replacement_tab_id) {
  std::optional<split_tabs::SplitTabId> split_id =
      browser_->GetActiveTabInterface()->GetSplit();
  if (split_id.has_value()) {
    const tabs::TabInterface* replacement_tab =
        tabs::TabHandle(replacement_tab_id).Get();
    const int32_t replacement_index =
        browser_->tab_strip_model()->GetIndexOfTab(replacement_tab);
    browser_->tab_strip_model()->UpdateTabInSplit(
        browser_->tab_strip_model()->GetActiveTab(), replacement_index,
        TabStripModel::SplitUpdateType::kReplace);
  }
}

void TabSearchPageHandler::SaveRecentlyClosedExpandedPref(bool expanded) {
  Profile::FromWebUI(web_ui_)->GetPrefs()->SetBoolean(
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

void TabSearchPageHandler::TriggerSignIn() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  const signin::IdentityManager* const identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile));
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id)) {
    signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
        profile, signin_metrics::AccessPoint::kTabOrganization);
  } else {
    signin_ui_util::ShowSigninPromptFromPromo(
        profile, signin_metrics::AccessPoint::kTabOrganization);
  }
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
        if (!ShouldTrackBrowser(browser)) {
          return true;
        }
        TabStripModel* const tab_strip_model = browser->GetTabStripModel();

        auto window = tab_search::mojom::Window::New();
        window->active = browser->IsActive();
        window->is_host_window = browser == browser_;
        window->height = browser->GetBrowserForMigrationOnly()
                             ->window()
                             ->GetContentsSize()
                             .height();
        for (int i = 0; i < tab_strip_model->count(); ++i) {
          content::WebContents* const web_contents =
              tab_strip_model->GetWebContentsAt(i);
          // A Tab can potentially be in a state where it has no committed
          // entries during loading and thus has no title/URL. Skip any such
          // pending tabs. These tabs will be added to the list later on once
          // loading has finished.
          if (!web_contents->GetController().GetLastCommittedEntry()) {
            continue;
          }
          tab_search::mojom::TabPtr tab =
              GetTab(tab_strip_model, web_contents, i);
          tab_dedup_keys.insert(DedupKey(tab->url, tab->group_id));
          window->tabs.push_back(std::move(tab));
        }
        profile_data->windows.push_back(std::move(window));

        // Collect tab groups from this browser
        if (tab_strip_model->group_model()) {
          for (auto tab_group_id :
               tab_strip_model->group_model()->ListTabGroups()) {
            const tab_groups::TabGroupVisualData* const tab_group_visual_data =
                tab_strip_model->group_model()
                    ->GetTabGroup(tab_group_id)
                    ->visual_data();

            auto tab_group = tab_search::mojom::TabGroup::New();
            tab_group->id = tab_group_id.token();
            tab_group->title =
                base::UTF16ToUTF8(tab_group_visual_data->title());
            tab_group->color = tab_group_visual_data->color();

            tab_group_ids.insert(tab_group_id);
            profile_data->tab_groups.push_back(std::move(tab_group));
          }
        }
        return true;
      });

  AddRecentlyClosedEntries(profile_data->recently_closed_tabs,
                           profile_data->recently_closed_tab_groups,
                           tab_group_ids, profile_data->tab_groups,
                           tab_dedup_keys);
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
    const TabStripModel* tab_strip_model,
    content::WebContents* contents,
    int index) const {
  auto tab_mojom_data = tab_search::mojom::Tab::New();
  tabs::TabInterface* const tab = tab_strip_model->GetTabAtIndex(index);

  tab_mojom_data->active = tab->IsActivated();
  tab_mojom_data->visible = tab->IsVisible();
  tab_mojom_data->tab_id = tab->GetHandle().raw_value();
  tab_mojom_data->index = index;
  const std::optional<tab_groups::TabGroupId> group_id = tab->GetGroup();
  if (group_id.has_value()) {
    tab_mojom_data->group_id = group_id.value().token();
  }
  tab_mojom_data->pinned = tab->IsPinned();
  tab_mojom_data->split = tab->IsSplit();

  const tabs::TabData tab_data = tabs::TabData::FromTabInterface(tab);
  tab_mojom_data->title = base::UTF16ToUTF8(tab_data.title);
  const auto& last_committed_url = tab_data.last_committed_url;
  // A visible URL is used when the a new tab is still loading.
  // If it is cancelled during loading the visible URL becomes empty.
  // We will display an empty URL as about:blank in Javascript.
  if (!last_committed_url.is_valid() || last_committed_url.is_empty()) {
    tab_mojom_data->url = tab_data.should_display_url
                              ? tab_data.visible_url
                              : GURL(url::kAboutBlankURL);
  } else {
    tab_mojom_data->url = last_committed_url;
  }

  if (tab_data.favicon.IsEmpty()) {
    tab_mojom_data->is_default_favicon = true;
  } else {
    const ui::ColorProvider& provider =
        web_ui_->GetWebContents()->GetColorProvider();
    const gfx::ImageSkia default_favicon =
        favicon::GetDefaultFaviconModel().Rasterize(&provider);
    gfx::ImageSkia raster_favicon = tab_data.favicon.Rasterize(&provider);

    if (tab_data.should_themify_favicon) {
      raster_favicon = ThemeFavicon(raster_favicon, provider);
    }

    tab_mojom_data->favicon_url = GURL(webui::EncodePNGAndMakeDataURI(
        raster_favicon, web_ui_->GetDeviceScaleFactor()));
    tab_mojom_data->is_default_favicon =
        raster_favicon.BackedBySameObjectAs(default_favicon);
  }

  tab_mojom_data->show_icon = tab_data.should_display_favicon;

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

void TabSearchPageHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  const auto* preload_state =
      WebUIContentsPreloadState::FromWebContents(web_ui_->GetWebContents());
  if (!IsWebContentsVisible() ||
      (preload_state && preload_state->pending_request) ||
      browser_tab_strip_tracker_.is_processing_initial_browsers()) {
    return;
  }

  if (change.type() == TabStripModelChange::kRemoved) {
    std::vector<int> tab_ids;
    std::set<SessionID> tab_restore_ids;
    for (const auto& removed_tab : change.GetRemove()->contents) {
      tabs::TabInterface* tab = removed_tab.tab;
      tab_ids.push_back(tab->GetHandle().raw_value());

      if (removed_tab.session_id.has_value() &&
          removed_tab.session_id.value().is_valid()) {
        tab_restore_ids.insert(removed_tab.session_id.value());
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
        if (entry->type == sessions::tab_restore::Type::TAB &&
            tab_restore_ids.contains(entry->id)) {
          // The associated tab group visual data for the recently closed tab is
          // already present at the client side from the initial GetProfileData
          // call.
          sessions::tab_restore::Tab* tab =
              static_cast<sessions::tab_restore::Tab*>(entry.get());
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

void TabSearchPageHandler::OnTabChangedAt(tabs::TabInterface* tab,
                                          int index,
                                          TabChangeType change_type) {
  if (!IsWebContentsVisible()) {
    return;
  }
  // TODO(crbug.com/40709736): Support more values for TabChangeType and filter
  // out the changes we are not interested in.
  if (change_type != TabChangeType::kAll) {
    return;
  }

  TRACE_EVENT0("browser", "TabSearchPageHandler:TabChangedAt");
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
  tab_update_info->tab =
      GetTab(browser->GetTabStripModel(), tab->GetContents(), index);
  page_->TabUpdated(std::move(tab_update_info));
}

void TabSearchPageHandler::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type != SplitTabChange::Type::kRemoved) {
    return;
  }
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

void TabSearchPageHandler::NotifyTabIndexPrefChanged(const Profile* profile) {
  const int32_t section_int =
      profile->GetPrefs()->GetInteger(tab_search_prefs::kTabSearchTabIndex);
  page_->TabSearchSectionChanged(
      tab_search_prefs::GetTabSearchSectionFromInt(section_int));
}

bool TabSearchPageHandler::IsWebContentsVisible() {
  auto visibility = web_ui_->GetWebContents()->GetVisibility();
  return visibility == content::Visibility::VISIBLE ||
         visibility == content::Visibility::OCCLUDED;
}

void TabSearchPageHandler::BeforeBubbleWidgetShowed() {
  NotifyTabsChanged();
}

bool TabSearchPageHandler::ShouldTrackBrowser(BrowserWindowInterface* browser) {
  return browser->GetProfile() == Profile::FromWebUI(web_ui_) &&
         browser->GetType() == BrowserWindowInterface::TYPE_NORMAL;
}

void TabSearchPageHandler::SetTimerForTesting(
    std::unique_ptr<base::RetainingOneShotTimer> timer) {
  debounce_timer_ = std::move(timer);
  debounce_timer_->Start(
      FROM_HERE, kTabsChangeDelay,
      base::BindRepeating(&TabSearchPageHandler::NotifyTabsChanged,
                          base::Unretained(this)));
}
