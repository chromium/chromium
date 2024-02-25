// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance_registry.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_update.h"

namespace apps {

InstanceParams::InstanceParams(const std::string& app_id, aura::Window* window)
    : app_id(app_id), window(window) {}

InstanceParams::~InstanceParams() = default;

InstanceRegistry::Observer::~Observer() {
  CHECK(!IsInObserverList());
}

InstanceRegistry::InstanceRegistry() = default;

InstanceRegistry::~InstanceRegistry() {
  for (auto& obs : observers_) {
    obs.OnInstanceRegistryWillBeDestroyed(this);
  }
  DCHECK(observers_.empty());
}

void InstanceRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InstanceRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InstanceRegistry::CreateOrUpdateInstance(InstanceParams&& params) {
  base::UnguessableToken instance_id;
  auto it = window_to_instance_ids_.find(params.window);
  if (it == window_to_instance_ids_.end()) {
    instance_id = base::UnguessableToken::Create();
  } else {
    instance_id = *it->second.begin();
  }

  auto instance =
      std::make_unique<Instance>(params.app_id, instance_id, params.window);

  if (params.launch_id.has_value()) {
    instance->SetLaunchId(params.launch_id.value());
  }

  if (params.state.has_value()) {
    instance->UpdateState(params.state.value().first,
                          params.state.value().second);
  }

  if (params.browser_context.has_value()) {
    instance->SetBrowserContext(params.browser_context.value());
  }
  OnInstance(std::move(instance));
}

void InstanceRegistry::OnInstance(InstancePtr delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (!delta || !delta->InstanceId()) {
    return;
  }

  // If the instance state is not kDestroyed, adds to
  // `window_to_instance_ids_`, otherwise removes the instance key from
  // `window_to_instance_ids_`.
  if (static_cast<InstanceState>(delta->State() & InstanceState::kDestroyed) ==
      InstanceState::kUnknown) {
    auto it = instance_id_to_window_.find(delta->InstanceId());
    // If `window` is changed, remove the instance id from
    // `window_to_instance_ids_`.
    if (it != instance_id_to_window_.end() && it->second != delta->Window()) {
      MaybeRemoveInstanceId(/*instance_id=*/it->first, /*window=*/it->second);
    }
    window_to_instance_ids_[delta->Window()].insert(delta->InstanceId());
    instance_id_to_window_[delta->InstanceId()] = delta->Window();
  } else {
    MaybeRemoveInstanceId(delta->InstanceId(), delta->Window());
    instance_id_to_window_.erase(delta->InstanceId());
  }

  if (in_progress_) {
    deltas_pending_.push_back(std::move(delta));
    return;
  }
  DoOnInstance(std::move(delta));
  while (!deltas_pending_.empty()) {
    InstancePtr instance = std::move(*deltas_pending_.begin());
    deltas_pending_.pop_front();
    DoOnInstance(std::move(instance));
  }
}

std::set<raw_ptr<const Instance, SetExperimental>>
InstanceRegistry::GetInstances(const std::string& app_id) {
  auto it = app_id_to_instances_.find(app_id);
  if (it == app_id_to_instances_.end()) {
    return std::set<raw_ptr<const Instance, SetExperimental>>();
  }
  return it->second;
}

InstanceState InstanceRegistry::GetState(const aura::Window* window) const {
  InstanceState state = InstanceState::kUnknown;
  ForInstancesWithWindow(window, [&state](const apps::InstanceUpdate& update) {
    state = update.State();
  });
  return state;
}

ash::ShelfID InstanceRegistry::GetShelfId(const aura::Window* window) const {
  ash::ShelfID shelf_id;
  ForInstancesWithWindow(
      window, [&shelf_id](const apps::InstanceUpdate& update) {
        shelf_id = ash::ShelfID(update.AppId(), update.LaunchId());
      });
  return shelf_id;
}

bool InstanceRegistry::Exists(const aura::Window* window) const {
  bool found = false;
  ForInstancesWithWindow(
      window, [&](const apps::InstanceUpdate& update) { found = true; });
  return found;
}

bool InstanceRegistry::ContainsAppId(const std::string& app_id) const {
  return base::Contains(app_id_to_instances_, app_id);
}

void InstanceRegistry::DoOnInstance(InstancePtr delta) {
  in_progress_ = true;

  auto s_iter = states_.find(delta->InstanceId());
  Instance* state = (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
  if (InstanceUpdate::Equals(state, delta.get())) {
    in_progress_ = false;
    return;
  }

  Instance* new_delta = delta.get();
  if (state) {
    old_state_ = state->Clone();
    InstanceUpdate::Merge(state, new_delta);
  } else {
    old_state_.reset();

    // `new_delta` is still valid, though `delta` is moved, because `new_delta`
    // is the pointer to the content of `delta`.
    states_.insert(std::make_pair(new_delta->InstanceId(), std::move(delta)));
    app_id_to_instances_[new_delta->AppId()].insert(new_delta);
  }

  for (auto& obs : observers_) {
    obs.OnInstanceUpdate(InstanceUpdate(old_state_.get(), new_delta));
  }

  if (static_cast<InstanceState>(new_delta->State() &
                                 InstanceState::kDestroyed) !=
      InstanceState::kUnknown) {
    MaybeRemoveInstance(new_delta);
  }

  old_state_.reset();
  in_progress_ = false;
}

void InstanceRegistry::MaybeRemoveInstance(const Instance* delta) {
  DCHECK(delta);

  auto it = states_.find(delta->InstanceId());
  if (it == states_.end()) {
    return;
  }

  const auto& app_id = delta->AppId();
  app_id_to_instances_[app_id].erase(it->second.get());
  if (app_id_to_instances_[app_id].empty()) {
    app_id_to_instances_.erase(app_id);
  }

  states_.erase(it);
}

void InstanceRegistry::MaybeRemoveInstanceId(
    const base::UnguessableToken& instance_id,
    aura::Window* window) {
  window_to_instance_ids_[window].erase(instance_id);
  if (window_to_instance_ids_[window].empty()) {
    window_to_instance_ids_.erase(window);
  }
}

}  // namespace apps
