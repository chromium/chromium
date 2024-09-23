// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_connections_manager.h"

#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection_impl.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::nearby::presence {

class NearbyPresenceConnectionsManagerTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_CALL(nearby_process_manager_, GetNearbyProcessReference)
        .WillOnce([&](ash::nearby::NearbyProcessManager::
                          NearbyProcessStoppedCallback) {
          auto mock_reference_ptr =
              std::make_unique<ash::nearby::MockNearbyProcessManager::
                                   MockNearbyProcessReference>();

          EXPECT_CALL(*(mock_reference_ptr.get()), GetNearbyConnections)
              .WillOnce(
                  testing::ReturnRef(nearby_connections_.shared_remote()));

          return mock_reference_ptr;
        });

    connections_manager_ = std::make_unique<NearbyPresenceConnectionsManager>(
        &nearby_process_manager_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  testing::NiceMock<ash::nearby::MockNearbyConnections> nearby_connections_;
  testing::NiceMock<ash::nearby::MockNearbyProcessManager>
      nearby_process_manager_;
  std::unique_ptr<NearbyPresenceConnectionsManager> connections_manager_;
};

TEST_F(NearbyPresenceConnectionsManagerTest, InitSuccessfully) {
  EXPECT_TRUE(connections_manager_);
}

}  // namespace ash::nearby::presence
