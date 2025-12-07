// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_live_tab_context.h"

#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/token.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/performance_manager/public/background_tab_loading_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/buildflags.h"
#include "components/performance_manager/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/content/content_platform_specific_tab_data.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/tab_loader.h"
#endif

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
  if (app_name.empty()) {
    return false;
  }

  // Only need to check that the app is installed if |app_name| is for a
  // platform app or web app. (|app_name| could also be for a devtools window.)
  const std::string app_id = web_app::GetAppIdFromApplicationName(app_name);
  if (app_id.empty()) {
    return true;
  }

  return apps::IsInstalledApp(profile, app_id);
}

sessions::LiveTabContext* GetLiveTabContext(Browser* browser) {
  return browser && !browser->is_delete_scheduled()
             ? browser->GetFeatures().live_tab_context()
             : nullptr;
}

}  // namespace

BrowserLiveTabContext::BrowserLiveTabContext(BrowserWindowInterface* browser,
                                             TabStripModel* tab_strip_model,
                                             Profile* profile,
                                             ui::BaseWindow* base_window,
                                             BrowserWindowInterface::Type type,
                                             const std::string& app_name,
                                             SessionID session_id)
    : browser_(CHECK_DEREF(browser)),
      tab_strip_model_(CHECK_DEREF(tab_strip_model)),
      profile_(CHECK_DEREF(profile)),
      base_window_(CHECK_DEREF(base_window)),
      window_type_(WindowTypeForBrowserType(type)),
      app_name_(app_name),
      session_id_(session_id) {}

BrowserLiveTabContext::~BrowserLiveTabContext() {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(&profile_.get());
  if (tab_restore_service) {
    tab_restore_service->BrowserClosed(this);
  }
}

void BrowserLiveTabContext::ShowBrowserWindow() {
  base_window_->Show();
}

SessionID BrowserLiveTabContext::GetSessionID() const {
  return session_id_;
}

sessions::SessionWindow::WindowType BrowserLiveTabContext::GetWindowType()
    const {
  return window_type_;
}

int BrowserLiveTabContext::GetTabCount() const {
  return tab_strip_model_->count();
}

int BrowserLiveTabContext::GetSelectedIndex() const {
  return tab_strip_model_->active_index();
}

std::string BrowserLiveTabContext::GetAppName() const {
  return app_name_;
}

std::string BrowserLiveTabContext::GetUserTitle() const {
  return browser_->GetBrowserForMigrationOnly()->user_title();
}

sessions::LiveTab* BrowserLiveTabContext::GetLiveTabAt(int index) const {
  return sessions::ContentLiveTab::GetForWebContents(
      tab_strip_model_->GetWebContentsAt(index));
}

sessions::LiveTab* BrowserLiveTabContext::GetActiveLiveTab() const {
  return sessions::ContentLiveTab::GetForWebContents(
      tab_strip_model_->GetActiveWebContents());
}

std::map<std::string, std::string> BrowserLiveTabContext::GetExtraDataForTab(
    int index) const {
  return std::map<std::string, std::string>();
}

std::map<std::string, std::string>
BrowserLiveTabContext::GetExtraDataForWindow() const {
  std::map<std::string, std::string> data;

  if (tabs::IsVerticalTabsFeatureEnabled()) {
    auto* controller =
        browser_->GetFeatures().vertical_tab_strip_state_controller();
    if (controller) {
      data[tabs::VerticalTabStripStateController::kCollapsedKey] =
          base::ToString(controller->IsCollapsed());
      data[tabs::VerticalTabStripStateController::kUncollapsedWidthKey] =
          base::NumberToString(controller->GetUncollapsedWidth());
    }
  }

  return data;
}

std::optional<tab_groups::TabGroupId> BrowserLiveTabContext::GetTabGroupForTab(
    int index) const {
  return tab_strip_model_->GetTabGroupForTab(index);
}

const tab_groups::TabGroupVisualData*
BrowserLiveTabContext::GetVisualDataForGroup(
    const tab_groups::TabGroupId& group) const {
  TabGroupModel* group_model = tab_strip_model_->group_model();
  CHECK(group_model);
  TabGroup* tab_group = group_model->GetTabGroup(group);
  CHECK(tab_group);
  return tab_group->visual_data();
}

const std::optional<base::Uuid>
BrowserLiveTabContext::GetSavedTabGroupIdForGroup(
    const tab_groups::TabGroupId& group) const {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(&profile_.get());
  CHECK(tab_group_service);

  const std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_service->GetGroup(group);

  return saved_group ? std::make_optional(saved_group->saved_guid())
                     : std::nullopt;
}

