// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_ai/test_support/fake_component.h"

#include <cstdint>
#include <utility>

namespace on_device_ai {

FakeComponent::FakeComponent(std::string id, uint64_t total_bytes)
    : id_(std::move(id)), total_bytes_(total_bytes) {}

component_updater::CrxUpdateItem FakeComponent::CreateUpdateItem(
    update_client::ComponentState state,
    uint64_t downloaded_bytes) const {
  component_updater::CrxUpdateItem update_item;
  update_item.state = state;
  update_item.id = id_;
  update_item.downloaded_bytes = downloaded_bytes;
  update_item.total_bytes = total_bytes_;
  return update_item;
}
}  // namespace on_device_ai
