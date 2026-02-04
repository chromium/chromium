// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_AI_TEST_SUPPORT_FAKE_COMPONENT_H_
#define COMPONENTS_ON_DEVICE_AI_TEST_SUPPORT_FAKE_COMPONENT_H_

#include <cstdint>

#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/crx_update_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_ai {

class FakeComponent {
 public:
  FakeComponent(std::string id, uint64_t total_bytes);

  component_updater::CrxUpdateItem CreateUpdateItem(
      update_client::ComponentState state,
      uint64_t downloaded_bytes) const;

  const std::string& id() { return id_; }
  uint64_t total_bytes() { return total_bytes_; }

 private:
  std::string id_;
  uint64_t total_bytes_;
};

}  // namespace on_device_ai

#endif  // COMPONENTS_ON_DEVICE_AI_TEST_SUPPORT_FAKE_COMPONENT_H_
