// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance.h"

#include <memory>
#include <utility>

namespace apps {

Instance::Instance(const std::string& app_id,
                   const base::UnguessableToken& instance_id,
                   aura::Window* window)
    : app_id_(app_id), instance_id_(instance_id), window_(window) {}

Instance::~Instance() = default;

std::unique_ptr<Instance> Instance::Clone() {
  std::unique_ptr<Instance> instance = std::make_unique<Instance>(
      this->AppId(), this->InstanceId(), this->Window());

  instance->SetLaunchId(this->LaunchId());
  instance->UpdateState(this->State(), this->LastUpdatedTime());
  instance->SetBrowserContext(this->BrowserContext());
  return instance;
}

void Instance::UpdateState(InstanceState state,
                           const base::Time& last_updated_time) {
  state_ = state;
  last_updated_time_ = last_updated_time;
}

}  // namespace apps
