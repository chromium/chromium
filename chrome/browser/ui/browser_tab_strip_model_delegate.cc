// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_switches.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ipc/ipc_message.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/range/range.h"

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, public:

BrowserTabStripModelDelegate::BrowserTabStripModelDelegate(Browser* browser)
    : browser_(browser) {}

BrowserTabStripModelDelegate::~BrowserTabStripModelDelegate() = default;

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, TabStripModelDelegate implementation:

void BrowserTabStripModelDelegate::AddTabAt(
    const GURL& url,
    int index,
    bool foreground,
    std::optional<tab_groups::TabGroupId> group) {
  chrome::AddTabAt(browser_, url, index, foreground, group);
}

Browser* BrowserTabStripModelDelegate::CreateNewStripWithTabs(
    std::vector<NewStripContents> tabs,
    const gfx::Rect& window_bounds,
    bool maximize) {
  DCHECK(browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP));

  // Create an empty new browser window the same size as the old one.
  Browser::CreateParams params(browser_->profile(), true);
  params.initial_bounds = window_bounds;
  params.initial_show_state = maximize ? ui::mojom::WindowShowState::kMaximized
                                       : ui::mojom::WindowShowState::kNormal;
  Browser* browser = Browser::Create(params);
  TabStripModel* new_model = browser->tab_strip_model();

  for (size_t i = 0; i < tabs.size(); ++i) {
    NewStripContents item = std::move(tabs[i]);

    // Enforce that there is an active tab in the strip at all times by forcing
    // the first web contents to be marked as active.
    if (i == 0)
      item.add_types |= AddTabTypes::ADD_ACTIVE;

    content::WebContents* const raw_web_contents = item.tab.get()->contents();
    new_model->InsertDetachedTabAt(static_cast<int>(i), std::move(item.tab),
                                   item.add_types);
    // Make sure the loading state is updated correctly, otherwise the throbber
    // won't start if the page is loading.
    // TODO(beng): find a better way of doing this.
    static_cast<content::WebContentsDelegate*>(browser)->LoadingStateChanged(
        raw_web_contents, true);
  }

  return browser;
}

void BrowserTabStripModelDelegate::WillAddWebContents(
    content::WebContents* contents) {
  TabHelpers::AttachTabHelpers(contents);

  // Make the tab show up in the task manager.
  task_manager::WebContentsTags::CreateForTabContents(contents);
}

int BrowserTabStripModelDelegate::GetDragActions() const {
  return TabStripModelDelegate::TAB_TEAROFF_ACTION |
         (browser_->tab_strip_model()->count() > 1
              ? TabStripModelDelegate::TAB_MOVE_ACTION
              : 0);
}

bool BrowserTabStripModelDelegate::CanDuplicateContentsAt(int index) {
  return CanDuplicateTabAt(browser_, index);
}

bool BrowserTabStripModelDelegate::IsTabStripEditable() {
  return browser_->window()->IsTabStripEditable();
}

void BrowserTabStripModelDelegate::DuplicateContentsAt(int index) {
  DuplicateTabAt(browser_, index);
}

void BrowserTabStripModelDelegate::MoveToExistingWindow(
    const std::vector<int>& indices,
    int browser_index) {
  std::vector<Browser*> existing_browsers =
      browser_->tab_menu_model_delegate()->GetOtherBrowserWindows(
          web_app::AppBrowserController::IsWebApp(browser_));
  size_t existing_browser_count = existing_browsers.size();
  if (static_cast<size_t>(browser_index) < existing_browser_count &&
      existing_browsers[browser_index]) {
    chrome::MoveTabsToExistingWindow(browser_, existing_browsers[browser_index],
                                     indices);
  }
}

bool BrowserTabStripModelDelegate::CanMoveTabsToWindow(
    const std::vector<int>& indices) {
  return CanMoveTabsToNewWindow(browser_, indices);
}

void BrowserTabStripModelDelegate::MoveTabsToNewWindow(
    const std::vector<int>& indices) {
  // chrome:: to disambiguate the free function from this method.
  chrome::MoveTabsToNewWindow(browser_, indices);
}

void BrowserTabStripModelDelegate::MoveGroupToNewWindow(
    const tab_groups::TabGroupId& group) {
  TabGroupModel* group_model = browser_->tab_strip_model()->group_model();
  if (!group_model)
    return;

  gfx::Range range = group_model->GetTabGroup(group)->ListTabs();

  std::vector<int> indices;
  indices.reserve(range.length());
  for (auto i = range.start(); i < range.end(); ++i)
    indices.push_back(i);

  // chrome:: to disambiguate the free function from
  // BrowserTabStripModelDelegate::MoveTabsToNewWindow().
  chrome::MoveTabsToNewWindow(browser_, indices, group);
}

std::optional<SessionID> BrowserTabStripModelDelegate::CreateHistoricalTab(
    content::WebContents* contents) {
  if (!BrowserSupportsHistoricalEntries())
    return std::nullopt;

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());

  // We only create historical tab entries for tabbed browser windows.
  if (service && browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    return service->CreateHistoricalTab(
        sessions::ContentLiveTab::GetForWebContents(contents),
        browser_->tab_strip_model()->GetIndexOfWebContents(contents));
  }
  return std::nullopt;
}

