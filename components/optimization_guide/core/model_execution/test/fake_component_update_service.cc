// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"

namespace optimization_guide {

FakeComponentUpdateService::FakeComponentUpdateService() = default;
FakeComponentUpdateService::~FakeComponentUpdateService() = default;

void FakeComponentUpdateService::AddObserver(
    component_updater::ServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FakeComponentUpdateService::RemoveObserver(
    component_updater::ServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeComponentUpdateService::SendUpdate(
    const component_updater::CrxUpdateItem& item) {
  for (auto& observer : observer_list_) {
    observer.OnEvent(item);
  }
}

FakeComponent::FakeComponent(std::string id, uint64_t total_bytes)
    : id_(std::move(id)), total_bytes_(total_bytes) {}

component_updater::CrxUpdateItem FakeComponent::CreateUpdateItem(
    update_client::ComponentState state,
    uint64_t downloaded_bytes) {
  downloaded_bytes_ = downloaded_bytes;

  component_updater::CrxUpdateItem update_item;
  update_item.state = state;
  update_item.id = id_;
  update_item.downloaded_bytes = downloaded_bytes;
  update_item.total_bytes = total_bytes_;
  return update_item;
}

}  // namespace optimization_guide
