// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sensors/fake_sensor_hal_client.h"
#include "chromeos/components/sensors/fake_sensor_hal_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace sensors {

class SensorHalDispatcherTest : public ::testing::Test {
 public:
  SensorHalDispatcherTest() {}
  SensorHalDispatcherTest(const SensorHalDispatcherTest&) = delete;
  SensorHalDispatcherTest& operator=(const SensorHalDispatcherTest&) = delete;
  ~SensorHalDispatcherTest() override = default;

  void SetUp() override {
    SensorHalDispatcher::Initialize();
    EXPECT_TRUE(SensorHalDispatcher::GetInstance());
  }

  void TearDown() override {
    SensorHalDispatcher::Shutdown();
    EXPECT_FALSE(SensorHalDispatcher::GetInstance());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

// Test that the SensorHalDisptcher correctly re-establishes a Mojo channel
// for the client when the server crashes.
TEST_F(SensorHalDispatcherTest, ServerConnectionError) {
  // First verify that a the SensorHalDispatcher establishes a Mojo channel
  // between the server and the client.
  auto fake_server = std::make_unique<FakeSensorHalServer>();
  auto fake_client = std::make_unique<FakeSensorHalClient>();

  auto remote_server = fake_server->PassRemote();
  auto remote_client = fake_client->PassRemote();

  SensorHalDispatcher::GetInstance()->RegisterServer(std::move(remote_server));
  SensorHalDispatcher::GetInstance()->RegisterClient(std::move(remote_client));

  // Wait until the server and client get the established Mojo channel.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_server->GetSensorService()->HasReceivers());
  EXPECT_TRUE(fake_client->SensorServiceIsValid());

  // Re-create a new server to simulate a server crash.
  fake_server = std::make_unique<FakeSensorHalServer>();
  fake_client->ResetSensorService();

  // Wait until the dispatcher resets the server and client remotes.
  base::RunLoop().RunUntilIdle();

  remote_server = fake_server->PassRemote();
  SensorHalDispatcher::GetInstance()->RegisterServer(std::move(remote_server));

  // Make sure we re-create the Mojo channel from the new server to the same
  // client.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_server->GetSensorService()->HasReceivers());
  EXPECT_TRUE(fake_client->SensorServiceIsValid());
}

// Test that the SensorHalDisptcher correctly re-establishes a Mojo channel
// for the client when the client reconnects after crash.
TEST_F(SensorHalDispatcherTest, ClientConnectionError) {
  // First verify that a the SensorHalDispatcher establishes a Mojo channel
  // between the server and the client.
  auto fake_server = std::make_unique<FakeSensorHalServer>();
  auto fake_client = std::make_unique<FakeSensorHalClient>();

  auto remote_server = fake_server->PassRemote();
  auto remote_client = fake_client->PassRemote();

  SensorHalDispatcher::GetInstance()->RegisterServer(std::move(remote_server));
  SensorHalDispatcher::GetInstance()->RegisterClient(std::move(remote_client));

  // Wait until the server and client get the established Mojo channel.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_server->GetSensorService()->HasReceivers());
  EXPECT_TRUE(fake_client->SensorServiceIsValid());

  // Re-create a new client to simulate a client crash.
  fake_client = std::make_unique<FakeSensorHalClient>();
  fake_server->GetSensorService()->ClearReceivers();

  remote_client = fake_client->PassRemote();
  SensorHalDispatcher::GetInstance()->RegisterClient(std::move(remote_client));

  // Make sure we re-create the Mojo channel from the same server to the new
  // client.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fake_server->GetSensorService()->HasReceivers());
  EXPECT_TRUE(fake_client->SensorServiceIsValid());
}

}  // namespace sensors
}  // namespace chromeos
