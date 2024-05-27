// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_instance_waiter.h"

namespace apps {

AppInstanceWaiter::AppInstanceWaiter(apps::InstanceRegistry& registry,
                                     const std::string& app_id,
                                     apps::InstanceState state)
    : registry_(registry), app_id_(app_id), state_(state) {}

AppInstanceWaiter::~AppInstanceWaiter() = default;

void AppInstanceWaiter::Await() {
  auto instances = registry_->GetInstances(app_id_);
  CHECK_LE(instances.size(), 1u);
  if (instances.empty() || (*instances.begin())->State() != state_) {
    observation_.Observe(&*registry_);
    run_loop_.Run();
  }
}

void AppInstanceWaiter::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (update.AppId() == app_id_ && update.State() == state_) {
    CHECK_EQ(registry_->GetInstances(app_id_).size(), 1u);
    run_loop_.Quit();
  }
}

void AppInstanceWaiter::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  observation_.Reset();
}

}  // namespace apps
