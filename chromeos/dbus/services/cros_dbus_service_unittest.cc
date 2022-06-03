// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/services/cros_dbus_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

namespace chromeos {

class MockProxyResolutionService
    : public CrosDBusService::ServiceProviderInterface {
 public:
  MOCK_METHOD1(Start, void(scoped_refptr<dbus::ExportedObject>
                           exported_object));
};

class CrosDBusServiceTest : public testing::Test {
 public:
  CrosDBusServiceTest() = default;

  // Creates an instance of CrosDBusService with mocks injected.
  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create a mock exported object.
    const dbus::ObjectPath kObjectPath("/org/example/TestService");
    mock_exported_object_ =
        new dbus::MockExportedObject(mock_bus_.get(), kObjectPath);

    // |mock_bus_|'s GetExportedObject() will return mock_exported_object_|
    // for the given service name and the object path.
    EXPECT_CALL(*mock_bus_.get(), GetExportedObject(kObjectPath))
        .WillRepeatedly(Return(mock_exported_object_.get()));

    // Create a mock proxy resolution service.
    auto mock_proxy_resolution_service_provider =
        std::make_unique<MockProxyResolutionService>();

    const char kServiceName[] = "org.example.TestService";

    {
      // |mock_exported_object_|'s Start method should be called before
      // RequestOwnership: https://crbug.com/874978
      InSequence export_methods_before_taking_ownership;
      EXPECT_CALL(*mock_proxy_resolution_service_provider,
                  Start(Eq(mock_exported_object_)))
          .WillOnce(Return());
      EXPECT_CALL(
          *mock_bus_.get(),
          RequestOwnership(kServiceName,
                           dbus::Bus::REQUIRE_PRIMARY_ALLOW_REPLACEMENT, _))
          .Times(1);
    }

    // Initialize the cros service with the mocks injected.
    CrosDBusService::ServiceProviderList service_providers;
    service_providers.push_back(
        std::move(mock_proxy_resolution_service_provider));
    cros_dbus_service_ = CrosDBusService::CreateRealImpl(
        mock_bus_.get(), kServiceName, kObjectPath,
        std::move(service_providers));
  }

  void TearDown() override {
    cros_dbus_service_.reset();
    mock_bus_->ShutdownAndBlock();

    // Clear expectations to allow |mock_bus_| to be destroyed:
    // http://go/soverflow/10286514
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(mock_bus_.get()));
  }

 protected:
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  std::unique_ptr<CrosDBusService> cros_dbus_service_;
};

TEST_F(CrosDBusServiceTest, Start) {
  // Simply start the service and see if mock expectations are met:
  // - The service object is exported by GetExportedObject()
  // - The proxy resolution service is started.
}

}  // namespace chromeos
