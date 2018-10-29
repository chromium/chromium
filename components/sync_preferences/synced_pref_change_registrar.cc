// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/synced_pref_change_registrar.h"

#include "base/bind.h"

namespace sync_preferences {

namespace {

void InvokeUnnamedCallback(
    const SyncedPrefChangeRegistrar::ChangeCallback& callback,
    const std::string& path,
    bool from_sync) {
  callback.Run(from_sync);
}

}  // namespace

SyncedPrefChangeRegistrar::SyncedPrefChangeRegistrar(
    PrefServiceSyncable* pref_service) {
  pref_service_ = pref_service;
}

SyncedPrefChangeRegistrar::~SyncedPrefChangeRegistrar() {
  RemoveAll();
}

void SyncedPrefChangeRegistrar::Add(const char* path,
                                    const ChangeCallback& callback) {
  Add(path, base::Bind(InvokeUnnamedCallback, callback));
}

void SyncedPrefChangeRegistrar::Add(const char* path,
                                    const NamedChangeCallback& callback) {
  DCHECK(!IsObserved(path));
  observers_[path] = callback;
  pref_service_->AddSyncedPrefObserver(path, this);
}

void SyncedPrefChangeRegistrar::Remove(const char* path) {
  observers_.erase(path);
  pref_service_->RemoveSyncedPrefObserver(path, this);
}

void SyncedPrefChangeRegistrar::RemoveAll() {
  for (auto iter = observers_.begin(); iter != observers_.end(); ++iter) {
    pref_service_->RemoveSyncedPrefObserver(iter->first, this);
  }
  observers_.clear();
}

bool SyncedPrefChangeRegistrar::IsObserved(const char* path) const {
  return observers_.find(path) != observers_.end();
}

void SyncedPrefChangeRegistrar::OnSyncedPrefChanged(const std::string& path,
                                                    bool from_sync) {
  if (pref_service_->IsManagedPreference(path))
    return;
  ObserverMap::const_iterator iter = observers_.find(path);
  if (iter == observers_.end())
    return;
  iter->second.Run(path, from_sync);
}

}  // namespace sync_preferences
