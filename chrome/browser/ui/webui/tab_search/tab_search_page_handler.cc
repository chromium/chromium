// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"

#include <stdint.h>

#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/user_education/common/tutorial/tutorial_identifier.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "ui/base/l10n/l10n_util.h"
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

std::string GetLastActiveElapsedTextForDeclutter(
    const base::Time& last_active_time) {
  const base::TimeDelta elapsed = base::Time::Now() - last_active_time;
  return l10n_util::GetPluralStringFUTF8(IDS_DECLUTTER_TIMESTAMP,
                                         elapsed.InDays());
}

// If Tab Group has no timestamp, we find the tab in the tab group with
// the most recent navigation last active time.
base::Time GetTabGroupTimeStamp(
    const std::vector<std::unique_ptr<sessions::tab_restore::Tab>>& tabs) {
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
    sessions::tab_restore::Tab* tab,
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

TabOrganization* GetTabOrganization(TabOrganizationService* service,
                                    int32_t session_id,
                                    int32_t organization_id) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return nullptr;
  }

  if (!service) {
    return nullptr;
  }

  TabOrganizationSession* session = service->GetSessionForBrowser(browser);
  if (!session || session->session_id() != session_id) {
    return nullptr;
  }

  TabOrganization* matching_organization = nullptr;
  for (const std::unique_ptr<TabOrganization>& organization :
       session->tab_organizations()) {
    if (organization->organization_id() == organization_id) {
      matching_organization = organization.get();
      break;
    }
  }

  return matching_organization;
}

tab_search::mojom::TabOrganizationSessionPtr CreateFailedMojoSession() {
  tab_search::mojom::TabOrganizationSessionPtr mojo_session =
      tab_search::mojom::TabOrganizationSession::New();
  mojo_session->state = tab_search::mojom::TabOrganizationState::kFailure;
  mojo_session->error = tab_search::mojom::TabOrganizationError::kGeneric;

  return mojo_session;
}

tab_search::mojom::TabOrganizationSessionPtr CreateNotStartedMojoSession() {
  tab_search::mojom::TabOrganizationSessionPtr mojo_session =
      tab_search::mojom::TabOrganizationSession::New();
  mojo_session->state = tab_search::mojom::TabOrganizationState::kNotStarted;

  return mojo_session;
}

}  // namespace

TabSearchPageHandler::TabSearchPageHandler(
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver,
    mojo::PendingRemote<tab_search::mojom::Page> page,
    content::WebUI* web_ui,
    TopChromeWebUIController* webui_controller,
    MetricsReporter* metrics_reporter)
    : optimization_guide::SettingsEnabledObserver(
          optimization_guide::UserVisibleFeatureKey::kTabOrganization),
      receiver_(this, std::move(receiver)),
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
  pref_change_registrar_.Add(
      tab_search_prefs::kTabOrganizationFeature,
      base::BindRepeating(
          &TabSearchPageHandler::NotifyOrganizationFeaturePrefChanged,
          base::Unretained(this), profile));
  pref_change_registrar_.Add(
      tab_search_prefs::kTabOrganizationShowFRE,
      base::BindRepeating(&TabSearchPageHandler::NotifyShowFREPrefChanged,
                          base::Unretained(this), profile));
  organization_service_ = TabOrganizationServiceFactory::GetForProfile(profile);
  if (organization_service_) {
    tab_organization_observation_.Observe(organization_service_);
  }
  optimization_guide_keyed_service_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_->AddModelExecutionSettingsEnabledObserver(
        this);
  }
  BrowserWindowInterfaceChanged();
}

TabSearchPageHandler::~TabSearchPageHandler() {
  base::UmaHistogramCounts1000("Tabs.TabSearch.NumTabsClosedPerInstance",
                               num_tabs_closed_);
  base::UmaHistogramEnumeration("Tabs.TabSearch.CloseAction",
                                called_switch_to_tab_
                                    ? TabSearchCloseAction::kTabSwitch
                                    : TabSearchCloseAction::kNoAction);
  for (TabOrganizationSession* session : listened_sessions_) {
    session->RemoveObserver(this);
  }
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_
        ->RemoveModelExecutionSettingsEnabledObserver(this);
  }
  pref_change_registrar_.Reset();
}