bool BrowserLiveTabContext::IsTabPinned(int index) const {
  return tab_strip_model_->IsTabPinned(index);
}

void BrowserLiveTabContext::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  TabGroupModel* group_model = tab_strip_model_->group_model();
  CHECK(group_model);
  CHECK(group_model->ContainsTabGroup(group));
  tab_strip_model_->ChangeTabGroupVisuals(group, std::move(visual_data));
}

const gfx::Rect BrowserLiveTabContext::GetRestoredBounds() const {
  return base_window_->GetRestoredBounds();
}

ui::mojom::WindowShowState BrowserLiveTabContext::GetRestoredState() const {
  return base_window_->GetRestoredState();
}

std::string BrowserLiveTabContext::GetWorkspace() const {
  return browser_->GetBrowserForMigrationOnly()->window()->GetWorkspace();
}

sessions::LiveTab* BrowserLiveTabContext::AddRestoredTab(
    const sessions::tab_restore::Tab& tab,
    int tab_index,
    bool select,
    bool is_restoring_group_or_window,
    sessions::tab_restore::Type original_session_type) {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(&profile_.get());
  CHECK(tab_group_service);

  SessionStorageNamespace* storage_namespace =
      tab.platform_data
          ? static_cast<const sessions::ContentPlatformSpecificTabData*>(
                tab.platform_data.get())
                ->session_storage_namespace()
          : nullptr;

  // If the browser does not support tabs groups, restore the grouped tab as a
  // normal tab instead. See crbug.com/368139715.
  std::optional<tab_groups::TabGroupId> group_id =
      tab_strip_model_->SupportsTabGroups() ? tab.group : std::nullopt;
  std::optional<base::Uuid> saved_group_id = tab.saved_group_id;
  content::WebContents* web_contents = nullptr;

  Browser* const browser = browser_->GetBrowserForMigrationOnly();
  const bool is_normal_tab = !group_id.has_value();
  const bool is_grouped_tab_unsaved =
      group_id.has_value() && !saved_group_id.has_value();
  const bool group_deleted_from_model =
      group_id.has_value() && saved_group_id.has_value() &&
      !tab_group_service->GetGroup(saved_group_id.value()).has_value();
  if (is_normal_tab || is_grouped_tab_unsaved || group_deleted_from_model) {
    // Add the tab to the browser.
    web_contents = chrome::AddRestoredTab(
        browser, tab.navigations, tab_index, tab.normalized_navigation_index(),
        tab.extension_app_id, group_id, select, tab.pinned, base::TimeTicks(),
        base::Time(), storage_namespace, tab.user_agent_override,
        tab.extra_data,
        /*from_session_restore=*/false, /*is_active_browser=*/std::nullopt);

    if (group_id.has_value() &&
        !tab_group_service->GetGroup(group_id.value()).has_value()) {
      // It's possible a tab's group was deleted or was unsaved before this tab
      // was restored. In that case, if the local group didn't become saved add
      // the visual metadata and save it manually.
      browser->GetFeatures().live_tab_context()->SetVisualDataForGroup(
          group_id.value(), tab.group_visual_data.value());
      tab_group_service->SaveGroup(
          tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
              tab.group.value()));
    }
  } else {
    std::optional<tab_groups::SavedTabGroup> saved_group =
        tab_group_service->GetGroup(saved_group_id.value());
    CHECK(saved_group);
    group_id = saved_group->local_group_id();

    if (group_id) {
      Browser* source_browser =
          tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
              group_id.value());
      tab_groups::SavedTabGroupUtils::FocusFirstTabOrWindowInOpenGroup(
          group_id.value());

      // Move the group into `browser` if it is open in a different browser.
      if (source_browser != browser) {
        chrome::MoveGroupToExistingWindow(source_browser, browser,
                                          group_id.value());
      }
    } else {
      // Open the group in this browser if it is closed.
      group_id = tab_group_service->OpenTabGroup(
          saved_group_id.value(),
          std::make_unique<tab_groups::TabGroupActionContextDesktop>(
              browser, tab_groups::OpeningSource::kOpenedFromTabRestore));
    }

    if (is_restoring_group_or_window) {
      // Open the saved tab group as-is if the tab is being restored from a
      // group or window context. This is to enforce that SavedTabGroups are
      // the source or truth.
      return nullptr;
    }

    // Add the saved tab to the end of group.
    web_contents = chrome::AddRestoredTab(
        browser, tab.navigations, tab_strip_model_->count(),
        tab.normalized_navigation_index(), tab.extension_app_id, group_id,
        select, tab.pinned, base::TimeTicks(), base::Time(), storage_namespace,
        tab.user_agent_override, tab.extra_data,
        /*from_session_restore=*/false, /*is_active_browser=*/std::nullopt);
  }

  CHECK(web_contents);

  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kBackgroundTabLoadingFromPerformanceManager)) {
    if (performance_manager::policies::CanScheduleLoadForRestoredTabs()) {
      performance_manager::policies::ScheduleLoadForRestoredTabs(
          {web_contents});
    } else {
      // Load the tab manually if there's no BackgroundTabLoadingPolicy.
      web_contents->GetController().LoadIfNecessary();
    }
    return sessions::ContentLiveTab::GetForWebContents(web_contents);
  }

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // The tab may have been made active even if `select` is false if it is the
  // only tab in `tab_strip_model_`.
  const bool is_active =
      tab_strip_model_->GetActiveWebContents() == web_contents;
  // The active tab will be loaded by Browser, and TabLoader will load the rest.
  if (!is_active) {
    // Regression check: make sure that the tab hasn't started to load
    // immediately.
    DCHECK(web_contents->GetController().NeedsReload());
    DCHECK(!web_contents->IsLoading());
  }

  std::vector<TabLoader::RestoredTab> restored_tabs;
  restored_tabs.emplace_back(web_contents, is_active,
                             !tab.extension_app_id.empty(), tab.pinned,
                             group_id, std::nullopt);
  TabLoader::DeprecatedRestoreTabs(restored_tabs, base::TimeTicks::Now());

