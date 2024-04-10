// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/supports_user_data.h"
#include "base/uuid.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace chrome {
namespace {

const char kBrowserTabRestorerKey[] = "BrowserTabRestorer";

// BrowserTabRestorer is responsible for restoring a tab when the
// sessions::TabRestoreService finishes loading. A TabRestoreService is
// associated with a
// single Browser and deletes itself if the Browser is destroyed.
// BrowserTabRestorer is installed on the Profile (by way of user data), only
// one instance is created per profile at a time.
class BrowserTabRestorer : public sessions::TabRestoreServiceObserver,
                           public BrowserListObserver,
                           public base::SupportsUserData::Data {
 public:
  BrowserTabRestorer(const BrowserTabRestorer&) = delete;
  BrowserTabRestorer& operator=(const BrowserTabRestorer&) = delete;

  ~BrowserTabRestorer() override;

  static void CreateIfNecessary(Browser* browser);

 private:
  explicit BrowserTabRestorer(Browser* browser);

  // TabRestoreServiceObserver:
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  raw_ptr<Browser> browser_;
  raw_ptr<sessions::TabRestoreService> tab_restore_service_;
};

BrowserTabRestorer::~BrowserTabRestorer() {
  tab_restore_service_->RemoveObserver(this);
  BrowserList::RemoveObserver(this);
}

// static
void BrowserTabRestorer::CreateIfNecessary(Browser* browser) {
  DCHECK(browser);
  if (browser->profile()->GetUserData(kBrowserTabRestorerKey))
    return;  // Only allow one restore for a given profile at a time.

  // BrowserTabRestorer is deleted at the appropriate time.
  new BrowserTabRestorer(browser);
}

BrowserTabRestorer::BrowserTabRestorer(Browser* browser)
    : browser_(browser),
      tab_restore_service_(
          TabRestoreServiceFactory::GetForProfile(browser->profile())) {
  DCHECK(tab_restore_service_);
  DCHECK(!tab_restore_service_->IsLoaded());
  tab_restore_service_->AddObserver(this);
  BrowserList::AddObserver(this);
  browser_->profile()->SetUserData(kBrowserTabRestorerKey,
                                   base::WrapUnique(this));
  tab_restore_service_->LoadTabsFromLastSession();
}

void BrowserTabRestorer::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {}

void BrowserTabRestorer::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  RestoreTab(browser_);
  // This deletes us.
  browser_->profile()->SetUserData(kBrowserTabRestorerKey, nullptr);
}

void BrowserTabRestorer::OnBrowserRemoved(Browser* browser) {
  // This deletes us.
  browser_->profile()->SetUserData(kBrowserTabRestorerKey, nullptr);
}

std::unordered_set<std::string> GetUrlsInSavedTabGroup(
    tab_groups::SavedTabGroupKeyedService& saved_tab_group_service,
    const base::Uuid& saved_id) {
  const tab_groups::SavedTabGroup* const saved_group =
      saved_tab_group_service.model()->Get(saved_id);
  CHECK(saved_group);

  std::unordered_set<std::string> saved_urls;
  for (const tab_groups::SavedTabGroupTab& saved_tab :
       saved_group->saved_tabs()) {
    if (!saved_urls.contains(saved_tab.url().spec())) {
      saved_urls.emplace(saved_tab.url().spec());
    }
  }

  return saved_urls;
}

content::WebContents* OpenTabWithNavigationStack(
    Browser* browser,
    const sessions::TabRestoreService::Tab& restored_tab) {
  const sessions::SerializedNavigationEntry& entry =
      restored_tab.navigations.at(restored_tab.normalized_navigation_index());
  const GURL tab_url = entry.virtual_url();

  content::WebContents* created_contents = created_contents =
      tab_groups::SavedTabGroupUtils::OpenTabInBrowser(
          tab_url, browser, browser->profile(),
          WindowOpenDisposition::NEW_BACKGROUND_TAB);

  std::vector<std::unique_ptr<content::NavigationEntry>> entries =
      sessions::ContentSerializedNavigationBuilder::ToNavigationEntries(
          restored_tab.navigations, browser->profile());
  created_contents->GetController().Restore(
      restored_tab.normalized_navigation_index(),
      content::RestoreType::kRestored, &entries);
  CHECK_EQ(0u, entries.size());

  if (restored_tab.pinned) {
    browser->tab_strip_model()->SetTabPinned(
        browser->tab_strip_model()->GetIndexOfWebContents(created_contents),
        /*pinned=*/true);
  }

  return created_contents;
}

