// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_registry_cache.h"

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "components/services/app_service/public/cpp/features.h"

#include <utility>

namespace apps {

AppRegistryCache::Observer::Observer(AppRegistryCache* cache) {
  Observe(cache);
}

AppRegistryCache::Observer::Observer() = default;

AppRegistryCache::Observer::~Observer() {
  if (cache_) {
    cache_->RemoveObserver(this);
  }
}

void AppRegistryCache::Observer::Observe(AppRegistryCache* cache) {
  if (cache == cache_) {
    // Early exit to avoid infinite loops if we're in the middle of a callback.
    return;
  }
  if (cache_) {
    cache_->RemoveObserver(this);
  }
  cache_ = cache;
  if (cache_) {
    cache_->AddObserver(this);
  }
}

AppRegistryCache::AppRegistryCache() : account_id_(EmptyAccountId()) {}

AppRegistryCache::~AppRegistryCache() {
  for (auto& obs : observers_) {
    obs.OnAppRegistryCacheWillBeDestroyed(this);
  }
  DCHECK(observers_.empty());
}

void AppRegistryCache::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AppRegistryCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppRegistryCache::OnApps(std::vector<apps::mojom::AppPtr> deltas,
                              apps::mojom::AppType app_type,
                              bool should_notify_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (should_notify_initialized) {
    DCHECK_NE(apps::mojom::AppType::kUnknown, app_type);
    if (!IsAppTypeInitialized(ConvertMojomAppTypToAppType(app_type))) {
      in_progress_initialized_mojom_app_types_.insert(app_type);
    }
  }

  if (!mojom_deltas_in_progress_.empty()) {
    std::move(deltas.begin(), deltas.end(),
              std::back_inserter(mojom_deltas_pending_));
    return;
  }

  DoOnApps(std::move(deltas));
  while (!mojom_deltas_pending_.empty()) {
    std::vector<apps::mojom::AppPtr> pending;
    pending.swap(mojom_deltas_pending_);
    DoOnApps(std::move(pending));
  }

  OnAppTypeInitialized();
}

void AppRegistryCache::OnApps(std::vector<AppPtr> deltas,
                              apps::AppType app_type,
                              bool should_notify_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (should_notify_initialized) {
    DCHECK_NE(apps::AppType::kUnknown, app_type);
    if (!IsAppTypeInitialized(app_type)) {
      in_progress_initialized_app_types_.insert(app_type);
    }
  }

  if (!deltas_in_progress_.empty()) {
    std::move(deltas.begin(), deltas.end(),
              std::back_inserter(deltas_pending_));
    return;
  }

  DoOnApps(std::move(deltas));
  while (!deltas_pending_.empty()) {
    std::vector<AppPtr> pending;
    pending.swap(deltas_pending_);
    DoOnApps(std::move(pending));
  }

  if (base::FeatureList::IsEnabled(
          kAppServiceOnAppTypeInitializedWithoutMojom)) {
    OnAppTypeInitialized();
  }
}

void AppRegistryCache::DoOnApps(std::vector<apps::mojom::AppPtr> deltas) {
  // Merge any deltas elements that have the same app_id. If an observer's
  // OnAppUpdate calls back into this AppRegistryCache then we can therefore
  // present a single delta for any given app_id.
  for (auto& delta : deltas) {
    auto d_iter = mojom_deltas_in_progress_.find(delta->app_id);
    if (d_iter != mojom_deltas_in_progress_.end()) {
      if (delta->readiness == mojom::Readiness::kRemoved) {
        // Ensure that removed deltas are *not* merged, so that the last update
        // before the merge is sent separately.
        mojom_deltas_pending_.push_back(std::move(delta));
      } else {
        AppUpdate::Merge(d_iter->second, delta.get());
      }
    } else {
      mojom_deltas_in_progress_[delta->app_id] = delta.get();
    }
  }

  if (!base::FeatureList::IsEnabled(kAppServiceOnAppUpdateWithoutMojom)) {
    // The remaining for loops range over the mojom_deltas_in_progress_ map, not
    // the deltas vector, so that OnAppUpdate is called only once per unique
    // app_id.

    // Notify the observers for every de-duplicated delta.
    for (const auto& d_iter : mojom_deltas_in_progress_) {
      // Do not update subscribers for removed apps.
      if (d_iter.second->readiness == mojom::Readiness::kRemoved) {
        continue;
      }
      auto s_iter = mojom_states_.find(d_iter.first);
      apps::mojom::App* state =
          (s_iter != mojom_states_.end()) ? s_iter->second.get() : nullptr;
      apps::mojom::App* delta = d_iter.second;

      for (auto& obs : observers_) {
        obs.OnAppUpdate(AppUpdate(state, delta, account_id_));
      }
    }
  }

  // Update the states for every de-duplicated delta.
  for (const auto& d_iter : mojom_deltas_in_progress_) {
    auto s_iter = mojom_states_.find(d_iter.first);
    apps::mojom::App* state =
        (s_iter != mojom_states_.end()) ? s_iter->second.get() : nullptr;
    apps::mojom::App* delta = d_iter.second;

    if (delta->readiness != mojom::Readiness::kRemoved) {
      if (state) {
        AppUpdate::Merge(state, delta);
      } else {
        mojom_states_.insert(std::make_pair(delta->app_id, delta->Clone()));
      }
    } else {
      DCHECK(!state || state->readiness != mojom::Readiness::kReady);
      mojom_states_.erase(d_iter.first);
    }
  }
  mojom_deltas_in_progress_.clear();
}

void AppRegistryCache::DoOnApps(std::vector<AppPtr> deltas) {
  // Merge any deltas elements that have the same app_id. If an observer's
  // OnAppUpdate calls back into this AppRegistryCache then we can therefore
  // present a single delta for any given app_id.
  for (auto& delta : deltas) {
    auto d_iter = deltas_in_progress_.find(delta->app_id);
    if (d_iter != deltas_in_progress_.end()) {
      if (delta->readiness == Readiness::kRemoved) {
        // Ensure that removed deltas are *not* merged, so that the last update
        // before the merge is sent separately.
        deltas_pending_.push_back(std::move(delta));
      } else {
        AppUpdate::Merge(d_iter->second, delta.get());
      }
    } else {
      deltas_in_progress_[delta->app_id] = delta.get();
    }
  }

  if (base::FeatureList::IsEnabled(kAppServiceOnAppUpdateWithoutMojom)) {
    // The remaining for loops range over the deltas_in_progress_ map, not
    // the deltas vector, so that OnAppUpdate is called only once per unique
    // app_id.

    // Notify the observers for every de-duplicated delta.
    for (const auto& d_iter : deltas_in_progress_) {
      // Do not update subscribers for removed apps.
      if (d_iter.second->readiness == Readiness::kRemoved) {
        continue;
      }
      auto s_iter = states_.find(d_iter.first);
      App* state = (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
      App* delta = d_iter.second;

      for (auto& obs : observers_) {
        obs.OnAppUpdate(AppUpdate(state, delta, account_id_));
      }
    }
  }

  // Update the states for every de-duplicated delta.
  for (const auto& d_iter : deltas_in_progress_) {
    auto s_iter = states_.find(d_iter.first);
    apps::App* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    apps::App* delta = d_iter.second;

    if (delta->readiness != Readiness::kRemoved) {
      if (state) {
        AppUpdate::Merge(state, delta);
      } else {
        states_.insert(std::make_pair(delta->app_id, delta->Clone()));
      }
    } else {
      DCHECK(!state || state->readiness != Readiness::kReady);
      states_.erase(d_iter.first);
    }
  }
  deltas_in_progress_.clear();
}

apps::mojom::AppType AppRegistryCache::GetAppType(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  auto d_iter = mojom_deltas_in_progress_.find(app_id);
  if (d_iter != mojom_deltas_in_progress_.end()) {
    return d_iter->second->app_type;
  }
  auto s_iter = mojom_states_.find(app_id);
  if (s_iter != mojom_states_.end()) {
    return s_iter->second->app_type;
  }
  return apps::mojom::AppType::kUnknown;
}

void AppRegistryCache::SetAccountId(const AccountId& account_id) {
  account_id_ = account_id;
}

const std::set<AppType>& AppRegistryCache::InitializedAppTypes() const {
  return initialized_app_types_;
}

bool AppRegistryCache::IsAppTypeInitialized(apps::AppType app_type) const {
  return base::Contains(initialized_app_types_, app_type);
}

void AppRegistryCache::OnMojomAppTypeInitialized() {
  if (in_progress_initialized_mojom_app_types_.empty()) {
    return;
  }

  auto in_progress_initialized_app_types =
      in_progress_initialized_mojom_app_types_;
  in_progress_initialized_mojom_app_types_.clear();

  for (auto app_type : in_progress_initialized_app_types) {
    for (auto& obs : observers_) {
      obs.OnAppTypeInitialized(ConvertMojomAppTypToAppType(app_type));
    }
    initialized_app_types_.insert(ConvertMojomAppTypToAppType(app_type));
  }
}

void AppRegistryCache::OnAppTypeInitialized() {
  if (!base::FeatureList::IsEnabled(
          kAppServiceOnAppTypeInitializedWithoutMojom)) {
    OnMojomAppTypeInitialized();
    return;
  }

  // Check both the non mojom and mojom initialized status. Only when they are
  // not initialized, call OnAppTypeInitialized to notify observers, because
  // observers might use the non mojom or mojom App struct.
  //
  // TODO(crbug.com/1253250): Remove the mojom initialized checking when all
  // observers use the non mojom App struct only.
  if (in_progress_initialized_mojom_app_types_.empty() ||
      in_progress_initialized_app_types_.empty()) {
    return;
  }

  // In observer's OnAppTypeInitialized callback, `OnApp` might be call  to
  // update the app, then this OnAppTypeInitialized might be called again. So we
  // need to check the initialized `app_type` first, and remove it from
  // `in_progress_initialized_app_types_` to prevent the dead loop.
  std::set<AppType> in_progress_initialized_app_types;
  for (auto app_type : in_progress_initialized_app_types_) {
    if (base::Contains(in_progress_initialized_mojom_app_types_,
                       ConvertAppTypeToMojomAppType(app_type))) {
      in_progress_initialized_app_types.insert(app_type);
    }
  }

  for (auto app_type : in_progress_initialized_app_types) {
    auto mojom_app_type = ConvertAppTypeToMojomAppType(app_type);
    in_progress_initialized_app_types_.erase(app_type);
    in_progress_initialized_mojom_app_types_.erase(mojom_app_type);
    for (auto& obs : observers_) {
      obs.OnAppTypeInitialized(app_type);
    }
    initialized_app_types_.insert(app_type);
  }
}

}  // namespace apps