void TabSearchPageHandler::CloseTab(int32_t tab_id) {
  std::optional<TabDetails> details = GetTabDetails(tab_id);
  if (!details) {
    return;
  }

  ++num_tabs_closed_;

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

void TabSearchPageHandler::DeclutterTabs(const std::vector<int32_t>& tab_ids) {
  // TODO(crbug.com/358382903): Add metrics logging.
  // Potentially also invoke IPH pending UX.
  if (!tab_declutter_controller_) {
    return;
  }

  std::vector<tabs::TabInterface*> tabs;

  // Add tabs that are present in the current browser.
  for (const int32_t tab_id : tab_ids) {
    const std::optional<TabDetails> details = GetTabDetails(tab_id);
    if (!details ||
        details->tab->GetBrowserWindowInterface()->GetTabStripModel() !=
            tab_declutter_controller_->tab_strip_model()) {
      continue;
    }

    tabs.push_back(details->tab);
  }
  tab_declutter_controller_->DeclutterTabs(tabs);

  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }
}

void TabSearchPageHandler::AcceptTabOrganization(
    int32_t session_id,
    int32_t organization_id,
    std::vector<tab_search::mojom::TabPtr> tabs) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  if (!organization_service_) {
    return;
  }

  TabOrganization* organization =
      GetTabOrganization(organization_service_, session_id, organization_id);
  if (!organization) {
    return;
  }

  std::unordered_set<int> tabs_tab_ids;
  for (tab_search::mojom::TabPtr& tab : tabs) {
    tabs_tab_ids.emplace(tab->tab_id);
  }

  std::vector<TabData::TabID> tab_ids_to_remove;
  for (const auto& tab_data : organization->tab_datas()) {
    if (!tab_data->tab()->GetContents() ||
        !base::Contains(tabs_tab_ids, tab_data->tab_id())) {
      tab_ids_to_remove.emplace_back(tab_data->tab_id());
    }
  }

  for (const auto& tab_id : tab_ids_to_remove) {
    organization->RemoveTabData(tab_id);
  }

  organization_service_->AcceptTabOrganization(browser, session_id,
                                               organization_id);

  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }
}

void TabSearchPageHandler::RejectTabOrganization(int32_t session_id,
                                                 int32_t organization_id) {
  TabOrganization* organization =
      GetTabOrganization(organization_service_, session_id, organization_id);
  if (!organization) {
    return;
  }

  organization->Reject();
}

void TabSearchPageHandler::RenameTabOrganization(int32_t session_id,
                                                 int32_t organization_id,
                                                 const std::u16string& name) {
  TabOrganization* organization =
      GetTabOrganization(organization_service_, session_id, organization_id);
  if (!organization) {
    return;
  }

  organization->SetCurrentName(name);
}

void TabSearchPageHandler::ExcludeFromStaleTabs(int32_t tab_id) {
  if (!tab_declutter_controller_) {
    return;
  }

  std::optional<TabDetails> details = GetTabDetails(tab_id);

  if (!details ||
      details->tab->GetBrowserWindowInterface()->GetTabStripModel() !=
          tab_declutter_controller_->tab_strip_model()) {
    return;
  }

  tab_declutter_controller_->ExcludeFromStaleTabs(details->tab);

  std::erase(stale_tabs_, details->tab);

  page_->StaleTabsChanged(GetMojoStaleTabs());
}

void TabSearchPageHandler::RegisterTabDeclutterCallbacks(
    tabs::TabInterface* tab) {
  std::vector<base::CallbackListSubscription> subscriptions;

  subscriptions.push_back(tab->RegisterDidEnterForeground(
      base::BindRepeating(&TabSearchPageHandler::OnStaleTabDidEnterForeground,
                          base::Unretained(this))));

  subscriptions.push_back(tab->RegisterWillDetach(base::BindRepeating(
      &TabSearchPageHandler::OnStaleTabWillDetach, base::Unretained(this))));

  subscriptions.push_back(tab->RegisterPinnedStateChanged(
      base::BindRepeating(&TabSearchPageHandler::OnStaleTabPinnedStateChanged,
                          base::Unretained(this))));

  subscriptions.push_back(tab->RegisterGroupChanged(base::BindRepeating(
      &TabSearchPageHandler::OnStaleTabGroupChanged, base::Unretained(this))));

  tab_declutter_subscriptions_map_[tab] = std::move(subscriptions);
}

void TabSearchPageHandler::UnregisterTabCallbacks() {
  tab_declutter_subscriptions_map_.clear();
}

