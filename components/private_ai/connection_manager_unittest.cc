// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_manager.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/connection.h"
#include "components/private_ai/connection_factory.h"
#include "components/private_ai/status_code.h"
#include "components/private_ai/testing/fake_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

class FakeConnectionFactory : public ConnectionFactory {
 public:
  FakeConnectionFactory() = default;
  ~FakeConnectionFactory() override = default;

  std::unique_ptr<Connection> Create(
      base::RepeatingCallback<void(StatusCode)> on_disconnect) override {
    auto connection = std::make_unique<FakeConnection>(
        base::BindRepeating(&FakeConnectionFactory::on_disconnect,
                            base::Unretained(this), on_disconnect),
        base::BindOnce(&FakeConnectionFactory::OnConnectionDestroyed,
                       base::Unretained(this), std::move(on_destruction_)));
    last_connection_ = connection.get();
    return connection;
  }

  FakeConnection* last_connection() { return last_connection_; }

  void on_disconnect(base::RepeatingCallback<void(StatusCode)> callback,
                     StatusCode status_code) {
    callback.Run(status_code);

    // Execute internal on_disconnect callback as well.
    if (on_disconnect_callback_) {
      std::move(on_disconnect_callback_).Run();
    }
  }

  void set_on_disconnect(base::OnceClosure on_disconnect) {
    on_disconnect_callback_ = std::move(on_disconnect);
  }

  void set_on_destruction(base::OnceClosure on_destruction) {
    on_destruction_ = std::move(on_destruction);
  }

 private:
  void OnConnectionDestroyed(base::OnceClosure callback) {
    last_connection_ = nullptr;
    if (callback) {
      std::move(callback).Run();
    }
  }

  raw_ptr<FakeConnection> last_connection_ = nullptr;

  base::OnceClosure on_disconnect_callback_;
  base::OnceClosure on_destruction_;
};

}  // namespace

class ConnectionManagerTest : public ::testing::Test {
 public:
  ConnectionManagerTest() {
    auto factory = std::make_unique<FakeConnectionFactory>();
    factory_ = factory.get();
    manager_ =
        std::make_unique<ConnectionManager>(std::move(factory), &logger_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  PrivateAiLogger logger_;
  std::unique_ptr<ConnectionManager> manager_;
  raw_ptr<FakeConnectionFactory> factory_;
};

TEST_F(ConnectionManagerTest, GetConnectionCreatesConnection) {
  EXPECT_EQ(factory_->last_connection(), nullptr);
  Connection* connection = manager_->GetConnection();
  EXPECT_NE(connection, nullptr);
  EXPECT_EQ(connection, factory_->last_connection());
}

TEST_F(ConnectionManagerTest, GetConnectionReturnsExistingConnection) {
  Connection* connection1 = manager_->GetConnection();
  Connection* connection2 = manager_->GetConnection();
  EXPECT_EQ(connection1, connection2);
}

TEST_F(ConnectionManagerTest, ConnectionRecreatedAfterDisconnect) {
  Connection* connection1 = manager_->GetConnection();
  FakeConnection* fake_connection1 = factory_->last_connection();

  base::test::TestFuture<void> disconnect_future;
  factory_->set_on_disconnect(disconnect_future.GetCallback());

  fake_connection1->SimulateDisconnect();
  EXPECT_TRUE(disconnect_future.Wait());

  Connection* connection2 = manager_->GetConnection();
  EXPECT_NE(connection1, connection2);
  EXPECT_EQ(connection2, factory_->last_connection());
}

TEST_F(ConnectionManagerTest, ConnectionDestroyedAsynchronously) {
  base::test::TestFuture<void> destruction_future;
  factory_->set_on_destruction(destruction_future.GetCallback());

  manager_->GetConnection();
  FakeConnection* fake_connection = factory_->last_connection();

  fake_connection->SimulateDisconnect();

  // Connection should not be destroyed yet.
  EXPECT_FALSE(destruction_future.IsReady());

  // Waiting until connection is destroyed.
  EXPECT_TRUE(destruction_future.Wait());
}

TEST_F(ConnectionManagerTest,
       PendingDestructionConnectionsDestroyedOnManagerDtor) {
  base::test::TestFuture<void> destruction_future;
  factory_->set_on_destruction(destruction_future.GetCallback());

  manager_->GetConnection();
  FakeConnection* fake_connection = factory_->last_connection();

  fake_connection->SimulateDisconnect();

  // Connection should not be destroyed yet.
  EXPECT_FALSE(destruction_future.IsReady());

  // Destroying the manager should destroy the pending connection.
  factory_ = nullptr;
  manager_.reset();
  EXPECT_TRUE(destruction_future.IsReady());
}

TEST_F(ConnectionManagerTest,
       OnlyFirstDisconnectFromSameConnectionIsProcessed) {
  manager_->GetConnection();
  FakeConnection* fake_connection = factory_->last_connection();

  base::test::TestFuture<void> disconnect_future;
  factory_->set_on_disconnect(disconnect_future.GetCallback());

  // First disconnect should be processed.
  fake_connection->SimulateDisconnect();
  EXPECT_TRUE(disconnect_future.Wait());

  // Subsequent disconnect from the same connection should be ignored.
  // We can't easily check it was ignored other than ensuring no crash or
  // unexpected side effects.
  fake_connection->SimulateDisconnect();
}

TEST_F(ConnectionManagerTest, OldConnectionCannotDisconnectNewOne) {
  manager_->GetConnection();
  FakeConnection* fake_connection1 = factory_->last_connection();

  base::test::TestFuture<void> disconnect_future;
  factory_->set_on_disconnect(disconnect_future.GetCallback());

  // Simulate disconnect for the first connection.
  fake_connection1->SimulateDisconnect();
  EXPECT_TRUE(disconnect_future.Wait());

  // Create a second connection.
  Connection* connection2 = manager_->GetConnection();
  FakeConnection* fake_connection2 = factory_->last_connection();
  EXPECT_NE(fake_connection1, fake_connection2);

  // Subsequent disconnect from the first connection should not disconnect the
  // second one.
  fake_connection1->SimulateDisconnect();
  EXPECT_EQ(manager_->GetConnection(), connection2);
}

}  // namespace private_ai
