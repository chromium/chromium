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

void InstanceRegistry::OnInstance(InstancePtr delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (!delta || delta->InstanceId()) {
    // TODO(crbug.com/1251501): Implement updating the instance registry using
    // instance ID as a key.
    return;
  }
  // If the instance state is not kDestroyed, adds to
  // |app_id_to_app_instance_key_|, otherwise removes the instance key from
  // |app_id_to_app_instance_key_|.
  if (static_cast<InstanceState>(delta->State() & InstanceState::kDestroyed) ==
      InstanceState::kUnknown) {
    app_id_to_app_instance_key_[delta->AppId()].insert(delta->GetInstanceKey());
  } else {
    app_id_to_app_instance_key_[delta->AppId()].erase(
        delta.get()->GetInstanceKey());
    if (app_id_to_app_instance_key_[delta->AppId()].size() == 0) {
      app_id_to_app_instance_key_.erase(delta->AppId());
    }
  }

  if (in_progress_) {
    deltas_pending_.push_back(std::move(delta));
    return;
  }
  DoOnInstance(std::move(delta));
  while (!deltas_pending_.empty()) {
    InstancePtr instance = std::move(*deltas_pending_.begin());
    DoOnInstance(std::move(instance));
    deltas_pending_.pop_front();
  }
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
  auto s_iter = instance_key_states_.find(instance_key);
  return (s_iter != instance_key_states_.end()) ? s_iter->second.get()->State()
                                                : InstanceState::kUnknown;
}

ash::ShelfID InstanceRegistry::GetShelfId(
    const Instance::InstanceKey& instance_key) const {
  auto s_iter = instance_key_states_.find(instance_key);
  return (s_iter != instance_key_states_.end())
             ? ash::ShelfID(s_iter->second.get()->AppId(),
                            s_iter->second.get()->LaunchId())
             : ash::ShelfID();
}

bool InstanceRegistry::Exists(const Instance::InstanceKey& instance_key) const {
  return instance_key_states_.find(instance_key) != instance_key_states_.end();
}

bool InstanceRegistry::ContainsAppId(const std::string& app_id) const {
  return base::Contains(app_id_to_app_instance_key_, app_id);
}

void InstanceRegistry::DoOnInstance(InstancePtr delta) {
  in_progress_ = true;

  if (delta->InstanceId()) {
    // TODO(crbug.com/1251501): Implement updating the instance registry using
    // instance ID as a key.
    in_progress_ = false;
    return;
  }
  auto s_iter = instance_key_states_.find(delta->GetInstanceKey());
  Instance* state =
      (s_iter != instance_key_states_.end()) ? s_iter->second.get() : nullptr;
  if (InstanceUpdate::Equals(state, delta.get())) {
    in_progress_ = false;
    return;
  }

  std::unique_ptr<Instance> old_state;
  Instance* new_delta = delta.get();
  if (state) {
    old_state = state->Clone();
    InstanceUpdate::Merge(state, delta.get());
  } else {
    // The content of `delta` is moved, however, `new_delta` is still valid,
    // because `new_delta` is the pointer to the content of `delta`.
    instance_key_states_.insert(
        std::make_pair(delta->GetInstanceKey(), std::move(delta)));
  }

  for (auto& obs : observers_) {
    obs.OnInstanceUpdate(InstanceUpdate(old_state.get(), new_delta));
  }

  if (static_cast<InstanceState>(new_delta->State() &
                                 InstanceState::kDestroyed) !=
      InstanceState::kUnknown) {
    instance_key_states_.erase(new_delta->GetInstanceKey());
  }
  in_progress_ = false;
}

}  // namespace apps
