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
#include "components/private_ai/error_code.h"
#include "components/private_ai/testing/fake_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

class FakeConnectionFactory : public ConnectionFactory {
 public:
  FakeConnectionFactory() = default;
  ~FakeConnectionFactory() override = default;

  std::unique_ptr<Connection> Create(
      base::OnceCallback<void(ErrorCode)> on_disconnect) override {
    auto connection = std::make_unique<FakeConnection>(
        base::BindOnce(&FakeConnectionFactory::on_disconnect,
                       base::Unretained(this), std::move(on_disconnect)));
    last_connection_ = connection.get();
    return connection;
  }

  FakeConnection* last_connection() { return last_connection_; }

  void on_disconnect(base::OnceCallback<void(ErrorCode)> callback,
                     ErrorCode error_code) {
    std::move(callback).Run(error_code);

    // Execute internal on_disconnect callback as well.
    std::move(on_disconnect_).Run();
  }

  void set_on_disconnect(base::OnceClosure on_disconnect) {
    on_disconnect_ = std::move(on_disconnect);
  }

 private:
  raw_ptr<FakeConnection> last_connection_ = nullptr;

  base::OnceClosure on_disconnect_;
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

}  // namespace private_ai
