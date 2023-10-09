// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_live_tab_context.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/token.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/closed_tab_cache.h"
#include "chrome/browser/sessions/closed_tab_cache_service_factory.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/buildflags.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/content/content_platform_specific_tab_data.h"
#include "components/sessions/core/session_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/session_storage_namespace.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/tab_loader.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/side_search/side_search_utils.h"
#endif  // defined(TOOLKIT_VIEWS)

using content::NavigationController;
using content::SessionStorageNamespace;
using content::WebContents;

namespace {

// |app_name| can could be for an app that has been uninstalled. In that
// case we don't want to open an app window. Note that |app_name| is also used
// for other types of windows like dev tools and we always want to open an
// app window in those cases.
bool ShouldCreateAppWindowForAppName(Profile* profile,
                                     const std::string& app_name) {
  if (app_name.empty())
    return false;

  // Only need to check that the app is installed if |app_name| is for a
  // platform app or web app. (|app_name| could also be for a devtools window.)
  const std::string app_id = web_app::GetAppIdFromApplicationName(app_name);
  if (app_id.empty())
    return true;

  return apps::IsInstalledApp(profile, app_id);
}

}  // namespace

void BrowserLiveTabContext::ShowBrowserWindow() {
  browser_->window()->Show();
}

SessionID BrowserLiveTabContext::GetSessionID() const {
  return browser_->session_id();
}

sessions::SessionWindow::WindowType BrowserLiveTabContext::GetWindowType()
    const {
  return WindowTypeForBrowserType(browser_->type());
}

int BrowserLiveTabContext::GetTabCount() const {
  return browser_->tab_strip_model()->count();
}

int BrowserLiveTabContext::GetSelectedIndex() const {
  return browser_->tab_strip_model()->active_index();
}

std::string BrowserLiveTabContext::GetAppName() const {
  return browser_->app_name();
}

std::string BrowserLiveTabContext::GetUserTitle() const {
  return browser_->user_title();
}

sessions::LiveTab* BrowserLiveTabContext::GetLiveTabAt(int index) const {
  return sessions::ContentLiveTab::GetForWebContents(
      browser_->tab_strip_model()->GetWebContentsAt(index));
}

sessions::LiveTab* BrowserLiveTabContext::GetActiveLiveTab() const {
  return sessions::ContentLiveTab::GetForWebContents(
      browser_->tab_strip_model()->GetActiveWebContents());
}

std::map<std::string, std::string> BrowserLiveTabContext::GetExtraDataForTab(
    int index) const {
  std::map<std::string, std::string> extra_data;

#if defined(TOOLKIT_VIEWS)
  if (IsSideSearchEnabled(browser_->profile())) {
    absl::optional<std::pair<std::string, std::string>> side_search_data =
        side_search::MaybeGetSideSearchTabRestoreData(
            browser_->tab_strip_model()->GetWebContentsAt(index));
    if (side_search_data.has_value())
      extra_data.insert(side_search_data.value());
  }
#endif  // defined(TOOLKIT_VIEWS)

  return extra_data;
}

std::map<std::string, std::string>
BrowserLiveTabContext::GetExtraDataForWindow() const {
  return std::map<std::string, std::string>();
}

absl::optional<tab_groups::TabGroupId> BrowserLiveTabContext::GetTabGroupForTab(
    int index) const {
  return browser_->tab_strip_model()->GetTabGroupForTab(index);
}

const tab_groups::TabGroupVisualData*
BrowserLiveTabContext::GetVisualDataForGroup(
    const tab_groups::TabGroupId& group) const {
  return browser_->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group)
      ->visual_data();
}

bool BrowserLiveTabContext::IsTabPinned(int index) const {
  return browser_->tab_strip_model()->IsTabPinned(index);
}

void BrowserLiveTabContext::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  browser_->tab_strip_model()->group_model()->GetTabGroup(group)->SetVisualData(
      std::move(visual_data));
}

const gfx::Rect BrowserLiveTabContext::GetRestoredBounds() const {
  return browser_->window()->GetRestoredBounds();
}

ui::WindowShowState BrowserLiveTabContext::GetRestoredState() const {
  return browser_->window()->GetRestoredState();
}

std::string BrowserLiveTabContext::GetWorkspace() const {
  return browser_->window()->GetWorkspace();
}