void BrowserTabStripModelDelegate::CreateHistoricalGroup(
    const tab_groups::TabGroupId& group) {
  if (!BrowserSupportsHistoricalEntries())
    return;

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service) {
    service->CreateHistoricalGroup(
        BrowserLiveTabContext::FindContextWithGroup(group, browser_->profile()),
        group);
  }
}

void BrowserTabStripModelDelegate::GroupAdded(
    const tab_groups::TabGroupId& group) {
  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    return;
  }

  if (!tab_groups::IsTabGroupsSaveV2Enabled()) {
    return;
  }

  tab_groups::SavedTabGroupKeyedService* saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser_->profile());
  if (!saved_tab_group_service) {
    return;
  }

  if (saved_tab_group_service->model()->Contains(group)) {
    return;
  }

  saved_tab_group_service->SaveGroup(
      group,
      /*is_pinned=*/tab_groups::SavedTabGroupUtils::ShouldAutoPinNewTabGroups(
          browser_->profile()));
}

void BrowserTabStripModelDelegate::WillCloseGroup(
    const tab_groups::TabGroupId& group) {
  // First the saved group must be stored in tab restore so that it keeps the
  // SavedTabGroup/TabIDs
  CreateHistoricalGroup(group);

  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    return;
  }

  // When closing, the group should stay available in revisit UIs so disconnect
  // the group to prevent deletion.
  tab_groups::SavedTabGroupKeyedService* saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser_->profile());

  if (saved_tab_group_service &&
      saved_tab_group_service->model()->Contains(group)) {
    saved_tab_group_service->DisconnectLocalTabGroup(group);
  }
}

void BrowserTabStripModelDelegate::GroupCloseStopped(
    const tab_groups::TabGroupId& group) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service)
    service->GroupCloseStopped(group);
}

bool BrowserTabStripModelDelegate::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return browser_->RunUnloadListenerBeforeClosing(contents);
}

bool BrowserTabStripModelDelegate::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return browser_->ShouldRunUnloadListenerBeforeClosing(contents);
}

bool BrowserTabStripModelDelegate::ShouldDisplayFavicon(
    content::WebContents* contents) const {
  // Don't show favicon when on an interstitial.
  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper = security_interstitials::
          SecurityInterstitialTabHelper::FromWebContents(contents);
  if (security_interstitial_tab_helper &&
      security_interstitial_tab_helper->IsDisplayingInterstitial())
    return false;

  return browser_->ShouldDisplayFavicon(contents);
}

bool BrowserTabStripModelDelegate::CanReload() const {
  return chrome::CanReload(browser_);
}

void BrowserTabStripModelDelegate::AddToReadLater(
    content::WebContents* web_contents) {
  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser_->profile());
  if (!model || !model->loaded())
    return;

  chrome::MoveTabToReadLater(browser_, web_contents);
}

bool BrowserTabStripModelDelegate::SupportsReadLater() {
  return !browser_->profile()->IsGuestSession() && !IsForWebApp();
}

bool BrowserTabStripModelDelegate::IsForWebApp() {
  return web_app::AppBrowserController::IsWebApp(browser_);
}

void BrowserTabStripModelDelegate::CopyURL(content::WebContents* web_contents) {
  chrome::CopyURL(browser_, web_contents);
}

void BrowserTabStripModelDelegate::GoBack(content::WebContents* web_contents) {
  chrome::GoBack(web_contents);
}

bool BrowserTabStripModelDelegate::CanGoBack(
    content::WebContents* web_contents) {
  return chrome::CanGoBack(web_contents);
}

bool BrowserTabStripModelDelegate::IsNormalWindow() {
  return browser_->is_type_normal();
}

BrowserWindowInterface*
BrowserTabStripModelDelegate::GetBrowserWindowInterface() {
  return browser_;
}

void BrowserTabStripModelDelegate::OnGroupsDestruction(
    const std::vector<tab_groups::TabGroupId>& group_ids,
    base::OnceCallback<void()> close_callback,
    bool is_bulk_operation) {
  if (is_bulk_operation && tab_groups::IsTabGroupsSaveV2Enabled()) {
    // If this is a bulk operation, close the groups rather than delete
    // them to retain the saved group.
    for (auto group_id : group_ids) {
      tab_groups::SavedTabGroupUtils::RemoveGroupFromTabstrip(browser_,
                                                              group_id);
    }
    std::move(close_callback).Run();
  } else {
    tab_groups::SavedTabGroupUtils::MaybeShowSavedTabGroupDeletionDialog(
        browser_,
        tab_groups::DeletionDialogController::DialogType::CloseTabAndDelete,
        group_ids, std::move(close_callback));
  }
}

void BrowserTabStripModelDelegate::OnRemovingAllTabsFromGroups(
    const std::vector<tab_groups::TabGroupId>& group_ids,
    base::OnceCallback<void()> callback) {
  tab_groups::SavedTabGroupUtils::MaybeShowSavedTabGroupDeletionDialog(
      browser_,
      tab_groups::DeletionDialogController::DialogType::RemoveTabAndDelete,
      group_ids, std::move(callback));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, private:

void BrowserTabStripModelDelegate::CloseFrame() {
  browser_->window()->Close();
}

bool BrowserTabStripModelDelegate::BrowserSupportsHistoricalEntries() {
  // We don't create historical tabs for incognito windows or windows without
  // profiles.
  return browser_->profile() && !browser_->profile()->IsOffTheRecord();
}

}  // namespace chrome
