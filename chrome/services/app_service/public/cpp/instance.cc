// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/instance.h"

#include <memory>

#include "ui/aura/window.h"

namespace apps {

Instance::Instance(const std::string& app_id, aura::Window* window)
    : app_id_(app_id), window_(window) {
  state_ = InstanceState::kUnknown;
}

Instance::~Instance() = default;

std::unique_ptr<Instance> Instance::Clone() {
  auto instance = std::make_unique<Instance>(this->AppId(), this->Window());
  instance->SetLaunchId(this->LaunchId());
  instance->UpdateState(this->State(), this->LastUpdatedTime());
  return instance;
}

void Instance::UpdateState(InstanceState state,
                           const base::Time& last_updated_time) {
  state_ = state;
  last_updated_time_ = last_updated_time;
}

}  // namespace apps
