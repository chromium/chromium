// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance_registry.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_update.h"

namespace apps {

InstanceRegistry::Observer::Observer(InstanceRegistry* instance_registry) {
  Observe(instance_registry);
}

InstanceRegistry::Observer::Observer() = default;
InstanceRegistry::Observer::~Observer() {
  if (instance_registry_) {
    instance_registry_->RemoveObserver(this);
  }
}

void InstanceRegistry::Observer::Observe(InstanceRegistry* instance_registry) {
  if (instance_registry == instance_registry_) {
    return;
  }

  if (instance_registry_) {
    instance_registry_->RemoveObserver(this);
  }

  instance_registry_ = instance_registry;
  if (instance_registry_) {
    instance_registry_->AddObserver(this);
  }
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

void InstanceRegistry::OnInstances(const Instances& deltas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  for (auto& delta : deltas) {
    // If the instance state is not kDestroyed, adds to
    // |app_id_to_app_instance_key_|, otherwise removes the instance key from
    // |app_id_to_app_instance_key_|.
    if (static_cast<InstanceState>(delta.get()->State() &
                                   InstanceState::kDestroyed) ==
        InstanceState::kUnknown) {
      app_id_to_app_instance_key_[delta.get()->AppId()].insert(
          delta.get()->GetInstanceKey());
    } else {
      app_id_to_app_instance_key_[delta.get()->AppId()].erase(
          delta.get()->GetInstanceKey());
      if (app_id_to_app_instance_key_[delta.get()->AppId()].size() == 0) {
        app_id_to_app_instance_key_.erase(delta.get()->AppId());
      }
    }
  }

  if (in_progress_) {
    for (auto& delta : deltas) {
      deltas_pending_.push_back(delta.get()->Clone());
    }
    return;
  }
  DoOnInstances(std::move(deltas));
  while (!deltas_pending_.empty()) {
    Instances pending;
    pending.swap(deltas_pending_);
    DoOnInstances(std::move(pending));
  }
}

std::set<aura::Window*> InstanceRegistry::GetEnclosingAppWindows(
    const std::string& app_id) {
  auto it = app_id_to_app_instance_key_.find(app_id);
  auto windows = std::set<aura::Window*>{};
  if (it != app_id_to_app_instance_key_.end()) {
    for (auto instance_key : it->second) {
      windows.insert(instance_key.GetEnclosingAppWindow());
    }
  }
  return windows;
}

std::set<const Instance::InstanceKey> InstanceRegistry::GetInstanceKeys(
    const std::string& app_id) {
  auto it = app_id_to_app_instance_key_.find(app_id);
  if (it == app_id_to_app_instance_key_.end())
    return std::set<const Instance::InstanceKey>();
  return it->second;
}

InstanceState InstanceRegistry::GetState(
    const Instance::InstanceKey& instance_key) const {
  auto s_iter = states_.find(instance_key);
  return (s_iter != states_.end()) ? s_iter->second.get()->State()
                                   : InstanceState::kUnknown;
}

ash::ShelfID InstanceRegistry::GetShelfId(
    const Instance::InstanceKey& instance_key) const {
  auto s_iter = states_.find(instance_key);
  return (s_iter != states_.end())
             ? ash::ShelfID(s_iter->second.get()->AppId(),
                            s_iter->second.get()->LaunchId())
             : ash::ShelfID();
}

bool InstanceRegistry::Exists(const Instance::InstanceKey& instance_key) const {
  return states_.find(instance_key) != states_.end();
}

bool InstanceRegistry::ContainsAppId(const std::string& app_id) const {
  return base::Contains(app_id_to_app_instance_key_, app_id);
}

void InstanceRegistry::DoOnInstances(const Instances& deltas) {
  in_progress_ = true;

  // The remaining for loops range over the deltas vector, so that
  // OninstanceUpdate is called for each updates, and notify the observers for
  // every de-duplicated delta. Also update the states for every delta.
  for (const auto& d_iter : deltas) {
    auto s_iter = states_.find(d_iter->GetInstanceKey());
    Instance* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    if (InstanceUpdate::Equals(state, d_iter.get())) {
      continue;
    }

    std::unique_ptr<Instance> old_state;
    if (state) {
      old_state = state->Clone();
      InstanceUpdate::Merge(state, d_iter.get());
    } else {
      states_.insert(std::make_pair(d_iter.get()->GetInstanceKey(),
                                    (d_iter.get()->Clone())));
    }

    for (auto& obs : observers_) {
      obs.OnInstanceUpdate(InstanceUpdate(old_state.get(), d_iter.get()));
    }

    if (static_cast<InstanceState>(d_iter.get()->State() &
                                   InstanceState::kDestroyed) !=
        InstanceState::kUnknown) {
      states_.erase(d_iter.get()->GetInstanceKey());
    }
  }
  in_progress_ = false;
}

}  // namespace apps
