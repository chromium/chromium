// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/sessions/closed_tab_cache.h"
#include "chrome/browser/sessions/closed_tab_cache_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_switches.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ipc/ipc_message.h"
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
    absl::optional<tab_groups::TabGroupId> group) {
  chrome::AddTabAt(browser_, url, index, foreground, group);
}

Browser* BrowserTabStripModelDelegate::CreateNewStripWithContents(
    std::vector<NewStripContents> contentses,
    const gfx::Rect& window_bounds,
    bool maximize) {
  DCHECK(browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP));

  // Create an empty new browser window the same size as the old one.
  Browser::CreateParams params(browser_->profile(), true);
  params.initial_bounds = window_bounds;
  params.initial_show_state =
      maximize ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL;
  Browser* browser = Browser::Create(params);
  TabStripModel* new_model = browser->tab_strip_model();

  for (size_t i = 0; i < contentses.size(); ++i) {
    NewStripContents item = std::move(contentses[i]);

    // Enforce that there is an active tab in the strip at all times by forcing
    // the first web contents to be marked as active.
    if (i == 0)
      item.add_types |= AddTabTypes::ADD_ACTIVE;

    content::WebContents* raw_web_contents = item.web_contents.get();
    new_model->InsertWebContentsAt(
        static_cast<int>(i), std::move(item.web_contents), item.add_types);
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

absl::optional<SessionID> BrowserTabStripModelDelegate::CreateHistoricalTab(
    content::WebContents* contents) {
  if (!BrowserSupportsHistoricalEntries())
    return absl::nullopt;

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());

  // We only create historical tab entries for tabbed browser windows.
  if (service && browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    return service->CreateHistoricalTab(
        sessions::ContentLiveTab::GetForWebContents(contents),
        browser_->tab_strip_model()->GetIndexOfWebContents(contents));
  }
  return absl::nullopt;
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

void BrowserTabStripModelDelegate::CacheWebContents(
    const std::vector<std::unique_ptr<TabStripModel::DetachedWebContents>>&
        web_contents) {
  if (browser_shutdown::HasShutdownStarted() ||
      browser_->profile()->IsOffTheRecord() ||
      !ClosedTabCache::IsFeatureEnabled()) {
    return;
  }

  DCHECK(!web_contents.empty());

  ClosedTabCache& cache =
      ClosedTabCacheServiceFactory::GetForProfile(browser_->profile())
          ->closed_tab_cache();

  // We assume a cache size of one. Only the last recently closed tab will be
  // cached.
  // TODO(https://crbug.com/1236077): Cache more than one tab in ClosedTabCache.
  auto& dwc = web_contents.back();
  if (!cache.CanCacheWebContents(dwc->id))
    return;

  std::unique_ptr<content::WebContents> wc;
  dwc->owned_contents.swap(wc);
  dwc->remove_reason = TabStripModelChange::RemoveReason::kCached;
  auto cached = std::make_pair(dwc->id, std::move(wc));
  cache.CacheWebContents(std::move(cached));
}

void BrowserTabStripModelDelegate::FollowSite(
    content::WebContents* web_contents) {
  chrome::FollowSite(web_contents);
}

void BrowserTabStripModelDelegate::UnfollowSite(
    content::WebContents* web_contents) {
  chrome::UnfollowSite(web_contents);
}

bool BrowserTabStripModelDelegate::IsForWebApp() {
  return web_app::AppBrowserController::IsWebApp(browser_);
}

void BrowserTabStripModelDelegate::CopyURL(content::WebContents* web_contents) {
  chrome::CopyURL(web_contents);
}

void BrowserTabStripModelDelegate::GoBack(content::WebContents* web_contents) {
  chrome::GoBack(web_contents);
}

bool BrowserTabStripModelDelegate::CanGoBack(
    content::WebContents* web_contents) {
  return chrome::CanGoBack(web_contents);
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
