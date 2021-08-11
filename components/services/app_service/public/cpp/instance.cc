// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance.h"

#include <memory>
#include <utility>

namespace apps {

Instance::InstanceKey::InstanceKey(aura::Window* window) : window_(window) {}

bool Instance::InstanceKey::operator<(const InstanceKey& other) const {
  return Window() < other.Window();
}

bool Instance::InstanceKey::operator==(const InstanceKey& other) const {
  return Window() == other.Window();
}

bool Instance::InstanceKey::operator!=(const InstanceKey& other) const {
  return Window() != other.Window();
}

Instance::Instance(const std::string& app_id, InstanceKey&& instance_key)
    : app_id_(app_id), instance_key_(std::move(instance_key)) {
  state_ = InstanceState::kUnknown;
}

Instance::~Instance() = default;

std::unique_ptr<Instance> Instance::Clone() {
  auto instance = std::make_unique<Instance>(
      this->AppId(), apps::Instance::InstanceKey(this->Window()));
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

void Instance::SetBrowserContext(content::BrowserContext* browser_context) {
  browser_context_ = browser_context;
}

}  // namespace apps

std::ostream& operator<<(std::ostream& os,
                         const apps::Instance::InstanceKey& instance_key) {
  return os << "InstanceKey {Window: " << instance_key.Window() << "}";
}

size_t InstanceKeyHash::operator()(
    const apps::Instance::InstanceKey& key) const {
  return std::hash<aura::Window*>()(key.Window());
}