sessions::LiveTab* BrowserLiveTabContext::AddRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    absl::optional<tab_groups::TabGroupId> group,
    const tab_groups::TabGroupVisualData& group_visual_data,
    bool select,
    bool pin,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data,
    const SessionID* tab_id) {
  SessionStorageNamespace* storage_namespace =
      tab_platform_data
          ? static_cast<const sessions::ContentPlatformSpecificTabData*>(
                tab_platform_data)
                ->session_storage_namespace()
          : nullptr;

  TabGroupModel* group_model = browser_->tab_strip_model()->group_model();
  const bool first_tab_in_group = group_model && group.has_value() &&
                                  !group_model->ContainsTabGroup(group.value());

  bool restored_from_closed_tab_cache = false;
  WebContents* web_contents = nullptr;
  if (tab_id) {
    // Try to restore the WebContents from the ClosedTabCache rather than
    // creating it again.
    ClosedTabCache& cache =
        ClosedTabCacheServiceFactory::GetForProfile(browser_->profile())
            ->closed_tab_cache();
    std::unique_ptr<WebContents> wc = cache.RestoreEntry(*tab_id);

    if (wc) {
      // Cache hit.
      restored_from_closed_tab_cache = true;
      web_contents = chrome::AddRestoredTabFromCache(
          std::move(wc), browser_, tab_index, group, select, pin,
          user_agent_override, extra_data);
    }
  }

  if (!restored_from_closed_tab_cache) {
    // Cache miss, ClosedTabCache feature disabled or non-existent |tab_id|.
    web_contents = chrome::AddRestoredTab(
        browser_, navigations, tab_index, selected_navigation, extension_app_id,
        group, select, pin, base::TimeTicks(), storage_namespace,
        user_agent_override, extra_data, false /* from_session_restore */);
  }

  // Record the metrics for restoring closed tabs. Set to true when the tab is
  // restored from closed tab cache and false otherwise.
  UMA_HISTOGRAM_BOOLEAN("Tab.RestoreClosedTab", restored_from_closed_tab_cache);

  // Only update the metadata if the group doesn't already exist since the
  // existing group has the latest metadata, which may have changed from the
  // time the tab was closed.
  if (first_tab_in_group) {
    const tab_groups::TabGroupVisualData new_data(
        group_visual_data.title(), group_visual_data.color(), false);
    group_model->GetTabGroup(group.value())->SetVisualData(new_data);
  }

  if (!restored_from_closed_tab_cache) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
    // The focused tab will be loaded by Browser, and TabLoader will load the
    // rest.
    if (!select) {
      // Regression check: make sure that the tab hasn't started to load
      // immediately.
      DCHECK(web_contents->GetController().NeedsReload());
      DCHECK(!web_contents->IsLoading());
    }
    std::vector<TabLoader::RestoredTab> restored_tabs;
    restored_tabs.emplace_back(web_contents, select, !extension_app_id.empty(),
                               pin, group);
    TabLoader::RestoreTabs(restored_tabs, base::TimeTicks::Now());
#else   // BUILDFLAG(ENABLE_SESSION_SERVICE)
    // Load the tab manually if there is no TabLoader.
    web_contents->GetController().LoadIfNecessary();
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
  }

  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

sessions::LiveTab* BrowserLiveTabContext::ReplaceRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    absl::optional<tab_groups::TabGroupId> group,
    int selected_navigation,
    const std::string& extension_app_id,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data) {
  SessionStorageNamespace* storage_namespace =
      tab_platform_data
          ? static_cast<const sessions::ContentPlatformSpecificTabData*>(
                tab_platform_data)
                ->session_storage_namespace()
          : nullptr;

  WebContents* web_contents = chrome::ReplaceRestoredTab(
      browser_, navigations, selected_navigation, extension_app_id,
      storage_namespace, user_agent_override, extra_data,
      false /* from_session_restore */);
  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

void BrowserLiveTabContext::CloseTab() {
  chrome::CloseTab(browser_);
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::Create(
    Profile* profile,
    sessions::SessionWindow::WindowType type,
    const std::string& app_name,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state,
    const std::string& workspace,
    const std::string& user_title,
    const std::map<std::string, std::string>& extra_data) {
  std::unique_ptr<Browser::CreateParams> create_params;
  if (ShouldCreateAppWindowForAppName(profile, app_name)) {
    // Only trusted app popup windows should ever be restored.
    if (type == sessions::SessionWindow::TYPE_APP_POPUP) {
      create_params = std::make_unique<Browser::CreateParams>(
          Browser::CreateParams::CreateForAppPopup(
              app_name, /*trusted_source=*/true, bounds, profile,
              /*user_gesture=*/true));
    } else {
      create_params = std::make_unique<Browser::CreateParams>(
          Browser::CreateParams::CreateForApp(app_name, /*trusted_source=*/true,
                                              bounds, profile,
                                              /*user_gesture=*/true));
    }
  } else {
    create_params = std::make_unique<Browser::CreateParams>(
        Browser::CreateParams(profile, true));
    create_params->initial_bounds = bounds;
  }

  create_params->initial_show_state = show_state;
  create_params->initial_workspace = workspace;
  create_params->user_title = user_title;
  Browser* browser = Browser::Create(*create_params.get());

  return browser->live_tab_context();
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::FindContextForWebContents(
    const WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithTab(contents);
  return browser && !browser->is_delete_scheduled()
             ? browser->live_tab_context()
             : nullptr;
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::FindContextWithID(
    SessionID desired_id) {
  Browser* browser = chrome::FindBrowserWithID(desired_id);
  return browser && !browser->is_delete_scheduled()
             ? browser->live_tab_context()
             : nullptr;
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::FindContextWithGroup(
    tab_groups::TabGroupId group,
    Profile* profile) {
  Browser* browser = chrome::FindBrowserWithGroup(group, profile);
  return browser && !browser->is_delete_scheduled()
             ? browser->live_tab_context()
             : nullptr;
}
