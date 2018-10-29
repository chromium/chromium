// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_window_delegate_android.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "components/omnibox/browser/toolbar_model_impl.h"
#include "content/public/browser/notification_service.h"

using content::NotificationService;

// Keep this in sync with
// chrome/android/java/src/org/chromium/chrome/browser/tabmodel/TabList.java
static int INVALID_TAB_INDEX = -1;

TabModel::TabModel(Profile* profile, bool is_tabbed_activity)
    : profile_(profile),
      live_tab_context_(new AndroidLiveTabContext(this)),
      synced_window_delegate_(
          new browser_sync::SyncedWindowDelegateAndroid(this,
                                                        is_tabbed_activity)),
      session_id_(SessionID::NewUnique()) {
  if (profile) {
    // A normal Profile creates an OTR profile if it does not exist when
    // GetOffTheRecordProfile() is called, so we guard it with
    // HasOffTheRecordProfile(). An OTR profile returns itself when you call
    // GetOffTheRecordProfile().
    is_off_the_record_ = (profile->HasOffTheRecordProfile() &&
        profile == profile->GetOffTheRecordProfile());

    // A profile can be destroyed, for example in the case of closing all
    // incognito tabs. We therefore must listen for when this happens, and
    // remove our pointer to the profile accordingly.
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::Source<Profile>(profile_));
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                   content::NotificationService::AllSources());
  } else {
    is_off_the_record_ = false;
  }
}

TabModel::~TabModel() {
}

Profile* TabModel::GetProfile() const {
  return profile_;
}

bool TabModel::IsOffTheRecord() const {
  return is_off_the_record_;
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
  if (profile_) {
    sync_sessions::SyncSessionsWebContentsRouter* router =
        sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
            profile_);
    if (router)
      router->NotifySessionRestoreComplete();
  } else {
    // TODO(nyquist): Uncomment this once downstream Android uses new
    // constructor that takes a Profile* argument. See crbug.com/159704.
    // NOTREACHED();
  }
}

void TabModel::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      // Our profile just got destroyed, so we delete our pointer to it.
      profile_ = NULL;
      break;
    case chrome::NOTIFICATION_PROFILE_CREATED:
      // Our incognito tab model out lives the profile, so we need to recapture
      // the pointer if ours was previously deleted.
      // NOTIFICATION_PROFILE_DESTROYED is not sent for every destruction, so
      // we overwrite the pointer regardless of whether it's NULL.
      if (is_off_the_record_) {
        Profile* profile = content::Source<Profile>(source).ptr();
        if (profile && profile->IsOffTheRecord())
          profile_ = profile;
      }
      break;
    default:
      NOTREACHED();
  }
}