void TabSearchPageHandler::RemoveStaleTab(tabs::TabInterface* tab) {
  CHECK(tab);
  CHECK(std::find(stale_tabs_.begin(), stale_tabs_.end(), tab) !=
        stale_tabs_.end());
  CHECK(tab_declutter_subscriptions_map_.find(tab) !=
        tab_declutter_subscriptions_map_.end());

  // Remove the TabInterface from stale_tabs_
  stale_tabs_.erase(std::remove(stale_tabs_.begin(), stale_tabs_.end(), tab),
                    stale_tabs_.end());

  // Unregister the subscriptions for this TabInterface
  tab_declutter_subscriptions_map_.erase(tab);
}

void TabSearchPageHandler::BrowserWindowInterfaceChanged() {
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_ui_->GetWebContents());
  SetTabDeclutterController(
      browser_window_interface
          ? browser_window_interface->GetFeatures().tab_declutter_controller()
          : nullptr);
}

void TabSearchPageHandler::OnStaleTabDidEnterForeground(
    tabs::TabInterface* tab) {
  RemoveStaleTab(static_cast<tabs::TabInterface*>(tab));
  page_->StaleTabsChanged(GetMojoStaleTabs());
}

void TabSearchPageHandler::OnStaleTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  RemoveStaleTab(static_cast<tabs::TabInterface*>(tab));
  page_->StaleTabsChanged(GetMojoStaleTabs());
}

void TabSearchPageHandler::OnStaleTabPinnedStateChanged(tabs::TabInterface* tab,
                                                        bool new_pinned_state) {
  RemoveStaleTab(tab);
  page_->StaleTabsChanged(GetMojoStaleTabs());
}

void TabSearchPageHandler::OnStaleTabGroupChanged(
    tabs::TabInterface* tab,
    std::optional<tab_groups::TabGroupId> new_group) {
  RemoveStaleTab(tab);
  page_->StaleTabsChanged(GetMojoStaleTabs());
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

void TabSearchPageHandler::GetStaleTabs(GetStaleTabsCallback callback) {
  UpdateStaleTabs();
  std::move(callback).Run(GetMojoStaleTabs());
}

void TabSearchPageHandler::GetTabSearchSection(
    GetTabSearchSectionCallback callback) {
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  tab_search::mojom::TabSearchSection section =
      tab_search_prefs::GetTabSearchSectionFromInt(
          prefs->GetInteger(tab_search_prefs::kTabSearchTabIndex));
  if (section == tab_search::mojom::TabSearchSection::kNone) {
    section = tab_search::mojom::TabSearchSection::kSearch;
  }
  std::move(callback).Run(section);
}

void TabSearchPageHandler::GetTabOrganizationFeature(
    GetTabOrganizationFeatureCallback callback) {
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  const tab_search::mojom::TabOrganizationFeature feature =
      tab_search_prefs::GetTabOrganizationFeatureFromInt(
          prefs->GetInteger(tab_search_prefs::kTabOrganizationFeature));
  std::move(callback).Run(feature);
}

void TabSearchPageHandler::GetTabOrganizationSession(
    GetTabOrganizationSessionCallback callback) {
  Browser* browser = chrome::FindLastActive();
  if (!browser || !browser->tab_strip_model()->SupportsTabGroups() ||
      !organization_service_) {
    std::move(callback).Run(CreateFailedMojoSession());
    return;
  }

  TabOrganizationSession* session =
      organization_service_->GetSessionForBrowser(browser);
  if (!session) {
    session = organization_service_->CreateSessionForBrowser(
        browser, TabOrganizationEntryPoint::kTabSearch);
  }

  if (!base::Contains(listened_sessions_, session)) {
    session->AddObserver(this);
    listened_sessions_.emplace_back(session);
  }

  tab_search::mojom::TabOrganizationSessionPtr mojo_session =
      GetMojoForTabOrganizationSession(session);

  std::move(callback).Run(std::move(mojo_session));
}

std::optional<TabSearchPageHandler::TabDetails>
TabSearchPageHandler::GetTabDetails(int32_t tab_id) {
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab) {
    return std::nullopt;
  }

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!ShouldTrackBrowser(browser)) {
      continue;
    }

    TabStripModel* const tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model ==
        tab->GetBrowserWindowInterface()->GetTabStripModel()) {
      return TabDetails(browser, tab);
    }
  }

  return std::nullopt;
}

