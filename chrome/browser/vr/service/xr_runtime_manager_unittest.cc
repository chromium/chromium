// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/service/vr_service_impl.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/test/fake_vr_device_provider.h"
#include "device/vr/test/fake_vr_service_client.h"
#include "device/vr/vr_device_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {

class XRRuntimeManagerForTesting : public XRRuntimeManager {
 public:
  explicit XRRuntimeManagerForTesting(ProviderList providers)
      : XRRuntimeManager(std::move(providers)) {}
  ~XRRuntimeManagerForTesting() override = default;

  size_t NumberOfConnectedServices() {
    return XRRuntimeManager::NumberOfConnectedServices();
  }

  // Expose this test-only method as public for tests.
  using XRRuntimeManager::GetRuntimeForTest;
};

class VRServiceImplForTesting : public VRServiceImpl {
 public:
  VRServiceImplForTesting() : VRServiceImpl() {}
  ~VRServiceImplForTesting() override = default;
};

}  // namespace

class XRRuntimeManagerTest : public testing::Test {
 public:
  static void onDeviceReturned(device::mojom::XRDevicePtr ptr) {}

 protected:
  XRRuntimeManagerTest() = default;
  ~XRRuntimeManagerTest() override = default;

  void SetUp() override {
    std::vector<std::unique_ptr<device::VRDeviceProvider>> providers;
    provider_ = new device::FakeVRDeviceProvider();
    providers.emplace_back(
        std::unique_ptr<device::FakeVRDeviceProvider>(provider_));
    new XRRuntimeManagerForTesting(std::move(providers));
  }

  void TearDown() override { EXPECT_FALSE(XRRuntimeManager::HasInstance()); }

  std::unique_ptr<VRServiceImplForTesting> BindService() {
    device::mojom::VRServiceClientPtr proxy;
    device::FakeVRServiceClient client(mojo::MakeRequest(&proxy));
    auto service = base::WrapUnique(new VRServiceImplForTesting());
    XRRuntimeManager::GetInstance()->AddService(service.get());
    service->RequestDevice(
        base::BindRepeating(&XRRuntimeManagerTest::onDeviceReturned));
    service->SetClient(std::move(proxy));
    return service;
  }

  XRRuntimeManagerForTesting* DeviceManager() {
    EXPECT_TRUE(XRRuntimeManager::HasInstance());
    return static_cast<XRRuntimeManagerForTesting*>(
        XRRuntimeManager::GetInstance());
  }

  size_t ServiceCount() { return DeviceManager()->NumberOfConnectedServices(); }

  device::FakeVRDeviceProvider* Provider() {
    EXPECT_TRUE(XRRuntimeManager::HasInstance());
    return provider_;
  }

 private:
  device::FakeVRDeviceProvider* provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(XRRuntimeManagerTest);
};

TEST_F(XRRuntimeManagerTest, InitializationTest) {
  EXPECT_FALSE(Provider()->Initialized());

  // Calling GetDevices should initialize the service if it hasn't been
  // initialized yet or the providesr have been released.
  // The mojom::VRService should initialize each of it's providers upon it's own
  // initialization. And SetClient method in VRService class will invoke
  // GetVRDevices too.
  auto service = BindService();
  EXPECT_TRUE(Provider()->Initialized());
}

TEST_F(XRRuntimeManagerTest, GetNoDevicesTest) {
  auto service = BindService();
  // Calling GetVRDevices should initialize the providers.
  EXPECT_TRUE(Provider()->Initialized());

  // GetDeviceByIndex should return nullptr if an invalid index in queried.
  device::mojom::XRRuntime* queried_device = DeviceManager()->GetRuntimeForTest(
      device::mojom::XRDeviceId::GVR_DEVICE_ID);
  EXPECT_EQ(nullptr, queried_device);
}

// Ensure that services are registered with the device manager as they are
// created and removed from the device manager as their connections are closed.
TEST_F(XRRuntimeManagerTest, DeviceManagerRegistration) {
  EXPECT_EQ(0u, ServiceCount());
  auto service_1 = BindService();
  EXPECT_EQ(1u, ServiceCount());
  auto service_2 = BindService();
  EXPECT_EQ(2u, ServiceCount());
  service_1.reset();
  EXPECT_EQ(1u, ServiceCount());
  service_2.reset();
  EXPECT_FALSE(XRRuntimeManager::HasInstance());
}

// Ensure that devices added and removed are reflected in calls to request
// sessions.
TEST_F(XRRuntimeManagerTest, AddRemoveDevices) {
  auto service = BindService();
  EXPECT_EQ(1u, ServiceCount());
  EXPECT_TRUE(Provider()->Initialized());
  device::FakeVRDevice* device =
      new device::FakeVRDevice(device::mojom::XRDeviceId::ARCORE_DEVICE_ID);
  Provider()->AddDevice(base::WrapUnique(device));

  device::mojom::XRSessionOptions options = {};
  options.provide_passthrough_camera = true;
  EXPECT_TRUE(DeviceManager()->GetRuntimeForOptions(&options));
  Provider()->RemoveDevice(device->GetId());
  EXPECT_TRUE(!DeviceManager()->GetRuntimeForOptions(&options));
}

}  // namespace vr
