// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_capability_access_cache.h"

#include <utility>
#include "base/observer_list.h"

namespace apps {

AppCapabilityAccessCache::Observer::~Observer() {
  CHECK(!IsInObserverList());
}

AppCapabilityAccessCache::AppCapabilityAccessCache()
    : account_id_(EmptyAccountId()) {}

AppCapabilityAccessCache::~AppCapabilityAccessCache() {
  for (auto& obs : observers_) {
    obs.OnAppCapabilityAccessCacheWillBeDestroyed(this);
  }
  DCHECK(observers_.empty());
}

void AppCapabilityAccessCache::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppCapabilityAccessCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppCapabilityAccessCache::SetAccountId(const AccountId& account_id) {
  account_id_ = account_id;
}

std::set<std::string> AppCapabilityAccessCache::GetAppsAccessingCamera() {
  std::set<std::string> app_ids;
  ForEachApp([&app_ids](const apps::CapabilityAccessUpdate& update) {
    auto camera = update.Camera();
    if (camera.value_or(false)) {
      app_ids.insert(update.AppId());
    }
  });
  return app_ids;
}

std::set<std::string> AppCapabilityAccessCache::GetAppsAccessingMicrophone() {
  std::set<std::string> app_ids;
  ForEachApp([&app_ids](const apps::CapabilityAccessUpdate& update) {
    auto microphone = update.Microphone();
    if (microphone.value_or(false)) {
      app_ids.insert(update.AppId());
    }
  });
  return app_ids;
}

std::set<std::string> AppCapabilityAccessCache::GetAppsAccessingCapabilities() {
  std::set<std::string> app_ids;
  ForEachApp([&app_ids](const apps::CapabilityAccessUpdate& update) {
    if (update.IsAccessingAnyCapability()) {
      app_ids.insert(update.AppId());
    }
  });
  return app_ids;
}

void AppCapabilityAccessCache::OnCapabilityAccesses(
    std::vector<CapabilityAccessPtr> deltas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (!deltas_in_progress_.empty()) {
    std::move(deltas.begin(), deltas.end(),
              std::back_inserter(deltas_pending_));
    return;
  }

  DoOnCapabilityAccesses(std::move(deltas));
  while (!deltas_pending_.empty()) {
    std::vector<CapabilityAccessPtr> pending;
    pending.swap(deltas_pending_);
    DoOnCapabilityAccesses(std::move(pending));
  }
}

void AppCapabilityAccessCache::DoOnCapabilityAccesses(
    std::vector<CapabilityAccessPtr> deltas) {
  // Merge any deltas elements that have the same app_id. If an observer's
  // OnCapabilityAccessUpdate calls back into this AppCapabilityAccessCache then
  // we can therefore present a single delta for any given app_id.
  for (auto& delta : deltas) {
    auto d_iter = deltas_in_progress_.find(delta->app_id);
    if (d_iter != deltas_in_progress_.end()) {
      CapabilityAccessUpdate::Merge(d_iter->second, delta.get());
    } else {
      deltas_in_progress_[delta->app_id] = delta.get();
    }
  }

  // The remaining for loops range over the deltas_in_progress_ map, not the
  // deltas vector, so that OnCapabilityAccessUpdate is called only once per
  // unique app_id.

  // Notify the observers for every de-duplicated delta.
  for (const auto& d_iter : deltas_in_progress_) {
    auto s_iter = states_.find(d_iter.first);
    CapabilityAccess* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    CapabilityAccess* delta = d_iter.second;

    for (auto& obs : observers_) {
      obs.OnCapabilityAccessUpdate(
          CapabilityAccessUpdate(state, delta, account_id_));
    }
  }

  // Update the states for every de-duplicated delta.
  for (const auto& d_iter : deltas_in_progress_) {
    auto s_iter = states_.find(d_iter.first);
    CapabilityAccess* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    CapabilityAccess* delta = d_iter.second;

    if (state) {
      CapabilityAccessUpdate::Merge(state, delta);
    } else {
      states_.insert(std::make_pair(delta->app_id, delta->Clone()));
    }
  }
  deltas_in_progress_.clear();
}

}  // namespace apps