void TabSearchPageHandler::GetTabOrganizationModelStrategy(
    GetTabOrganizationModelStrategyCallback callback) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  const int32_t strategy_int = profile->GetPrefs()->GetInteger(
      tab_search_prefs::kTabOrganizationModelStrategy);
  const auto strategy =
      static_cast<tab_search::mojom::TabOrganizationModelStrategy>(
          strategy_int);
  std::move(callback).Run(std::move(strategy));
}

void TabSearchPageHandler::SwitchToTab(
    tab_search::mojom::SwitchToTabInfoPtr switch_to_tab_info) {
  const std::optional<TabDetails> details =
      GetTabDetails(switch_to_tab_info->tab_id);
  if (!details) {
    return;
  }

  called_switch_to_tab_ = true;

  details->tab->GetBrowserWindowInterface()->GetTabStripModel()->ActivateTabAt(
      details->GetIndex());
  details->browser->window()->Activate();
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
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  if (!organization_service_) {
    return;
  }

  TabOrganizationSession* session =
      organization_service_->GetSessionForBrowser(browser);
  if (!session) {
    session = organization_service_->CreateSessionForBrowser(
        browser, TabOrganizationEntryPoint::kTabSearch);
  } else if (session->IsComplete()) {
    session = organization_service_->ResetSessionForBrowser(
        browser, TabOrganizationEntryPoint::kTabSearch);
  }

  if (!base::Contains(listened_sessions_, session)) {
    session->AddObserver(this);
    listened_sessions_.emplace_back(session);
  }

  browser->profile()->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabOrganizationShowFRE, false);
  organization_service_->StartRequest(browser,
                                      TabOrganizationEntryPoint::kTabSearch);
}

void TabSearchPageHandler::RemoveTabFromOrganization(
    int32_t session_id,
    int32_t organization_id,
    tab_search::mojom::TabPtr tab) {
  if (!organization_service_) {
    return;
  }

  TabOrganization* organization =
      GetTabOrganization(organization_service_, session_id, organization_id);
  if (!organization) {
    return;
  }

  organization->RemoveTabData(tab->tab_id);
}

void TabSearchPageHandler::RejectSession(int32_t session_id) {
  Browser* browser = chrome::FindLastActive();
  if (!browser || !organization_service_) {
    return;
  }

  TabOrganizationSession* session =
      organization_service_->GetSessionForBrowser(browser);
  if (!session || session->session_id() != session_id) {
    return;
  }

  for (const std::unique_ptr<TabOrganization>& organization :
       session->tab_organizations()) {
    // Organization may have already been rejected, but should not have been
    // accepted.
    CHECK(organization->choice() != TabOrganization::UserChoice::kAccepted);

    if (organization->choice() == TabOrganization::UserChoice::kNoChoice) {
      organization->Reject();
    }
  }

  organization_service_->ResetSessionForBrowser(
      browser, TabOrganizationEntryPoint::kTabSearch, nullptr);
}

void TabSearchPageHandler::RestartSession() {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  if (!organization_service_) {
    return;
  }

  restarting_ = true;
  TabOrganizationSession* current_session =
      organization_service_->GetSessionForBrowser(browser);
  const tabs::TabInterface* base_session_tab =
      current_session ? current_session->base_session_tab() : nullptr;
  // Don't notify observers to avoid a repaint
  TabOrganizationSession* session =
      organization_service_->ResetSessionForBrowser(
          browser, TabOrganizationEntryPoint::kTabSearch, base_session_tab);
  if (!base::Contains(listened_sessions_, session)) {
    session->AddObserver(this);
    listened_sessions_.emplace_back(session);
  }

  organization_service_->StartRequest(browser,
                                      TabOrganizationEntryPoint::kTabSearch);

  restarting_ = false;

  OnTabOrganizationSessionUpdated(session);
}

void TabSearchPageHandler::SaveRecentlyClosedExpandedPref(bool expanded) {
  Profile::FromWebUI(web_ui_)->GetPrefs()->SetBoolean(
      tab_search_prefs::kTabSearchRecentlyClosedSectionExpanded, expanded);

  base::UmaHistogramEnumeration(
      "Tabs.TabSearch.RecentlyClosedSectionToggleAction",
      expanded ? TabSearchRecentlyClosedToggleAction::kExpand
               : TabSearchRecentlyClosedToggleAction::kCollapse);
}

