// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/complex_tasks/task_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "components/translate/content/browser/content_record_page_language.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#endif

using content::NavigationEntry;

namespace {

// Helper to access the correct NavigationEntry, accounting for pending entries.
NavigationEntry* GetPossiblyPendingEntryAtIndex(
    content::WebContents* web_contents,
    int i) {
  int pending_index = web_contents->GetController().GetPendingEntryIndex();
  return (pending_index == i)
             ? web_contents->GetController().GetPendingEntry()
             : web_contents->GetController().GetEntryAtIndex(i);
}

}  // namespace

TabContentsSyncedTabDelegate::TabContentsSyncedTabDelegate()
    : web_contents_(nullptr) {}

TabContentsSyncedTabDelegate::~TabContentsSyncedTabDelegate() {}

bool TabContentsSyncedTabDelegate::IsBeingDestroyed() const {
  return web_contents_->IsBeingDestroyed();
}

std::string TabContentsSyncedTabDelegate::GetExtensionAppId() const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions::TabHelper::FromWebContents(web_contents_)->GetAppId();
#else
  return std::string();
#endif
}

bool TabContentsSyncedTabDelegate::IsInitialBlankNavigation() const {
  return web_contents_->GetController().IsInitialBlankNavigation();
}

int TabContentsSyncedTabDelegate::GetCurrentEntryIndex() const {
  return web_contents_->GetController().GetCurrentEntryIndex();
}

int TabContentsSyncedTabDelegate::GetEntryCount() const {
  return web_contents_->GetController().GetEntryCount();
}

GURL TabContentsSyncedTabDelegate::GetVirtualURLAtIndex(int i) const {
  DCHECK(web_contents_);
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  return entry ? entry->GetVirtualURL() : GURL();
}

GURL TabContentsSyncedTabDelegate::GetFaviconURLAtIndex(int i) const {
  DCHECK(web_contents_);
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  return entry ? (entry->GetFavicon().valid ? entry->GetFavicon().url : GURL())
               : GURL();
}

ui::PageTransition TabContentsSyncedTabDelegate::GetTransitionAtIndex(
    int i) const {
  DCHECK(web_contents_);
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  // If we don't have an entry, there's not a coherent PageTransition we can
  // supply. There's no PageTransition::Unknown, so we just use the default,
  // which is PageTransition::LINK.
  return entry ? entry->GetTransitionType()
               : ui::PageTransition::PAGE_TRANSITION_LINK;
}

std::string TabContentsSyncedTabDelegate::GetPageLanguageAtIndex(int i) const {
  DCHECK(web_contents_);
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  // If we don't have an entry, return empty language.
  return entry ? translate::GetPageLanguageFromNavigation(entry)
               : std::string();
}

void TabContentsSyncedTabDelegate::GetSerializedNavigationAtIndex(
    int i,
    sessions::SerializedNavigationEntry* serialized_entry) const {
  DCHECK(web_contents_);
  NavigationEntry* entry = GetPossiblyPendingEntryAtIndex(web_contents_, i);
  if (entry) {
    // Explicitly exclude page state when serializing the navigation entry.
    // Sync ignores the page state anyway (e.g. form data is not synced), and
    // the page state can be expensive to serialize.
    *serialized_entry =
        sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
            i, entry,
            sessions::ContentSerializedNavigationBuilder::EXCLUDE_PAGE_STATE);
  }
}

bool TabContentsSyncedTabDelegate::ProfileIsSupervised() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->IsSupervised();
}

const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
TabContentsSyncedTabDelegate::GetBlockedNavigations() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserNavigationObserver* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents_);
  DCHECK(navigation_observer);
  return &navigation_observer->blocked_navigations();
#else
  NOTREACHED();
  return nullptr;
#endif
}

bool TabContentsSyncedTabDelegate::ShouldSync(
    sync_sessions::SyncSessionsClient* sessions_client) {
  if (sessions_client->GetSyncedWindowDelegatesGetter()->FindById(
          GetWindowId()) == nullptr)
    return false;

  // Is there a valid NavigationEntry?
  if (ProfileIsSupervised() && !GetBlockedNavigations()->empty())
    return true;

  if (IsInitialBlankNavigation())
    return false;  // This deliberately ignores a new pending entry.

  int entry_count = GetEntryCount();
  for (int i = 0; i < entry_count; ++i) {
    const GURL& virtual_url = GetVirtualURLAtIndex(i);
    if (!virtual_url.is_valid())
      continue;

    if (sessions_client->ShouldSyncURL(virtual_url))
      return true;
  }
  return false;
}

int64_t TabContentsSyncedTabDelegate::GetTaskIdForNavigationId(
    int nav_id) const {
  const tasks::TaskTabHelper* task_tab_helper = this->task_tab_helper();
  if (task_tab_helper &&
      task_tab_helper->get_task_id_for_navigation(nav_id) != nullptr) {
    return task_tab_helper->get_task_id_for_navigation(nav_id)->id();
  }
  return -1;
}

int64_t TabContentsSyncedTabDelegate::GetParentTaskIdForNavigationId(
    int nav_id) const {
  const tasks::TaskTabHelper* task_tab_helper = this->task_tab_helper();
  if (task_tab_helper &&
      task_tab_helper->get_task_id_for_navigation(nav_id) != nullptr) {
    return task_tab_helper->get_task_id_for_navigation(nav_id)->parent_id();
  }
  return -1;
}

int64_t TabContentsSyncedTabDelegate::GetRootTaskIdForNavigationId(
    int nav_id) const {
  const tasks::TaskTabHelper* task_tab_helper = this->task_tab_helper();
  if (task_tab_helper &&
      task_tab_helper->get_task_id_for_navigation(nav_id) != nullptr) {
    return task_tab_helper->get_task_id_for_navigation(nav_id)->root_id();
  }
  return -1;
}

const content::WebContents* TabContentsSyncedTabDelegate::web_contents() const {
  return web_contents_;
}

content::WebContents* TabContentsSyncedTabDelegate::web_contents() {
  return web_contents_;
}

void TabContentsSyncedTabDelegate::SetWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents;
}

const tasks::TaskTabHelper* TabContentsSyncedTabDelegate::task_tab_helper()
    const {
  if (web_contents_ == nullptr)
    return nullptr;
  return tasks::TaskTabHelper::FromWebContents(web_contents_);
}
