// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance.h"

#include <memory>
#include <utility>

namespace apps {

// static
Instance::InstanceKey Instance::InstanceKey::ForWebBasedApp(
    aura::Window* window) {
  return InstanceKey(window,
                     /*is_web_contents_backed=*/true);
}

// static
Instance::InstanceKey Instance::InstanceKey::ForWindowBasedApp(
    aura::Window* window) {
  return InstanceKey(window,
                     /*is_web_contents_backed=*/false);
}

Instance::InstanceKey::InstanceKey(aura::Window* window,
                                   bool is_web_contents_backed)
    : window_(window), is_web_contents_backed_(is_web_contents_backed) {}

aura::Window* Instance::InstanceKey::GetEnclosingAppWindow() const {
  if (is_web_contents_backed_)
    return window_->GetToplevelWindow();
  return window_;
}

bool Instance::InstanceKey::operator<(const InstanceKey& other) const {
  return window_ < other.window_;
}

bool Instance::InstanceKey::operator==(const InstanceKey& other) const {
  // TODO(crbug.com/1203992): This explicitly excludes is_web_contents_backed
  // from the equality checks for now as there are some cases where
  // AppServiceInstanceRegistryHelper will create an InstanceKey with and
  // without is_web_contents_backed set for the same aura::Window object,
  // resulting in assertion failures in the InstanceRegistry. When the
  // BrowserAppWindowTracker is hooked up to this class (which will replace the
  // instance tracking logic in the AppServiceInstanceRegistryHelper) and
  // BrowserAppInstanceTracker::kEnabled is set to true, we should incorporate
  // the WebContents ID of the instance into the equality checks and hash
  // operator.
  return window_ == other.window_;
}

bool Instance::InstanceKey::operator!=(const InstanceKey& other) const {
  return window_ != other.window_;
}

Instance::Instance(const std::string& app_id, InstanceKey&& instance_key)
    : app_id_(app_id), instance_key_(std::move(instance_key)) {
  state_ = InstanceState::kUnknown;
}

Instance::~Instance() = default;

std::unique_ptr<Instance> Instance::Clone() {
  auto instance = std::make_unique<Instance>(
      this->AppId(), apps::Instance::InstanceKey(this->GetInstanceKey()));
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

std::ostream& operator<<(std::ostream& os,
                         const apps::Instance::InstanceKey& instance_key) {
  return os << "InstanceKey {window: " << instance_key.window_
            << ", is_web_contents_backed: "
            << instance_key.is_web_contents_backed_ << "}";
}

size_t InstanceKeyHash::operator()(
    const apps::Instance::InstanceKey& key) const {
  return std::hash<aura::Window*>()(key.window_);
}

}  // namespace apps