void TabSearchPageHandler::SetOrganizationFeature(
    tab_search::mojom::TabOrganizationFeature feature) {
  Profile::FromWebUI(web_ui_)->GetPrefs()->SetInteger(
      tab_search_prefs::kTabOrganizationFeature,
      tab_search_prefs::GetIntFromTabOrganizationFeature(feature));
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

void TabSearchPageHandler::TriggerFeedback(int32_t session_id) {
  TabOrganizationSession* session =
      organization_service_->GetSessionForBrowser(chrome::FindLastActive());
  const std::u16string feedback_id = session->feedback_id();
  // Bypass feedback flow if there is no feedback id, as in tests.
  if (session->session_id() != session_id || feedback_id.length() == 0) {
    return;
  }
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }
  OptimizationGuideKeyedService* opt_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser->profile());
  if (!opt_guide_keyed_service ||
      !opt_guide_keyed_service->ShouldFeatureBeCurrentlyAllowedForFeedback(
          optimization_guide::proto::LogAiDataRequest::kTabOrganization)) {
    return;
  }
  base::Value::Dict feedback_metadata;
  feedback_metadata.Set("log_id", feedback_id);
  chrome::ShowFeedbackPage(
      browser, feedback::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_TAB_ORGANIZATION_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"tab_organization",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(feedback_metadata));
}

void TabSearchPageHandler::TriggerSignIn() {
  Profile* profile = chrome::FindLastActive()->profile();
  const signin::IdentityManager* const identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile));
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id)) {
    signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
        profile, signin_metrics::AccessPoint::ACCESS_POINT_TAB_ORGANIZATION);
  } else {
    signin_ui_util::ShowSigninPromptFromPromo(
        profile, signin_metrics::AccessPoint::ACCESS_POINT_TAB_ORGANIZATION);
  }
}