// Adds a restored tab to the saved group if its URL does not exist in the
// group.
void AddMissingTabToGroup(
    Browser* const browser,
    tab_groups::SavedTabGroupKeyedService& saved_tab_group_service,
    const base::Uuid& saved_id,
    const sessions::TabRestoreService::Tab& restored_tab,
    std::unordered_set<std::string>* const saved_urls) {
  const tab_groups::SavedTabGroup* const saved_group =
      saved_tab_group_service.model()->Get(saved_id);
  CHECK(saved_group);

  const sessions::SerializedNavigationEntry& entry =
      restored_tab.navigations.at(restored_tab.normalized_navigation_index());
  const GURL tab_url = entry.virtual_url();
  if (!saved_urls->contains(tab_url.spec())) {
    // Restore the tab with is navigation stack.
    content::WebContents* created_contents =
        OpenTabWithNavigationStack(browser, restored_tab);

    // Add the tab to the correct group.
    int index =
        browser->tab_strip_model()->GetIndexOfWebContents(created_contents);
    browser->tab_strip_model()->AddToGroupForRestore(
        {index}, saved_group->local_group_id().value());
    saved_urls->emplace(tab_url.spec());
  }
}

void UpdateGroupVisualData(const tab_groups::TabGroupId& group_id,
                           const tab_groups::TabGroupVisualData& visual_data) {
  TabGroup* const tab_group =
      tab_groups::SavedTabGroupUtils::GetTabGroupWithId(group_id);
  CHECK(tab_group);
  tab_group->SetVisualData(visual_data);
}

void OpenSavedTabGroupAndAddRestoredTabs(
    Browser* browser,
    const sessions::TabRestoreService::Group& group,
    tab_groups::SavedTabGroupKeyedService& saved_tab_group_service) {
  // service, saved id, group
  std::optional<tab_groups::TabGroupId> new_group_id =
      saved_tab_group_service.OpenSavedTabGroupInBrowser(
          browser, group.saved_group_id.value());
  CHECK(new_group_id.has_value());

  // It could be the case that the current state of the saved group has
  // deviated from what is represented in TabRestoreService. Make sure any
  // tabs that are not in the saved group are added to it.
  std::unordered_set<std::string> urls_in_saved_group = GetUrlsInSavedTabGroup(
      saved_tab_group_service, group.saved_group_id.value());
  for (const std::unique_ptr<sessions::TabRestoreService::Tab>& grouped_tab :
       group.tabs) {
    AddMissingTabToGroup(browser, saved_tab_group_service,
                         group.saved_group_id.value(), *grouped_tab.get(),
                         &urls_in_saved_group);
  }

  UpdateGroupVisualData(new_group_id.value(), group.visual_data);
}

void OpenTabGroup(sessions::TabRestoreService& tab_restore_service,
                  const sessions::TabRestoreService::Group& group,
                  Browser* browser) {
  tab_groups::SavedTabGroupKeyedService* saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser->profile());
  CHECK(saved_tab_group_service);

  const std::optional<base::Uuid>& saved_id = group.saved_group_id;
  const bool is_group_saved =
      saved_id.has_value() &&
      saved_tab_group_service->model()->Contains(saved_id.value());
  if (!is_group_saved) {
    // Copy these values so they are not overwritten when we remove the entry
    // from TabRestoreService .
    tab_groups::TabGroupId group_id = group.group_id;
    tab_groups::TabGroupVisualData visual_data = group.visual_data;

    // If the group is not saved, restore it normally, and save it.
    tab_restore_service.RestoreMostRecentEntry(browser->live_tab_context());
    saved_tab_group_service->SaveGroup(group_id);

    // Update the color and title of the group appropriately.
    UpdateGroupVisualData(group_id, visual_data);
    return;
  }

  const SessionID& session_id = group.id;
  OpenSavedTabGroupAndAddRestoredTabs(browser, group, *saved_tab_group_service);

  // Clean up TabRestoreService.
  tab_restore_service.RemoveEntryById(session_id);
}

