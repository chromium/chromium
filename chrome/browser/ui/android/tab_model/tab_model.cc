// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_window_delegate_android.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"

using chrome::android::ActivityType;

// Keep this in sync with
// chrome/android/java/src/org/chromium/chrome/browser/tabmodel/TabList.java
static int INVALID_TAB_INDEX = -1;

namespace {
sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate(Profile* profile) {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(profile);

  return service->GetOpenTabsUIDelegate();
}
}  // namespace

TabModel::TabModel(Profile* profile, ActivityType activity_type)
    : profile_(profile),
      activity_type_(activity_type),
      live_tab_context_(new AndroidLiveTabContext(this)),
      synced_window_delegate_(new browser_sync::SyncedWindowDelegateAndroid(
          this,
          activity_type == ActivityType::kTabbed)),
      session_id_(SessionID::NewUnique()) {}

TabModel::~TabModel() = default;

Profile* TabModel::GetProfile() const {
  return profile_;
}

bool TabModel::IsOffTheRecord() const {
  return GetProfile()->IsOffTheRecord();
}

sync_sessions::SyncedWindowDelegate* TabModel::GetSyncedWindowDelegate() const {
  return synced_window_delegate_.get();
}

SessionID TabModel::GetSessionId() const {
  return session_id_;
}

sessions::LiveTabContext* TabModel::GetLiveTabContext() const {
  return live_tab_context_.get();
}

content::WebContents* TabModel::GetActiveWebContents() const {
  int active_index = GetActiveIndex();
  if (active_index == INVALID_TAB_INDEX)
    return nullptr;
  return GetWebContentsAt(active_index);
}

void TabModel::BroadcastSessionRestoreComplete() {
  sync_sessions::SyncSessionsWebContentsRouter* router =
      sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
          GetProfile());
  if (router)
    router->NotifySessionRestoreComplete();

  RecordActualSyncedTabsHistogram();
}

// This logic is loosely based off of
// TabContentsSyncedTabDelegate::ShouldSync().
void TabModel::RecordActualSyncedTabsHistogram() {
  sync_sessions::OpenTabsUIDelegate* open_tabs_delegate =
      GetOpenTabsUIDelegate(GetProfile());
  // This null check will early exit if the user is in incognito mode or
  // if the user has opted out of syncing tabs.
  if (!open_tabs_delegate) {
    return;
  }

  // This null check will early exit if the user has a null sync session,
  // which includes but is not limited to: no synced tabs or an opt out of
  // syncing tabs even if sync is enabled.
  const sync_sessions::SyncedSession* local_session = nullptr;
  if (!open_tabs_delegate->GetLocalSession(&local_session)) {
    return;
  }

  // This check will early exit if there are no tabs in the local model.
  if (GetTabCount() == 0) {
    return;
  }

  int synced_tabs_count = 0;
  for (const auto& [window_id, window] : local_session->windows) {
    synced_tabs_count += window->wrapped_window.tabs.size();
  }

  int eligible_tabs_count = 0;
  for (int i = 0; i < GetTabCount(); i++) {
    if (SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
            GetTabAt(i)->GetURL())) {
      eligible_tabs_count++;
    }
  }

  // Prevent dividing by 0 in case all tabs are filtered out.
  if (eligible_tabs_count == 0) {
    return;
  }

  int percent_synced = synced_tabs_count * 100 / eligible_tabs_count;
  base::UmaHistogramPercentage("Android.Sync.ActualSyncedTabCountPercentage",
                               percent_synced);
}