void TabSearchPageHandler::OpenHelpPage() {
  Browser* browser = chrome::FindLastActive();
  GURL help_url(chrome::kTabOrganizationLearnMorePageURL);
  NavigateParams params(browser, help_url,
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void TabSearchPageHandler::SetTabOrganizationModelStrategy(
    tab_search::mojom::TabOrganizationModelStrategy strategy) {
  const auto strategy_int = static_cast<int32_t>(strategy);
  Profile* profile = Profile::FromWebUI(web_ui_);
  profile->GetPrefs()->SetInteger(
      tab_search_prefs::kTabOrganizationModelStrategy, strategy_int);
  page_->TabOrganizationModelStrategyUpdated(std::move(strategy));
}

void TabSearchPageHandler::SetUserFeedback(
    int32_t session_id,
    int32_t organization_id,
    tab_search::mojom::UserFeedback feedback) {
  optimization_guide::proto::UserFeedback user_feedback;
  switch (feedback) {
    case tab_search::mojom::UserFeedback::kUserFeedBackPositive:
      user_feedback =
          optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP;
      break;
    case tab_search::mojom::UserFeedback::kUserFeedBackNegative:
      user_feedback =
          optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN;
      break;
    case tab_search::mojom::UserFeedback::kUserFeedBackUnspecified:
      user_feedback =
          optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
      break;
  }
  if (base::FeatureList::IsEnabled(features::kMultiTabOrganization)) {
    CHECK(organization_id == -1);
    Browser* browser = chrome::FindLastActive();
    if (!browser) {
      return;
    }
    TabOrganizationSession* session =
        organization_service_->GetSessionForBrowser(browser);
    if (!session) {
      return;
    }
    session->SetFeedback(user_feedback);
  } else {
    CHECK(organization_id >= 0);
    TabOrganization* organization =
        GetTabOrganization(organization_service_, session_id, organization_id);
    if (!organization) {
      return;
    }
    organization->SetFeedback(user_feedback);
  }
}

void TabSearchPageHandler::NotifyOrganizationUIReadyToShow() {
  organization_ready_to_show_ = true;
  MaybeShowUI();
}

void TabSearchPageHandler::NotifySearchUIReadyToShow() {
  search_ready_to_show_ = true;
  MaybeShowUI();
}

void TabSearchPageHandler::MaybeShowUI() {
  Profile* const profile = Profile::FromWebUI(web_ui_);
  bool organization_enabled =
      TabOrganizationUtils::GetInstance()->IsEnabled(profile) &&
      organization_service_;
  if ((organization_enabled && !organization_ready_to_show_) ||
      !search_ready_to_show_) {
    return;
  }
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
  for (Browser* browser : *BrowserList::GetInstance()) {
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

void TabSearchPageHandler::UpdateStaleTabs() {
  stale_tabs_.clear();
  UnregisterTabCallbacks();
  if (!tab_declutter_controller_) {
    return;
  }
  std::vector<tabs::TabInterface*> stale_tabs =
      tab_declutter_controller_->GetStaleTabs();
  stale_tabs_ = stale_tabs;
  for (tabs::TabInterface* tab : stale_tabs_) {
    RegisterTabDeclutterCallbacks(tab);
  }
}

void TabSearchPageHandler::SetTabDeclutterController(
    tabs::TabDeclutterController* tab_declutter_controller) {
  if (tab_declutter_controller == tab_declutter_controller_) {
    return;
  }

  tab_declutter_observation_.Reset();
  tab_declutter_controller_ = tab_declutter_controller;
  if (tab_declutter_controller_) {
    tab_declutter_observation_.Observe(tab_declutter_controller_.get());
    UpdateStaleTabs();
    page_->StaleTabsChanged(GetMojoStaleTabs());
  }
}

void TabSearchPageHandler::OnStaleTabsProcessed(
    std::vector<tabs::TabInterface*> tabs) {
  stale_tabs_.clear();
  UnregisterTabCallbacks();
  stale_tabs_ = tabs;
  for (tabs::TabInterface* tab : stale_tabs_) {
    RegisterTabDeclutterCallbacks(tab);
  }
  page_->StaleTabsChanged(GetMojoStaleTabs());
}

std::vector<mojo::StructPtr<tab_search::mojom::Tab>>
TabSearchPageHandler::GetMojoStaleTabs() {
  std::vector<mojo::StructPtr<tab_search::mojom::Tab>> mojo_tabs;
  if (!tab_declutter_controller_) {
    return mojo_tabs;
  }
  TabStripModel* tab_strip_model = tab_declutter_controller_->tab_strip_model();

  for (tabs::TabInterface* tab : stale_tabs_) {
    const int tab_index =
        tab_strip_model->GetIndexOfWebContents(tab->GetContents());
    const std::string last_active_text = GetLastActiveElapsedTextForDeclutter(
        tab->GetContents()->GetLastActiveTime());
    mojo_tabs.push_back(GetTab(tab_strip_model, tab->GetContents(), tab_index,
                               last_active_text));
  }
  return mojo_tabs;
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
    const TabStripModel* tab_strip_model,
    content::WebContents* contents,
    int index,
    std::string custom_last_active_text) const {
  auto tab_data = tab_search::mojom::Tab::New();
  const tabs::TabInterface* const tab = tab_strip_model->GetTabAtIndex(index);

  tab_data->active = tab->IsInForeground();
  tab_data->tab_id = tab->GetTabHandle();
  tab_data->index = index;
  const std::optional<tab_groups::TabGroupId> group_id = tab->GetGroup();
  if (group_id.has_value()) {
    tab_data->group_id = group_id.value().token();
  }
  tab_data->pinned = tab->IsPinned();

  TabRendererData tab_renderer_data =
      TabRendererData::FromTabInModel(tab_strip_model, index);
  tab_data->title = base::UTF16ToUTF8(tab_renderer_data.title);
  const auto& last_committed_url = tab_renderer_data.last_committed_url;
  // A visible URL is used when the a new tab is still loading.
  // If it is cancelled during loading the visible URL becomes empty.
  // We will display an empty URL as about:blank in Javascript.
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

  const base::TimeTicks last_active_time_ticks =
      contents->GetLastActiveTimeTicks();
  tab_data->last_active_time_ticks = last_active_time_ticks;

  // last_active_time_for_testing can affect pixel tests depending on when the
  // view pops up. To make it consistent, override the string to something
  // constant.
  tab_data->last_active_elapsed_text =
      disable_last_active_time_for_testing_ ? "0"
      : custom_last_active_text.length() > 0
          ? custom_last_active_text
          : GetLastActiveElapsedText(last_active_time_ticks);

  std::vector<TabAlertState> alert_states =
      GetTabAlertStatesForContents(contents);
  // Currently, we only report media alert states.
  base::ranges::copy_if(alert_states.begin(), alert_states.end(),
                        std::back_inserter(tab_data->alert_states),
                        [](TabAlertState alert) {
                          return alert == TabAlertState::MEDIA_RECORDING ||
                                 alert == TabAlertState::AUDIO_RECORDING ||
                                 alert == TabAlertState::VIDEO_RECORDING ||
                                 alert == TabAlertState::AUDIO_PLAYING ||
                                 alert == TabAlertState::AUDIO_MUTING;
                        });

  return tab_data;
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
  if (!IsWebContentsVisible() ||
      browser_tab_strip_tracker_.is_processing_initial_browsers()) {
    return;
  }

  if (change.type() == TabStripModelChange::kRemoved) {
    std::vector<int> tab_ids;
    std::set<SessionID> tab_restore_ids;
    for (const auto& removed_tab : change.GetRemove()->contents) {
      tabs::TabInterface* tab = removed_tab.tab;
      tab_ids.push_back(tab->GetTabHandle());

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
            base::Contains(tab_restore_ids, entry->id)) {
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

void TabSearchPageHandler::TabChangedAt(content::WebContents* contents,
                                        int index,
                                        TabChangeType change_type) {
  if (!IsWebContentsVisible())
    return;
  // TODO(crbug.com/40709736): Support more values for TabChangeType and filter
  // out the changes we are not interested in.
  if (change_type != TabChangeType::kAll)
    return;
  Browser* browser = chrome::FindBrowserWithTab(contents);
  if (!browser)
    return;
  Browser* active_browser = chrome::FindLastActive();
  TRACE_EVENT0("browser", "TabSearchPageHandler:TabChangedAt");

  const bool is_mark_overlap = metrics_reporter_->HasLocalMark("TabUpdated");
  base::UmaHistogramBoolean("Tabs.TabSearch.Mojo.TabUpdated.IsOverlap",
                            is_mark_overlap);
  if (!is_mark_overlap) {
    metrics_reporter_->Mark("TabUpdated");
  }

  auto tab_update_info = tab_search::mojom::TabUpdateInfo::New();
  tab_update_info->in_active_window = (browser == active_browser);
  tab_update_info->tab = GetTab(browser->tab_strip_model(), contents, index);
  page_->TabUpdated(std::move(tab_update_info));
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

void TabSearchPageHandler::NotifyTabIndexPrefChanged(const Profile* profile) {
  const int32_t section_int =
      profile->GetPrefs()->GetInteger(tab_search_prefs::kTabSearchTabIndex);
  page_->TabSearchSectionChanged(
      tab_search_prefs::GetTabSearchSectionFromInt(section_int));
}

void TabSearchPageHandler::NotifyOrganizationFeaturePrefChanged(
    const Profile* profile) {
  const int32_t feature_int = profile->GetPrefs()->GetInteger(
      tab_search_prefs::kTabOrganizationFeature);
  page_->TabOrganizationFeatureChanged(
      tab_search_prefs::GetTabOrganizationFeatureFromInt(feature_int));
}

void TabSearchPageHandler::NotifyShowFREPrefChanged(const Profile* profile) {
  const bool show_fre = profile->GetPrefs()->GetBoolean(
      tab_search_prefs::kTabOrganizationShowFRE);
  page_->ShowFREChanged(show_fre);
}

bool TabSearchPageHandler::IsWebContentsVisible() {
  auto visibility = web_ui_->GetWebContents()->GetVisibility();
  return visibility == content::Visibility::VISIBLE ||
         visibility == content::Visibility::OCCLUDED;
}

tab_search::mojom::TabPtr TabSearchPageHandler::GetMojoForTabData(
    TabData* tab_data) const {
  return TabSearchPageHandler::GetTab(
      tab_data->original_tab_strip_model(), tab_data->tab()->GetContents(),
      tab_data->original_tab_strip_model()->GetIndexOfWebContents(
          tab_data->tab()->GetContents()));
}

tab_search::mojom::TabOrganizationPtr
TabSearchPageHandler::GetMojoForTabOrganization(
    const TabOrganization* organization) const {
  tab_search::mojom::TabOrganizationPtr mojo_organization =
      tab_search::mojom::TabOrganization::New();

  std::vector<tab_search::mojom::TabPtr> tabs;
  for (const std::unique_ptr<TabData>& tab_data : organization->tab_datas()) {
    if (!tab_data->IsValidForOrganizing(organization->group_id())) {
      continue;
    }

    tabs.emplace_back(GetMojoForTabData(tab_data.get()));
  }

  mojo_organization->organization_id = organization->organization_id();
  mojo_organization->tabs = std::move(tabs);
  mojo_organization->first_new_tab_index = organization->first_new_tab_index();
  mojo_organization->name = organization->GetDisplayName();

  return mojo_organization;
}

tab_search::mojom::TabOrganizationSessionPtr
TabSearchPageHandler::GetMojoForTabOrganizationSession(
    const TabOrganizationSession* session) const {
  tab_search::mojom::TabOrganizationSessionPtr mojo_session =
      tab_search::mojom::TabOrganizationSession::New();

  mojo_session->session_id = session->session_id();
  mojo_session->error = tab_search::mojom::TabOrganizationError::kNone;
  mojo_session->active_tab_id =
      session->base_session_tab() ? session->base_session_tab()->GetTabHandle()
                                  : tabs::TabHandle::NullValue;
  std::vector<tab_search::mojom::TabOrganizationPtr> organizations;

  TabOrganizationRequest::State state = session->request()->state();
  switch (state) {
    case TabOrganizationRequest::State::NOT_STARTED: {
      mojo_session->state =
          tab_search::mojom::TabOrganizationState::kNotStarted;
      break;
    }
    case TabOrganizationRequest::State::STARTED: {
      mojo_session->state =
          tab_search::mojom::TabOrganizationState::kInProgress;
      break;
    }
    case TabOrganizationRequest::State::COMPLETED: {
      if (session->tab_organizations().size() > 0) {
        for (const std::unique_ptr<TabOrganization>& organization :
             session->tab_organizations()) {
          if (!organization->IsValidForOrganizing() ||
              organization->choice() !=
                  TabOrganization::UserChoice::kNoChoice) {
            continue;
          }
          organizations.emplace_back(
              GetMojoForTabOrganization(organization.get()));
        }
        if (organizations.size() > 0) {
          mojo_session->state =
              tab_search::mojom::TabOrganizationState::kSuccess;
        } else {
          mojo_session->state =
              tab_search::mojom::TabOrganizationState::kFailure;
          mojo_session->error =
              tab_search::mojom::TabOrganizationError::kGrouping;
        }
      } else {
        mojo_session->state = tab_search::mojom::TabOrganizationState::kFailure;
        mojo_session->error =
            tab_search::mojom::TabOrganizationError::kGrouping;
      }
      break;
    }
    case TabOrganizationRequest::State::FAILED:
    case TabOrganizationRequest::State::CANCELED: {
      mojo_session->state = tab_search::mojom::TabOrganizationState::kFailure;
      mojo_session->error = tab_search::mojom::TabOrganizationError::kGeneric;
      break;
    }
  }
  mojo_session->organizations = std::move(organizations);

  return mojo_session;
}

void TabSearchPageHandler::OnTabOrganizationSessionUpdated(
    const TabOrganizationSession* session) {
  if (restarting_ || !base::Contains(listened_sessions_, session)) {
    return;
  }

  tab_search::mojom::TabOrganizationSessionPtr mojo_session =
      GetMojoForTabOrganizationSession(session);

  page_->TabOrganizationSessionUpdated(std::move(mojo_session));
}

void TabSearchPageHandler::OnTabOrganizationSessionDestroyed(
    TabOrganizationSession::ID session_id) {
  for (auto session_iter = listened_sessions_.begin();
       session_iter != listened_sessions_.end(); session_iter++) {
    if (session_id == (*session_iter)->session_id()) {
      listened_sessions_.erase(session_iter);
      // Ignore this update when restarting, as it will be replaced by the new
      // session.
      if (!restarting_) {
        page_->TabOrganizationSessionUpdated(CreateNotStartedMojoSession());
      }
      return;
    }
  }
}

void TabSearchPageHandler::OnSessionCreated(const Browser* browser,
                                            TabOrganizationSession* session) {
  Profile* const profile = Profile::FromWebUI(web_ui_);
  if (restarting_ || !browser || browser->profile() != profile) {
    return;
  }

  session->AddObserver(this);
  listened_sessions_.emplace_back(session);

  OnTabOrganizationSessionUpdated(session);
}

void TabSearchPageHandler::OnChangeInFeatureCurrentlyEnabledState(
    bool is_now_enabled) {
  Profile* const profile = Profile::FromWebUI(web_ui_);
  // This logic is slightly more strict than is_now_enabled, may make a
  // difference in some edge cases.
  bool enabled = TabOrganizationUtils::GetInstance()->IsEnabled(profile);
  page_->TabOrganizationEnabledChanged(enabled && organization_service_);
}

void TabSearchPageHandler::SetTabDeclutterControllerForTesting(
    tabs::TabDeclutterController* tab_declutter_controller) {
  SetTabDeclutterController(tab_declutter_controller);
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