void OpenTab(sessions::TabRestoreService& tab_restore_service,
             const sessions::TabRestoreService::Tab& tab,
             Browser* browser) {
  tab_groups::SavedTabGroupKeyedService* saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser->profile());
  CHECK(saved_tab_group_service);

  // This value is copied here since it is used throughout this function.
  std::optional<tab_groups::TabGroupId> group_id = tab.group;

  const bool is_group_saved =
      group_id.has_value() && tab.saved_group_id.has_value() &&
      saved_tab_group_service->model()->Contains(tab.saved_group_id.value());
  if (!is_group_saved) {
    // Copy these values so they are not overwritten when we make calls to the
    // TabRestoreService that will update its list of entries.
    std::optional<base::Uuid> saved_id = tab.saved_group_id;
    std::optional<tab_groups::TabGroupVisualData> visual_data =
        tab.group_visual_data;

    // If the tab is not in a group or has not been saved restore it normally.
    tab_restore_service.RestoreMostRecentEntry(browser->live_tab_context());

    if (group_id.has_value() &&
        !saved_tab_group_service->model()->Contains(group_id.value())) {
      // Save the group if it isn't already saved.
      saved_tab_group_service->SaveGroup(group_id.value());
    }

    // Update the color and title of the group appropriately.
    if (visual_data.has_value()) {
      UpdateGroupVisualData(group_id.value(), visual_data.value());
    }

    return;
  }

  const SessionID& session_id = tab.id;
  const std::optional<base::Uuid>& saved_id = tab.saved_group_id;
  const std::optional<tab_groups::TabGroupVisualData>& visual_data =
      tab.group_visual_data;

  const tab_groups::SavedTabGroup* const saved_group =
      saved_tab_group_service->model()->Get(saved_id.value());
  if (saved_group->local_group_id().has_value()) {
    // If the group is open already, restore the tab normally.
    tab_restore_service.RestoreMostRecentEntry(browser->live_tab_context());

    // Move the tab into the correct group. This happens in cases where the
    // original group id was regenerated (such as when calling
    // SavedTabGroupKeyedService::OpenSavedTabGroupInBrowser).
    int index = browser->tab_strip_model()->active_index();
    browser->tab_strip_model()->AddToExistingGroup(
        {index}, saved_group->local_group_id().value(), /*add_to_end=*/true);
    return;
  }

  std::optional<tab_groups::TabGroupId> new_group_id =
      saved_tab_group_service->OpenSavedTabGroupInBrowser(browser,
                                                          saved_id.value());
  CHECK(new_group_id.has_value());

  // It could be the case that the current state of the saved group has deviated
  // from what is represented in TabRestoreService. Make sure any tabs that are
  // not in the saved group are added to it.
  std::unordered_set<std::string> urls_in_saved_group =
      GetUrlsInSavedTabGroup(*saved_tab_group_service, saved_id.value());
  AddMissingTabToGroup(browser, *saved_tab_group_service, saved_id.value(), tab,
                       &urls_in_saved_group);

  if (visual_data.has_value()) {
    UpdateGroupVisualData(new_group_id.value(), visual_data.value());
  }

  // Clean up TabRestoreService.
  tab_restore_service.RemoveEntryById(session_id);
}

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

Browser* CreateBrowserWindow(
    Profile* profile,
    const sessions::TabRestoreService::Window& window) {
  std::unique_ptr<Browser::CreateParams> create_params;
  if (ShouldCreateAppWindowForAppName(profile, window.app_name)) {
    // Only trusted app popup windows should ever be restored.
    if (window.type == sessions::SessionWindow::TYPE_APP_POPUP) {
      create_params = std::make_unique<Browser::CreateParams>(
          Browser::CreateParams::CreateForAppPopup(
              window.app_name, /*trusted_source=*/true, window.bounds, profile,
              /*user_gesture=*/true));
    } else {
      create_params = std::make_unique<Browser::CreateParams>(
          Browser::CreateParams::CreateForApp(
              window.app_name, /*trusted_source=*/true, window.bounds, profile,
              /*user_gesture=*/true));
    }
  } else {
    create_params = std::make_unique<Browser::CreateParams>(
        Browser::CreateParams(profile, true));
    create_params->initial_bounds = window.bounds;
  }

  create_params->initial_show_state = window.show_state;
  create_params->initial_workspace = window.workspace;
  create_params->user_title = window.user_title;

  return Browser::Create(*create_params.get());
}

