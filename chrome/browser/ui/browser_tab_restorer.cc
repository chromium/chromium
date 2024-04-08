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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/navigation_entry.h"
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

void OpenSavedTabGroup(sessions::TabRestoreService& tab_restore_service,
                       const sessions::TabRestoreService::Group& group,
                       Browser* browser) {
  tab_groups::SavedTabGroupKeyedService* saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser->profile());
  CHECK(saved_tab_group_service);

  const std::optional<base::Uuid>& saved_id = group.saved_id;
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
  const tab_groups::TabGroupVisualData& visual_data = group.visual_data;

  // Open the group.
  std::optional<tab_groups::TabGroupId> new_group_id =
      saved_tab_group_service->OpenSavedTabGroupInBrowser(browser,
                                                          saved_id.value());
  CHECK(new_group_id.has_value());

  // It could be the case that the current state of the saved group has deviated
  // from what is represented in TabRestoreService. Make sure any tabs that are
  // not in the saved group are added to it.
  std::unordered_set<std::string> urls_in_saved_group =
      GetUrlsInSavedTabGroup(*saved_tab_group_service, saved_id.value());
  for (const std::unique_ptr<sessions::TabRestoreService::Tab>& tab :
       group.tabs) {
    AddMissingTabToGroup(browser, *saved_tab_group_service, saved_id.value(),
                         *tab.get(), &urls_in_saved_group);
  }

  UpdateGroupVisualData(new_group_id.value(), visual_data);

  // Clean up TabRestoreService.
  tab_restore_service.RemoveEntryById(session_id);
}

void OpenSavedTabGroupTab(sessions::TabRestoreService& tab_restore_service,
                          const sessions::TabRestoreService::Tab& tab,
                          Browser* browser) {
  tab_groups::SavedTabGroupKeyedService* saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser->profile());
  CHECK(saved_tab_group_service);

  // This value is copied here since it is used throughout this function.
  std::optional<tab_groups::TabGroupId> group_id = tab.group;

  const bool is_group_saved =
      group_id.has_value() && tab.saved_id.has_value() &&
      saved_tab_group_service->model()->Contains(tab.saved_id.value());
  if (!is_group_saved) {
    // Copy these values so they are not overwritten when we make calls to the
    // TabRestoreService that will update its list of entries.
    std::optional<base::Uuid> saved_id = tab.saved_id;
    std::optional<tab_groups::TabGroupVisualData> visual_data =
        tab.group_visual_data;

    // If the tab is not in a group or has not been saved restore it normally.
    tab_restore_service.RestoreMostRecentEntry(browser->live_tab_context());

    if (!saved_tab_group_service->model()->Contains(group_id.value())) {
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
  const std::optional<base::Uuid>& saved_id = tab.saved_id;
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
    int index = browser->tab_strip_model()
                    ->group_model()
                    ->GetTabGroup(group_id.value())
                    ->GetFirstTab()
                    .value();
    browser->tab_strip_model()->AddToExistingGroup(
        {index}, saved_group->local_group_id().value(), /*add_to_end=*/true);
    return;
  }

  // Open the group.
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
        OpenSavedTabGroupTab(*service,
                             static_cast<sessions::TabRestoreService::Tab&>(
                                 *most_recent_entry.get()),
                             browser);
        return;
      }
      case sessions::TabRestoreService::WINDOW: {
        // TODO(dljames): Handle Window Entries. Restore windows normally for
        // now.
        service->RestoreMostRecentEntry(browser->live_tab_context());
        return;
      }
      case sessions::TabRestoreService::GROUP: {
        OpenSavedTabGroup(*service,
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
