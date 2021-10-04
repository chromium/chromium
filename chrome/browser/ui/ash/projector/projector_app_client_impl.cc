// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"

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