void RecreateAndSaveTabGroup(
    Browser* browser,
    const sessions::TabRestoreService::Group& group,
    tab_groups::SavedTabGroupKeyedService& saved_tab_group_service) {
  // If the group is not saved:
  //  0. Generate a new tab group id to avoid conflicts.
  //  1. Open all of the tabs in the new browser window.
  //  2. Add all of the tabs to a new tab group using |new_id|.
  //  3. Save the group.
  tab_groups::TabGroupId new_id = tab_groups::TabGroupId::GenerateNew();
  std::vector<int> indices_of_tabs;
  for (const std::unique_ptr<sessions::TabRestoreService::Tab>& tab :
       group.tabs) {
    content::WebContents* opened_tab =
        OpenTabWithNavigationStack(browser, *tab.get());
    int index = browser->tab_strip_model()->GetIndexOfWebContents(opened_tab);
    indices_of_tabs.emplace_back(index);
  }

  browser->tab_strip_model()->AddToGroupForRestore(indices_of_tabs, new_id);
  saved_tab_group_service.SaveGroup(new_id);
  UpdateGroupVisualData(new_id, group.visual_data);
}

void OpenWindow(sessions::TabRestoreService& tab_restore_service,
                const sessions::TabRestoreService::Window& window,
                Browser* browser) {
  tab_groups::SavedTabGroupKeyedService* saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser->profile());
  CHECK(saved_tab_group_service);

  std::unordered_set<std::string> seen_groups;

  // This should only be created when we actually need to open a new window.
  Browser* new_browser = nullptr;

  for (const std::unique_ptr<sessions::TabRestoreService::Tab>& tab :
       window.tabs) {
    if (!tab->group.has_value()) {
      if (!new_browser) {
        // Create a new browser window.
        new_browser = CreateBrowserWindow(browser->profile(), window);
      }
      OpenTabWithNavigationStack(new_browser, *tab.get());
      continue;
    }

    // Skip this group if we have already processed it.
    if (seen_groups.contains(tab->group.value().ToString())) {
      continue;
    }

    // Process all of the tabs in this group.
    seen_groups.emplace(tab->group.value().ToString());

    const std::unique_ptr<sessions::TabRestoreService::Group>& group =
        window.groups.at(tab->group.value());
    const tab_groups::SavedTabGroup* saved_group =
        group->saved_group_id.has_value()
            ? saved_tab_group_service->model()->Get(
                  group->saved_group_id.value())
            : nullptr;

    if ((!saved_group || !saved_group->local_group_id().has_value()) &&
        !new_browser) {
      // Create a new browser window if we haven't already if:
      // 1. The group was not saved and should be opened in the new window.
      // 2. The group is not open and should be reopened in the new window.
      new_browser = CreateBrowserWindow(browser->profile(), window);
    }

    if (!saved_group) {
      // If the group is not saved:
      // 1. Restore normally it
      // 2. Save it.
      RecreateAndSaveTabGroup(new_browser, *group.get(),
                              *saved_tab_group_service);
    } else {
      // If the group is saved:
      // 1. Find the browser the group should go in.
      // 2. Open the group in the browser.
      // 3. Add tabs from restore to the saved group if they do not exist.
      Browser* groups_browser =
          saved_group->local_group_id().has_value()
              ? tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
                    saved_group->local_group_id().value())
              : new_browser;
      CHECK(groups_browser);
      OpenSavedTabGroupAndAddRestoredTabs(groups_browser, *group.get(),
                                          *saved_tab_group_service);

      groups_browser->window()->Show();
    }
  }

  if (new_browser) {
    new_browser->window()->Show();
  }

  tab_restore_service.RemoveEntryById(window.id);
}

}  // namespace

void RestoreTab(Browser* browser) {
  base::RecordAction(base::UserMetricsAction("RestoreTab"));

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  if (!service) {
    return;
  }

  if (service->IsLoaded()) {
    if (!tab_groups::IsTabGroupsSaveV2Enabled() || service->entries().empty()) {
      // Restore normally.
      service->RestoreMostRecentEntry(browser->live_tab_context());
      return;
    }

    const std::unique_ptr<sessions::TabRestoreService::Entry>&
        most_recent_entry = service->entries().front();
    switch (most_recent_entry->type) {
      case sessions::TabRestoreService::TAB: {
        OpenTab(*service,
                static_cast<sessions::TabRestoreService::Tab&>(
                    *most_recent_entry.get()),
                browser);
        return;
      }
      case sessions::TabRestoreService::WINDOW: {
        OpenWindow(*service,
                   static_cast<sessions::TabRestoreService::Window&>(
                       *most_recent_entry.get()),
                   browser);
        return;
      }
      case sessions::TabRestoreService::GROUP: {
        OpenTabGroup(*service,
                     static_cast<sessions::TabRestoreService::Group&>(
                         *most_recent_entry.get()),
                     browser);
        return;
      }
    }

    NOTREACHED();
  }

  BrowserTabRestorer::CreateIfNecessary(browser);
}

}  // namespace chrome
