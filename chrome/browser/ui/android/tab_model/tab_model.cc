// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_window_delegate_android.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "components/omnibox/browser/location_bar_model_impl.h"

// Keep this in sync with
// chrome/android/java/src/org/chromium/chrome/browser/tabmodel/TabList.java
static int INVALID_TAB_INDEX = -1;

TabModel::TabModel(Profile* profile, bool is_tabbed_activity)
    : profile_(profile),
      live_tab_context_(new AndroidLiveTabContext(this)),
      synced_window_delegate_(
          new browser_sync::SyncedWindowDelegateAndroid(this,
                                                        is_tabbed_activity)),
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
}
