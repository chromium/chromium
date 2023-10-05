// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hiberman/hiberman_client.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/hiberman/dbus-constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace ash {

class HibermanClientTest : public testing::Test {
 public:
  HibermanClientTest() = default;
  ~HibermanClientTest() override = default;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);

    dbus::ObjectPath hibernate_object_path =
        dbus::ObjectPath(::hiberman::kHibernateServicePath);
    proxy_ = new dbus::MockObjectProxy(
        bus_.get(), ::hiberman::kHibernateServiceName, hibernate_object_path);

    dbus_service_proxy_ = new dbus::MockObjectProxy(
        bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));

    // Makes sure `GetObjectProxy()` is caled with the correct service name and
    // path.
    EXPECT_CALL(*bus_.get(),
                GetObjectProxy(::hiberman::kHibernateServiceName,
                               hibernate_object_path))
        .WillRepeatedly(Return(proxy_.get()));

    EXPECT_CALL(*proxy_.get(), DoCallMethod(_, _, _))
        .WillRepeatedly(Invoke(this, &HibermanClientTest::OnCallMethod));

    HibermanClient::Initialize(bus_.get());

    // Execute callbacks posted by `client_->Init()`.
    base::RunLoop().RunUntilIdle();

    client_ = HibermanClient::Get();
  }

  void TearDown() override { HibermanClient::Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock bus and proxy for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;
  scoped_refptr<dbus::MockObjectProxy> dbus_service_proxy_;

  // Convenience pointer to the global instance.
  raw_ptr<HibermanClient, ExperimentalAsh> client_ = nullptr;

 private:
  // Handles calls to |proxy_|'s `CallMethod()`.
  void OnCallMethod(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback* callback) {
    std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
    dbus::MessageWriter writer(response.get());
    if (method_call->GetMember() == ::hiberman::kResumeFromHibernateASMethod) {
      dbus::MessageReader reader(method_call);
      std::string account_id;
      const uint8_t* bytes = nullptr;
      size_t length = 0;
      // The ResumeFromHibernateAS method should have an auth_session_id byte
      // array.
      EXPECT_TRUE(reader.PopArrayOfBytes(&bytes, &length));
      EXPECT_NE(length, static_cast<size_t>(0));
      // The ResumeFromHibernateAS method should have an account_id string.
      ASSERT_TRUE(reader.PopString(&account_id));
    } else if (method_call->GetMember() == "NameHasOwner") {
      dbus::MessageReader reader(method_call);
      std::string name;
      ASSERT_TRUE(reader.PopString(&name));
      ASSERT_EQ(name, ::hiberman::kHibernateServiceName);
    } else {
      ASSERT_FALSE(true) << "Unrecognized member: " << method_call->GetMember();
    }
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](dbus::ObjectProxy::ResponseCallback callback,
               std::unique_ptr<dbus::Response> response)
               {std::move(callback).Run(response.get()); },
            std::move(*callback), std::move(response)));
  }
};

}  // namespace ash
