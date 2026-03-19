// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_service_tracker.h"

#include <ranges>

namespace tabs_api::testing {

ToyTabStripServiceTracker::ToyTabStripServiceTracker() = default;
ToyTabStripServiceTracker::~ToyTabStripServiceTracker() = default;

void ToyTabStripServiceTracker::SetOnAddedCallback(ServiceCallback on_added) {
  on_service_added_callback_ = std::move(on_added);
}

void ToyTabStripServiceTracker::SetOnRemovedCallback(
    ServiceCallback on_removed) {
  on_service_removed_callback_ = std::move(on_removed);
}

std::vector<TabStripService*> ToyTabStripServiceTracker::GetExistingServices() {
  std::vector<TabStripService*> result;
  for (const auto& service : services_) {
    result.push_back(service.get());
  }

  return result;
}

void ToyTabStripServiceTracker::AddService(TabStripService* service) {
  services_.push_back(service);
  if (on_service_added_callback_) {
    on_service_added_callback_.Run(service);
  }
}

void ToyTabStripServiceTracker::RemoveService(TabStripService* service) {
  std::erase(services_, service);
  if (on_service_removed_callback_) {
    on_service_removed_callback_.Run(service);
  }
}

}  // namespace tabs_api::testing
