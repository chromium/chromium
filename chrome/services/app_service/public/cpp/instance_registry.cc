// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/instance_registry.h"

#include <memory>
#include <utility>

#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/cpp/instance_update.h"

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
  DCHECK(!observers_.might_have_observers());
}

void InstanceRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InstanceRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InstanceRegistry::OnInstances(const Instances& deltas) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

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

void InstanceRegistry::DoOnInstances(const Instances& deltas) {
  in_progress_ = true;

  // The remaining for loops range over the deltas vector, so that
  // OninstanceUpdate is called for each updates, and notify the observers for
  // every de-duplicated delta. Also update the states for every delta.
  for (const auto& d_iter : deltas) {
    auto s_iter = states_.find(d_iter->Window());
    Instance* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    if (InstanceUpdate::Equals(state, d_iter.get())) {
      continue;
    }

    std::unique_ptr<Instance> old_state = nullptr;
    if (state) {
      old_state = state->Clone();
      InstanceUpdate::Merge(state, d_iter.get());
    } else {
      states_.insert(
          std::make_pair(d_iter.get()->Window(), (d_iter.get()->Clone())));
    }

    for (auto& obs : observers_) {
      obs.OnInstanceUpdate(InstanceUpdate(old_state.get(), d_iter.get()));
    }

    if (static_cast<InstanceState>(d_iter.get()->State() &
                                   InstanceState::kDestroyed) !=
        InstanceState::kUnknown) {
      states_.erase(d_iter.get()->Window());
    }
  }
  in_progress_ = false;
}

}  // namespace apps
