// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/common/blocking_method_caller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

namespace {

class FakeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  // base::SingleThreadTaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    std::move(task).Run();
    return true;
  }
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return PostDelayedTask(from_here, std::move(task), delay);
  }
  bool RunsTasksInCurrentSequence() const override { return true; }

 protected:
  ~FakeTaskRunner() override = default;
};

}  // namespace

class BlockingMethodCallerTest : public testing::Test {
 public:
  BlockingMethodCallerTest() : task_runner_(new FakeTaskRunner) {}

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock proxy.
    mock_proxy_ =
        new dbus::MockObjectProxy(mock_bus_.get(), "org.chromium.TestService",
                                  dbus::ObjectPath("/org/chromium/TestObject"));

    // Set an expectation so mock_proxy's CallMethodAndBlock() will use
    // CreateMockProxyResponse() to return responses.
    EXPECT_CALL(*mock_proxy_.get(), CallMethodAndBlock(_, _))
        .WillRepeatedly(
            Invoke(this, &BlockingMethodCallerTest::CreateMockProxyResponse));

    // Set an expectation so mock_bus's GetObjectProxy() for the given
    // service name and the object path will return mock_proxy_.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy("org.chromium.TestService",
                               dbus::ObjectPath("/org/chromium/TestObject")))
        .WillOnce(Return(mock_proxy_.get()));

    // Set an expectation so mock_bus's GetDBusTaskRunner will return the fake
    // task runner.
    EXPECT_CALL(*mock_bus_.get(), GetDBusTaskRunner())
        .WillRepeatedly(Return(task_runner_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());
  }

  void TearDown() override { mock_bus_->ShutdownAndBlock(); }

 protected:
  scoped_refptr<FakeTaskRunner> task_runner_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;

 private:
  // Returns a response for the given method call. Used to implement
  // CallMethodAndBlock() for |mock_proxy_|.
  base::expected<std::unique_ptr<dbus::Response>, dbus::Error>
  CreateMockProxyResponse(dbus::MethodCall* method_call, int timeout_ms) {
    if (method_call->GetInterface() == "org.chromium.TestInterface" &&
        method_call->GetMember() == "Echo") {
      dbus::MessageReader reader(method_call);
      std::string text_message;
      if (reader.PopString(&text_message)) {
        std::unique_ptr<dbus::Response> response =
            dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendString(text_message);
        return base::ok(std::move(response));
      }
    }

    LOG(ERROR) << "Unexpected method call: " << method_call->ToString();
    return base::unexpected(dbus::Error());
  }
};

TEST_F(BlockingMethodCallerTest, Echo) {
  const char kHello[] = "Hello";
  // Get an object proxy from the mock bus.
  dbus::ObjectProxy* proxy = mock_bus_->GetObjectProxy(
      "org.chromium.TestService", dbus::ObjectPath("/org/chromium/TestObject"));

  // Create a method call.
  dbus::MethodCall method_call("org.chromium.TestInterface", "Echo");
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kHello);

  // Call the method.
  BlockingMethodCaller blocking_method_caller(mock_bus_.get(), proxy);
  auto result = blocking_method_caller.CallMethodAndBlock(&method_call);
  ASSERT_TRUE(result.has_value());

  // Check the response.
  ASSERT_TRUE(result->get());
  dbus::MessageReader reader(result->get());
  std::string text_message;
  ASSERT_TRUE(reader.PopString(&text_message));
  // The text message should be echo'ed back.
  EXPECT_EQ(kHello, text_message);
}

}  // namespace chromeos
