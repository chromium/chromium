// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_info_watcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/service/sync_service.h"

ProfileInfoWatcher::ProfileInfoWatcher(Profile* profile,
                                       base::RepeatingClosure callback)
    : profile_(profile), callback_(std::move(callback)) {
  DCHECK(profile_);
  DCHECK(!callback_.is_null());

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service) {
    sync_observation_.Observe(sync_service);
  }
}

ProfileInfoWatcher::~ProfileInfoWatcher() = default;

void ProfileInfoWatcher::OnStateChanged(syncer::SyncService* sync) {
  RunCallback();
}

void ProfileInfoWatcher::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observation_.Reset();
}

HistorySignInState ProfileInfoWatcher::GetSignInState() const {
  return HistoryUtil::GetSignInState(profile_);
}

void ProfileInfoWatcher::RunCallback() {
  callback_.Run();
}
