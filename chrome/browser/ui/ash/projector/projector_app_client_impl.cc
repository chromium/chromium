// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"

// static
void ProjectorAppClientImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      ash::prefs::kProjectorCreationFlowEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

ProjectorAppClientImpl::ProjectorAppClientImpl() = default;
ProjectorAppClientImpl::~ProjectorAppClientImpl() = default;

signin::IdentityManager* ProjectorAppClientImpl::GetIdentityManager() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(chromeos::ProfileHelper::IsPrimaryProfile(profile));
  return IdentityManagerFactory::GetForProfile(profile);
}

void ProjectorAppClientImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProjectorAppClientImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

network::mojom::URLLoaderFactory*
ProjectorAppClientImpl::GetUrlLoaderFactory() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(chromeos::ProfileHelper::IsPrimaryProfile(profile));
  return profile->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

void ProjectorAppClientImpl::OnNewScreencastPreconditionChanged(
    bool can_start) {
  for (auto& observer : observers_)
    observer.OnNewScreencastPreconditionChanged(can_start);
}

// TODO(b/201468756): Implement a PendingScreencastManager to provide the set
// of PendingScreencast.
const std::set<ash::PendingScreencast>&
ProjectorAppClientImpl::GetPendingScreencasts() const {
  return pending_screencasts_;
}

void ProjectorAppClientImpl::NotifyScreencastsPendingStatusChanged(
    const std::set<ash::PendingScreencast>& pending_screencast) {
  for (auto& observer : observers_)
    observer.OnScreencastsPendingStatusChanged(pending_screencast);
}