#else   // BUILDFLAG(ENABLE_SESSION_SERVICE)
  // Load the tab manually if there is no TabLoader.
  web_contents->GetController().LoadIfNecessary();
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

sessions::LiveTab* BrowserLiveTabContext::ReplaceRestoredTab(
    const sessions::tab_restore::Tab& tab) {
  const sessions::tab_restore::PlatformSpecificTabData* tab_platform_data =
      tab.platform_data.get();
  SessionStorageNamespace* storage_namespace =
      tab_platform_data
          ? static_cast<const sessions::ContentPlatformSpecificTabData*>(
                tab_platform_data)
                ->session_storage_namespace()
          : nullptr;

  WebContents* web_contents = chrome::ReplaceRestoredTab(
      browser_->GetBrowserForMigrationOnly(), tab.navigations,
      tab.normalized_navigation_index(), tab.extension_app_id,
      storage_namespace, tab.user_agent_override, tab.extra_data,
      false /* from_session_restore */);
  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

void BrowserLiveTabContext::CloseTab() {
  chrome::CloseTab(browser_->GetBrowserForMigrationOnly());
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::Create(
    Profile* profile,
    sessions::SessionWindow::WindowType type,
    const std::string& app_name,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state,
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

  if (tabs::IsVerticalTabsFeatureEnabled()) {
    if (extra_data.contains(
            tabs::VerticalTabStripStateController::kCollapsedKey)) {
      create_params->vertical_tab_strip_collapsed =
          extra_data.at(tabs::VerticalTabStripStateController::kCollapsedKey) ==
          "true";
    }

    if (extra_data.contains(
            tabs::VerticalTabStripStateController::kUncollapsedWidthKey)) {
      int uncollapsed_width = 0;
      if (base::StringToInt(
              extra_data.at(
                  tabs::VerticalTabStripStateController::kUncollapsedWidthKey),
              &uncollapsed_width)) {
        create_params->vertical_tab_strip_uncollapsed_width = uncollapsed_width;
      }
    }
  }

  Browser* browser = Browser::Create(*create_params.get());

  return browser->GetFeatures().live_tab_context();
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::FindContextForWebContents(
    const WebContents* contents) {
  Browser* const browser = chrome::FindBrowserWithTab(contents);
  return GetLiveTabContext(browser);
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::FindContextWithID(
    SessionID desired_id) {
  Browser* const browser = chrome::FindBrowserWithID(desired_id);
  return GetLiveTabContext(browser);
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::FindContextWithGroup(
    tab_groups::TabGroupId group,
    Profile* profile) {
  Browser* const browser = chrome::FindBrowserWithGroup(group, profile);
  return GetLiveTabContext(browser);
}
