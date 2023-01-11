// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/gatt_client_manager_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic_impl.h"
#include "chromecast/device/bluetooth/le/remote_descriptor_impl.h"
#include "chromecast/device/bluetooth/le/remote_device_impl.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "chromecast/device/bluetooth/shlib/mock_gatt_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace chromecast {
namespace bluetooth {

namespace {

const bluetooth_v2_shlib::Addr kTestAddr1 = {
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
const bluetooth_v2_shlib::Addr kTestAddr2 = {
    {0x10, 0x11, 0x12, 0x13, 0x14, 0x15}};
const bluetooth_v2_shlib::Addr kTestAddr3 = {
    {0x20, 0x21, 0x22, 0x23, 0x24, 0x25}};
const bluetooth_v2_shlib::Addr kTestAddr4 = {
    {0x30, 0x31, 0x32, 0x33, 0x34, 0x35}};
const bluetooth_v2_shlib::Addr kTestAddr5 = {
    {0x40, 0x41, 0x42, 0x43, 0x44, 0x45}};

class MockGattClientManagerObserver : public GattClientManager::Observer {
 public:
  MOCK_METHOD2(OnConnectChanged,
               void(scoped_refptr<RemoteDevice> device, bool connected));
  MOCK_METHOD2(OnMtuChanged, void(scoped_refptr<RemoteDevice> device, int mtu));
  MOCK_METHOD2(OnServicesUpdated,
               void(scoped_refptr<RemoteDevice> device,
                    std::vector<scoped_refptr<RemoteService>> services));
  MOCK_METHOD3(OnCharacteristicNotification,
               void(scoped_refptr<RemoteDevice> device,
                    scoped_refptr<RemoteCharacteristic> characteristic,
                    std::vector<uint8_t> value));
};

std::vector<bluetooth_v2_shlib::Gatt::Service> GenerateServices() {
  std::vector<bluetooth_v2_shlib::Gatt::Service> ret;

  bluetooth_v2_shlib::Gatt::Service service;
  bluetooth_v2_shlib::Gatt::Characteristic characteristic;
  bluetooth_v2_shlib::Gatt::Descriptor descriptor;

  service.uuid = {{0x1}};
  service.handle = 0x1;
  service.primary = true;

  // Generate a characteristic that supports notification only.
  characteristic.uuid = {{0x1, 0x1}};
  characteristic.handle = 0x2;
  characteristic.permissions =
      static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
          bluetooth_v2_shlib::Gatt::PERMISSION_READ |
          bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.properties = bluetooth_v2_shlib::Gatt::PROPERTY_NOTIFY;

  descriptor.uuid = {{0x1, 0x1, 0x1}};
  descriptor.handle = 0x3;
  descriptor.permissions = static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
      bluetooth_v2_shlib::Gatt::PERMISSION_READ |
      bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.descriptors.push_back(descriptor);

  descriptor.uuid = RemoteDescriptor::kCccdUuid;
  descriptor.handle = 0x4;
  descriptor.permissions = static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
      bluetooth_v2_shlib::Gatt::PERMISSION_READ |
      bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.descriptors.push_back(descriptor);
  service.characteristics.push_back(characteristic);

  // Generate a characteristic that does not support notification or indication.
  characteristic.uuid = {{0x1, 0x2}};
  characteristic.handle = 0x5;
  characteristic.permissions =
      static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
          bluetooth_v2_shlib::Gatt::PERMISSION_READ |
          bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.properties =
      static_cast<bluetooth_v2_shlib::Gatt::Properties>(0);
  characteristic.descriptors.clear();
  service.characteristics.push_back(characteristic);

  // Generate a characteristic that supports indication only.
  characteristic.uuid = {{0x1, 0x3}};
  characteristic.handle = 0x6;
  characteristic.permissions =
      static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
          bluetooth_v2_shlib::Gatt::PERMISSION_READ |
          bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.properties = bluetooth_v2_shlib::Gatt::PROPERTY_INDICATE;

  descriptor.uuid = {{0x1, 0x3, 0x1}};
  descriptor.handle = 0x7;
  descriptor.permissions = static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
      bluetooth_v2_shlib::Gatt::PERMISSION_READ |
      bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.descriptors.push_back(descriptor);

  descriptor.uuid = RemoteDescriptor::kCccdUuid;
  descriptor.handle = 0x8;
  descriptor.permissions = static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
      bluetooth_v2_shlib::Gatt::PERMISSION_READ |
      bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.descriptors.push_back(descriptor);
  service.characteristics.push_back(characteristic);

  // Generate a characteristic that supports both notification and indication.
  characteristic.uuid = {{0x1, 0x4}};
  characteristic.handle = 0x9;
  characteristic.permissions =
      static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
          bluetooth_v2_shlib::Gatt::PERMISSION_READ |
          bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.properties = static_cast<bluetooth_v2_shlib::Gatt::Properties>(
      bluetooth_v2_shlib::Gatt::PROPERTY_NOTIFY |
      bluetooth_v2_shlib::Gatt::PROPERTY_INDICATE);
  characteristic.descriptors.clear();

  descriptor.uuid = {{0x1, 0x4, 0x1}};
  descriptor.handle = 0xA;
  descriptor.permissions = static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
      bluetooth_v2_shlib::Gatt::PERMISSION_READ |
      bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.descriptors.push_back(descriptor);

  descriptor.uuid = RemoteDescriptor::kCccdUuid;
  descriptor.handle = 0xB;
  descriptor.permissions = static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
      bluetooth_v2_shlib::Gatt::PERMISSION_READ |
      bluetooth_v2_shlib::Gatt::PERMISSION_WRITE);
  characteristic.descriptors.push_back(descriptor);
  service.characteristics.push_back(characteristic);

  ret.push_back(service);

  service.uuid = {{0x2}};
  service.handle = 0xC;
  service.primary = true;
  service.characteristics.clear();
  ret.push_back(service);

  return ret;
}

class GattClientManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    gatt_client_ = std::make_unique<bluetooth_v2_shlib::MockGattClient>();
    gatt_client_manager_ =
        std::make_unique<GattClientManagerImpl>(gatt_client_.get());
    observer_ = std::make_unique<MockGattClientManagerObserver>();

    // Normally bluetooth_manager does this.
    gatt_client_->SetDelegate(gatt_client_manager_.get());
    gatt_client_manager_->Initialize(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    gatt_client_manager_->AddObserver(observer_.get());
  }

  void TearDown() override {
    gatt_client_->SetDelegate(nullptr);
    gatt_client_manager_->RemoveObserver(observer_.get());
    gatt_client_manager_->Finalize();
  }

  scoped_refptr<RemoteDevice> GetDevice(const bluetooth_v2_shlib::Addr& addr) {
    scoped_refptr<RemoteDevice> ret;
    gatt_client_manager_->GetDevice(
        addr, base::BindOnce(
                  [](scoped_refptr<RemoteDevice>* ret_ptr,
                     scoped_refptr<RemoteDevice> result) { *ret_ptr = result; },
                  &ret));

    return ret;
  }

  std::vector<scoped_refptr<RemoteService>> GetServices(RemoteDevice* device) {
    std::vector<scoped_refptr<RemoteService>> ret;
    device->GetServices(base::BindOnce(
        [](std::vector<scoped_refptr<RemoteService>>* ret_ptr,
           std::vector<scoped_refptr<RemoteService>> result) {
          *ret_ptr = result;
        },
        &ret));

    return ret;
  }

  scoped_refptr<RemoteService> GetServiceByUuid(
      RemoteDevice* device,
      const bluetooth_v2_shlib::Uuid& uuid) {
    scoped_refptr<RemoteService> ret;
    device->GetServiceByUuid(uuid, base::BindOnce(
                                       [](scoped_refptr<RemoteService>* ret_ptr,
                                          scoped_refptr<RemoteService> result) {
                                         *ret_ptr = result;
                                       },
                                       &ret));
    return ret;
  }

  void Connect(const bluetooth_v2_shlib::Addr& addr) {
    EXPECT_CALL(*gatt_client_,
                Connect(addr, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
      .WillOnce(Return(true));
    scoped_refptr<RemoteDevice> device = GetDevice(addr);
    EXPECT_CALL(connect_cb_, Run(RemoteDevice::ConnectStatus::kSuccess));
    device->Connect(connect_cb_.Get());
    bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
        gatt_client_->delegate();
    EXPECT_CALL(*gatt_client_, GetServices(addr)).WillOnce(Return(true));
    delegate->OnConnectChanged(addr, true /* status */, true /* connected */);
    delegate->OnGetServices(addr, {});
    ASSERT_TRUE(device->IsConnected());
  }

  base::MockCallback<RemoteDevice::StatusCallback> cb_;
  base::MockCallback<RemoteDevice::ConnectCallback> connect_cb_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<GattClientManagerImpl> gatt_client_manager_;
  std::unique_ptr<bluetooth_v2_shlib::MockGattClient> gatt_client_;
  std::unique_ptr<MockGattClientManagerObserver> observer_;
};

}  // namespace

TEST_F(GattClientManagerTest, RemoteDeviceConnect) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();

  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(gatt_client_manager_->IsConnectedLeDevice(kTestAddr1));
  EXPECT_EQ(kTestAddr1, device->addr());

  // Disconnect from an already disconnected device.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(false));
  EXPECT_CALL(*gatt_client_, ClearPendingDisconnect(kTestAddr1))
      .WillOnce(Return(true));
  EXPECT_CALL(cb_, Run(true));
  device->Disconnect(cb_.Get());

  // These should fail if we're not connected.
  EXPECT_CALL(cb_, Run(false));
  device->CreateBond(cb_.Get());

  base::MockCallback<RemoteDevice::RssiCallback> rssi_cb;
  EXPECT_CALL(rssi_cb, Run(false, _));
  device->ReadRemoteRssi(rssi_cb.Get());

  EXPECT_CALL(cb_, Run(false));
  device->RequestMtu(512, cb_.Get());

  EXPECT_CALL(cb_, Run(false));
  device->ConnectionParameterUpdate(10, 10, 50, 100, cb_.Get());

  // First connect request fails right away.
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr1, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(false));
  EXPECT_CALL(*gatt_client_, ClearPendingConnect(kTestAddr1))
      .WillOnce(Return(true));
  EXPECT_CALL(connect_cb_, Run(RemoteDevice::ConnectStatus::kFailure));
  device->Connect(connect_cb_.Get());
  EXPECT_FALSE(device->IsConnected());

  // Second connect request succeeds.
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr1, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  EXPECT_CALL(connect_cb_, Run(RemoteDevice::ConnectStatus::kSuccess));
  device->Connect(connect_cb_.Get());
  EXPECT_CALL(*gatt_client_, GetServices(kTestAddr1)).WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             true /* connected */);
  EXPECT_CALL(*observer_, OnConnectChanged(device, true));
  delegate->OnGetServices(kTestAddr1, {});

  EXPECT_TRUE(device->IsConnected());
  EXPECT_TRUE(gatt_client_manager_->IsConnectedLeDevice(kTestAddr1));

  base::MockCallback<
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteDevice>>)>>
      get_connected_callback;
  const std::vector<scoped_refptr<RemoteDevice>> kExpectedDevices({device});
  EXPECT_CALL(get_connected_callback, Run(kExpectedDevices));
  gatt_client_manager_->GetConnectedDevices(get_connected_callback.Get());

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
  device->Disconnect({});
  // Should declare device as not connected after disconnect starts
  EXPECT_FALSE(device->IsConnected());

  EXPECT_CALL(*observer_, OnConnectChanged(device, false));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             false /* connected */);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(gatt_client_manager_->IsConnectedLeDevice(kTestAddr1));

  task_environment_.RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceBond) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();

  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  Connect(kTestAddr1);
  EXPECT_FALSE(device->IsBonded());

  // CreateBond fails in the initial request.
  EXPECT_CALL(*gatt_client_, CreateBond(kTestAddr1)).WillOnce(Return(false));
  EXPECT_CALL(cb_, Run(false));
  device->CreateBond(cb_.Get());
  EXPECT_FALSE(device->IsBonded());

  // CreateBond fails in the callback.
  EXPECT_CALL(*gatt_client_, CreateBond(kTestAddr1)).WillOnce(Return(true));
  EXPECT_CALL(cb_, Run(false));
  device->CreateBond(cb_.Get());
  delegate->OnBondChanged(kTestAddr1, false /* status */, false /* bonded */);
  EXPECT_FALSE(device->IsBonded());

  // CreateBond succeeds.
  EXPECT_CALL(*gatt_client_, CreateBond(kTestAddr1)).WillOnce(Return(true));
  device->CreateBond(cb_.Get());
  EXPECT_CALL(cb_, Run(true));
  delegate->OnBondChanged(kTestAddr1, true /* status */, true /* bonded */);
  EXPECT_TRUE(device->IsBonded());

  // Bond with an already bonded device should fail.
  EXPECT_CALL(cb_, Run(false));
  device->CreateBond(cb_.Get());
  EXPECT_TRUE(device->IsBonded());

  // RemoveBond succeeds.
  EXPECT_CALL(*gatt_client_, RemoveBond(kTestAddr1)).WillOnce(Return(true));
  device->RemoveBond(cb_.Get());
  EXPECT_CALL(cb_, Run(true));
  delegate->OnBondChanged(kTestAddr1, false /* status */, false /* bonded */);
  EXPECT_FALSE(device->IsBonded());

  // RemoveBond from an unbonded device succeeds.
  EXPECT_CALL(*gatt_client_, RemoveBond(kTestAddr1)).WillOnce(Return(true));
  device->RemoveBond(cb_.Get());
  EXPECT_CALL(cb_, Run(true));
  delegate->OnBondChanged(kTestAddr1, false /* status */, false /* bonded */);
  EXPECT_FALSE(device->IsBonded());

  // CreateBond again succeeds.
  EXPECT_CALL(*gatt_client_, CreateBond(kTestAddr1)).WillOnce(Return(true));
  device->CreateBond(cb_.Get());
  EXPECT_CALL(cb_, Run(true));
  delegate->OnBondChanged(kTestAddr1, true /* status */, true /* bonded */);
  EXPECT_TRUE(device->IsBonded());

  // Device should remain bonded when it disconnects.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
  device->Disconnect({});
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             false /* connected */);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_TRUE(device->IsBonded());

  // RemoveBond from a disconnected but bonded device.
  EXPECT_CALL(*gatt_client_, RemoveBond(kTestAddr1)).WillOnce(Return(true));
  device->RemoveBond(cb_.Get());
  EXPECT_CALL(cb_, Run(true));
  delegate->OnBondChanged(kTestAddr1, false /* status */, false /* bonded */);
  EXPECT_FALSE(device->IsBonded());

  task_environment_.RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceBondedOnInitialization) {
  // NotifyBonded at initialization.
  gatt_client_manager_->NotifyBonded(kTestAddr1);

  // Device should have updated the bonding state.
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_TRUE(device->IsBonded());

  Connect(kTestAddr1);
  EXPECT_TRUE(device->IsConnected());
  EXPECT_TRUE(device->IsBonded());

  task_environment_.RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceConnectConcurrent) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device1 = GetDevice(kTestAddr1);
  scoped_refptr<RemoteDevice> device2 = GetDevice(kTestAddr2);
  scoped_refptr<RemoteDevice> device3 = GetDevice(kTestAddr3);
  scoped_refptr<RemoteDevice> device4 = GetDevice(kTestAddr4);
  scoped_refptr<RemoteDevice> device5 = GetDevice(kTestAddr5);

  base::MockCallback<RemoteDevice::ConnectCallback> cb1;
  base::MockCallback<RemoteDevice::ConnectCallback> cb2;
  base::MockCallback<RemoteDevice::ConnectCallback> cb3;
  base::MockCallback<RemoteDevice::ConnectCallback> cb4;
  base::MockCallback<RemoteDevice::StatusCallback> cb5;

  // Device5 is already connected at the beginning.
  Connect(kTestAddr5);

  // Only the 1st Connect request will be executed immediately. The rest will be
  // queued.
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr1, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  device1->Connect(cb1.Get());
  device2->Connect(cb2.Get());
  device3->Connect(cb3.Get());
  device4->Connect(cb4.Get());
  device5->Disconnect(cb5.Get());

  EXPECT_CALL(*gatt_client_, GetServices(kTestAddr1)).WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             true /* connected */);

  // Queued Connect requests will not be called until we receive OnGetServices
  // of the current Connect request if it is successful.
  EXPECT_CALL(cb1, Run(RemoteDevice::ConnectStatus::kSuccess));
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr2, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(false));
  EXPECT_CALL(cb2, Run(RemoteDevice::ConnectStatus::kFailure));
  // If the Connect request fails in the initial request (not in the callback),
  // the next queued request will be executed immediately.
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr3, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  delegate->OnGetServices(kTestAddr1, {});

  EXPECT_CALL(cb3, Run(RemoteDevice::ConnectStatus::kFailure));
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr4, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr3, true /* status */,
                             false /* connected */);

  EXPECT_CALL(*gatt_client_, GetServices(kTestAddr4)).WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr4, true /* status */,
                             true /* connected */);

  EXPECT_CALL(cb4, Run(RemoteDevice::ConnectStatus::kSuccess));
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr5)).WillOnce(Return(true));
  delegate->OnGetServices(kTestAddr4, {});

  EXPECT_CALL(cb5, Run(true));
  delegate->OnConnectChanged(kTestAddr5, true /* status */,
                             false /* connected */);

  EXPECT_TRUE(device1->IsConnected());
  EXPECT_FALSE(device2->IsConnected());
  EXPECT_FALSE(device3->IsConnected());
  EXPECT_TRUE(device4->IsConnected());
  EXPECT_FALSE(device5->IsConnected());

  base::MockCallback<base::OnceCallback<void(size_t)>>
      get_num_connected_callback;
  EXPECT_CALL(get_num_connected_callback, Run(2));
  gatt_client_manager_->GetNumConnected(get_num_connected_callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(GattClientManagerTest, ConnectTimeout) {
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);

  // Issue a Connect request
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr1, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  device->Connect(connect_cb_.Get());

  // Let Connect request timeout
  // We should expect to receive Connect failure message
  EXPECT_CALL(*gatt_client_, ClearPendingConnect(kTestAddr1))
      .WillOnce(Return(true));
  EXPECT_CALL(connect_cb_, Run(RemoteDevice::ConnectStatus::kFailure));
  task_environment_.FastForwardBy(GattClientManagerImpl::kConnectTimeout);
  EXPECT_FALSE(device->IsConnected());
}

TEST_F(GattClientManagerTest, GetServicesTimeout) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();

  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);

  // Issue a Connect request and let Connect succeed
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr1, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  device->Connect(connect_cb_.Get());
  EXPECT_CALL(*gatt_client_, GetServices(kTestAddr1)).WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             true /* connected */);

  // Let GetServices request timeout
  // We should request a disconnect.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
  task_environment_.FastForwardBy(GattClientManagerImpl::kConnectTimeout);

  // Make sure we issued a disconnect.
  testing::Mock::VerifyAndClearExpectations(gatt_client_.get());

  EXPECT_CALL(connect_cb_, Run(RemoteDevice::ConnectStatus::kFailure));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             false /* connected */);

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(GattClientManagerTest, RemoteDeviceReadRssi) {
  static const int kRssi = -34;

  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);

  Connect(kTestAddr1);
  base::MockCallback<RemoteDevice::RssiCallback> rssi_cb;

  // First ReadRemoteRssi request fails right away.
  EXPECT_CALL(*gatt_client_, ReadRemoteRssi(kTestAddr1))
      .WillOnce(Return(false));
  EXPECT_CALL(rssi_cb, Run(false, 0));
  device->ReadRemoteRssi(rssi_cb.Get());

  // Second ReadRemoteRssi request succeeds.
  EXPECT_CALL(*gatt_client_, ReadRemoteRssi(kTestAddr1)).WillOnce(Return(true));
  device->ReadRemoteRssi(rssi_cb.Get());

  EXPECT_CALL(rssi_cb, Run(true, kRssi));
  delegate->OnReadRemoteRssi(kTestAddr1, true /* status */, kRssi);
}

TEST_F(GattClientManagerTest, DisconnectAll) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  base::MockCallback<GattClientManagerImpl::StatusCallback> cb;

  // No connected devices, DisconnectAll should be successful.
  EXPECT_CALL(cb, Run(true));
  gatt_client_manager_->DisconnectAll(cb.Get());

  scoped_refptr<RemoteDevice> device1 = GetDevice(kTestAddr1);
  scoped_refptr<RemoteDevice> device2 = GetDevice(kTestAddr2);
  scoped_refptr<RemoteDevice> device3 = GetDevice(kTestAddr3);

  // Connect all 3 devices.
  Connect(kTestAddr1);
  Connect(kTestAddr2);
  Connect(kTestAddr3);

  // Disable GATT client connectability.
  EXPECT_TRUE(gatt_client_manager_->SetGattClientConnectable(false));
  EXPECT_FALSE(gatt_client_manager_->gatt_client_connectable());

  // Disconnect requests will be queued.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
  gatt_client_manager_->DisconnectAll(cb.Get());

  // cb will be run when last device got disconnected.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr2)).WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             false /* connected */);
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr3)).WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr2, true /* status */,
                             false /* connected */);

  // Shouldn't be able to enable connectability when DisconnectAll is pending.
  EXPECT_FALSE(gatt_client_manager_->SetGattClientConnectable(true));
  EXPECT_FALSE(gatt_client_manager_->gatt_client_connectable());

  EXPECT_CALL(cb, Run(true));
  delegate->OnConnectChanged(kTestAddr3, true /* status */,
                             false /* connected */);

  base::MockCallback<base::OnceCallback<void(size_t)>>
      get_num_connected_callback;
  EXPECT_CALL(get_num_connected_callback, Run(0));
  gatt_client_manager_->GetNumConnected(get_num_connected_callback.Get());

  // Re-enable connectability when DisconnectAll completes.
  EXPECT_TRUE(gatt_client_manager_->SetGattClientConnectable(true));
  EXPECT_TRUE(gatt_client_manager_->gatt_client_connectable());

  base::RunLoop().RunUntilIdle();
}

TEST_F(GattClientManagerTest, DisconnectAllTimeout) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  base::MockCallback<GattClientManagerImpl::StatusCallback> cb;

  scoped_refptr<RemoteDevice> device1 = GetDevice(kTestAddr1);
  scoped_refptr<RemoteDevice> device2 = GetDevice(kTestAddr2);
  Connect(kTestAddr1);
  Connect(kTestAddr2);

  // Issue a DisconnectAll request.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
  gatt_client_manager_->DisconnectAll(cb.Get());

  // Let the fist Disconnect request timeout
  EXPECT_CALL(*gatt_client_, ClearPendingDisconnect(kTestAddr1))
      .WillOnce(Return(true));

  // We should expect to receive DisconnectAll failure message
  EXPECT_CALL(cb, Run(false));
  // Run second Disconnect request in the queue.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr2)).WillOnce(Return(true));
  task_environment_.FastForwardBy(GattClientManagerImpl::kDisconnectTimeout);

  // We should treat device as disconnected for this unknown case
  EXPECT_FALSE(device1->IsConnected());

  // Second Disconnect request succeeds.
  delegate->OnConnectChanged(kTestAddr2, true /* status */,
                             false /* connected */);

  base::MockCallback<base::OnceCallback<void(size_t)>>
      get_num_connected_callback;
  EXPECT_CALL(get_num_connected_callback, Run(0));
  gatt_client_manager_->GetNumConnected(get_num_connected_callback.Get());
}

TEST_F(GattClientManagerTest, Connectability) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);

  // By default GATT client is connectable.
  EXPECT_TRUE(gatt_client_manager_->gatt_client_connectable());

  // Start a connection.
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr1, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  device->Connect(connect_cb_.Get());

  // Disable GATT client connectability while connection is pending.
  EXPECT_TRUE(gatt_client_manager_->SetGattClientConnectable(false));
  EXPECT_FALSE(gatt_client_manager_->gatt_client_connectable());

  // Expect to disconnect after receiving the connect callback.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             true /* connected */);

  EXPECT_CALL(connect_cb_, Run(RemoteDevice::ConnectStatus::kFailure));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             false /* connected */);
  ASSERT_FALSE(device->IsConnected());

  // Connect should fail when GATT client connectability is already disabled.
  EXPECT_CALL(*gatt_client_, Connect).Times(0);
  EXPECT_CALL(connect_cb_, Run(RemoteDevice::ConnectStatus::kFailure));
  device->Connect(connect_cb_.Get());
  ASSERT_FALSE(device->IsConnected());

  // Re-enable connectability.
  EXPECT_TRUE(gatt_client_manager_->SetGattClientConnectable(true));
  EXPECT_TRUE(gatt_client_manager_->gatt_client_connectable());

  // Connect succeeds.
  Connect(kTestAddr1);

  base::RunLoop().RunUntilIdle();
}

TEST_F(GattClientManagerTest, ReadRemoteRssiTimeout) {
  static const int kRssi = -34;

  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);

  Connect(kTestAddr1);

  // Issue a ReadRemoteRssi request.
  base::MockCallback<RemoteDevice::RssiCallback> rssi_cb;
  EXPECT_CALL(*gatt_client_, ReadRemoteRssi(kTestAddr1)).WillOnce(Return(true));
  device->ReadRemoteRssi(rssi_cb.Get());

  // Let ReadRemoteRssi request timeout.
  // We should expect to receive ReadRemoteRssi failure message.
  EXPECT_CALL(rssi_cb, Run(false, 0));
  task_environment_.FastForwardBy(
      GattClientManagerImpl::kReadRemoteRssiTimeout);

  // The following callback should be ignored.
  delegate->OnReadRemoteRssi(kTestAddr1, true /* status */, kRssi);

  // Device should remain connected.
  EXPECT_TRUE(device->IsConnected());
}

TEST_F(GattClientManagerTest, RemoteDeviceReadRssiConcurrent) {
  static const int kRssi1 = -34;
  static const int kRssi3 = -68;

  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device1 = GetDevice(kTestAddr1);
  scoped_refptr<RemoteDevice> device2 = GetDevice(kTestAddr2);
  scoped_refptr<RemoteDevice> device3 = GetDevice(kTestAddr3);

  base::MockCallback<RemoteDevice::RssiCallback> rssi_cb1;
  base::MockCallback<RemoteDevice::RssiCallback> rssi_cb2;
  base::MockCallback<RemoteDevice::RssiCallback> rssi_cb3;

  // Only the 1st ReadRemoteRssi request will be executed immediately. The rest
  // will be queued.
  EXPECT_CALL(*gatt_client_, ReadRemoteRssi(kTestAddr1)).WillOnce(Return(true));
  device1->ReadRemoteRssi(rssi_cb1.Get());
  device2->ReadRemoteRssi(rssi_cb2.Get());
  device3->ReadRemoteRssi(rssi_cb3.Get());

  // Queued ReadRemoteRssi requests will not be called until we receive
  // OnGetServices of the current Connect request if it is successful.
  EXPECT_CALL(rssi_cb1, Run(true, kRssi1));
  EXPECT_CALL(*gatt_client_, ReadRemoteRssi(kTestAddr2))
      .WillOnce(Return(false));
  EXPECT_CALL(rssi_cb2, Run(false, _));
  // If the ReadRemoteRssi request fails in the initial request (not in the
  // callback), the next queued request will be executed immediately.
  EXPECT_CALL(*gatt_client_, ReadRemoteRssi(kTestAddr3)).WillOnce(Return(true));
  delegate->OnReadRemoteRssi(kTestAddr1, true, kRssi1);

  EXPECT_CALL(rssi_cb3, Run(true, kRssi3));
  delegate->OnReadRemoteRssi(kTestAddr3, true, kRssi3);
}

TEST_F(GattClientManagerTest, RemoteDeviceRequestMtu) {
  static const int kMtu = 512;
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  Connect(kTestAddr1);

  EXPECT_EQ(RemoteDevice::kDefaultMtu, device->GetMtu());
  EXPECT_CALL(*gatt_client_, RequestMtu(kTestAddr1, kMtu))
      .WillOnce(Return(true));
  EXPECT_CALL(cb_, Run(true));
  device->RequestMtu(kMtu, cb_.Get());
  EXPECT_CALL(*observer_, OnMtuChanged(device, kMtu));
  delegate->OnMtuChanged(kTestAddr1, true, kMtu);
  EXPECT_EQ(kMtu, device->GetMtu());
  task_environment_.RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceConnectionParameterUpdate) {
  const int kMinInterval = 10;
  const int kMaxInterval = 10;
  const int kLatency = 50;
  const int kTimeout = 100;

  Connect(kTestAddr1);

  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  EXPECT_CALL(*gatt_client_,
              ConnectionParameterUpdate(kTestAddr1, kMinInterval, kMaxInterval,
                                        kLatency, kTimeout))
      .WillOnce(Return(true));
  EXPECT_CALL(cb_, Run(true));
  device->ConnectionParameterUpdate(kMinInterval, kMaxInterval, kLatency,
                                    kTimeout, cb_.Get());
}

TEST_F(GattClientManagerTest, RemoteDeviceServices) {
  const auto kServices = GenerateServices();
  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  std::vector<scoped_refptr<RemoteService>> services;
  EXPECT_EQ(0ul, GetServices(device.get()).size());

  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kServices.size(), GetServices(device.get()).size());
  for (const auto& service : kServices) {
    scoped_refptr<RemoteService> remote_service =
        GetServiceByUuid(device.get(), service.uuid);
    ASSERT_TRUE(remote_service);
    EXPECT_EQ(service.uuid, remote_service->uuid());
    EXPECT_EQ(service.handle, remote_service->handle());
    EXPECT_EQ(service.primary, remote_service->primary());
    EXPECT_EQ(service.characteristics.size(),
              remote_service->GetCharacteristics().size());

    for (const auto& characteristic : service.characteristics) {
      scoped_refptr<RemoteCharacteristic> remote_char =
          remote_service->GetCharacteristicByUuid(characteristic.uuid);
      ASSERT_TRUE(remote_char);
      EXPECT_EQ(characteristic.uuid, remote_char->uuid());
      EXPECT_EQ(characteristic.handle, remote_char->handle());
      EXPECT_EQ(characteristic.permissions, remote_char->permissions());
      EXPECT_EQ(characteristic.properties, remote_char->properties());
      EXPECT_EQ(characteristic.descriptors.size(),
                remote_char->GetDescriptors().size());

      for (const auto& descriptor : characteristic.descriptors) {
        scoped_refptr<RemoteDescriptor> remote_desc =
            remote_char->GetDescriptorByUuid(descriptor.uuid);
        ASSERT_TRUE(remote_desc);
        EXPECT_EQ(descriptor.uuid, remote_desc->uuid());
        EXPECT_EQ(descriptor.handle, remote_desc->handle());
        EXPECT_EQ(descriptor.permissions, remote_desc->permissions());
      }
    }
  }
}

TEST_F(GattClientManagerTest, RemoteDeviceCharacteristic) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};
  const std::vector<uint8_t> kTestData2 = {0x4, 0x5, 0x6};
  const std::vector<uint8_t> kTestData3 = {0x7, 0x8, 0x9};
  const auto kServices = GenerateServices();
  const bluetooth_v2_shlib::Gatt::Client::AuthReq kAuthReq =
      bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_MITM;
  const bluetooth_v2_shlib::Gatt::WriteType kWriteType =
      bluetooth_v2_shlib::Gatt::WRITE_TYPE_DEFAULT;

  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  auto service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_GE(characteristics.size(), 1ul);
  ASSERT_TRUE(characteristics[0]);
  auto* characteristic =
      static_cast<RemoteCharacteristicImpl*>(characteristics[0].get());

  EXPECT_CALL(*gatt_client_,
              WriteCharacteristic(kTestAddr1, characteristic->characteristic(),
                                  kAuthReq, kWriteType, kTestData1))
      .WillOnce(Return(true));

  EXPECT_CALL(cb_, Run(true));
  characteristic->WriteAuth(kAuthReq, kWriteType, kTestData1, cb_.Get());
  delegate->OnCharacteristicWriteResponse(kTestAddr1, true,
                                          characteristic->handle());

  EXPECT_CALL(*gatt_client_,
              ReadCharacteristic(kTestAddr1, characteristic->characteristic(),
                                 kAuthReq))
      .WillOnce(Return(true));

  base::MockCallback<RemoteCharacteristic::ReadCallback> read_cb;
  EXPECT_CALL(read_cb, Run(true, kTestData2));
  characteristic->ReadAuth(kAuthReq, read_cb.Get());
  delegate->OnCharacteristicReadResponse(kTestAddr1, true,
                                         characteristic->handle(), kTestData2);

  EXPECT_CALL(*gatt_client_,
              SetCharacteristicNotification(
                  kTestAddr1, characteristic->characteristic(), true))
      .WillOnce(Return(true));

  EXPECT_CALL(cb_, Run(true));
  characteristic->SetNotification(true, cb_.Get());

  EXPECT_CALL(*observer_, OnCharacteristicNotification(
                              device, characteristics[0], kTestData3));
  delegate->OnNotification(kTestAddr1, characteristic->handle(), kTestData3);
  task_environment_.RunUntilIdle();
}

TEST_F(GattClientManagerTest,
       RemoteDeviceCharacteristicSetRegisterNotification) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};
  const auto kServices = GenerateServices();
  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  scoped_refptr<RemoteService> service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_GE(characteristics.size(), 1ul);
  ASSERT_TRUE(characteristics[0]);
  RemoteCharacteristicImpl* characteristic =
      static_cast<RemoteCharacteristicImpl*>(characteristics[0].get());

  scoped_refptr<RemoteDescriptor> cccd =
      characteristic->GetDescriptorByUuid(RemoteDescriptor::kCccdUuid);
  ASSERT_TRUE(cccd);

  EXPECT_CALL(*gatt_client_,
              SetCharacteristicNotification(
                  kTestAddr1, characteristic->characteristic(), true))
      .WillOnce(Return(true));
  std::vector<uint8_t> cccd_enable_notification = {
      std::begin(bluetooth::RemoteDescriptor::kEnableNotificationValue),
      std::end(bluetooth::RemoteDescriptor::kEnableNotificationValue)};
  EXPECT_CALL(*gatt_client_,
              WriteDescriptor(
                  kTestAddr1,
                  static_cast<RemoteDescriptorImpl*>(cccd.get())->descriptor(),
                  _, cccd_enable_notification))
      .WillOnce(Return(true));

  characteristic->SetRegisterNotification(true, cb_.Get());
  EXPECT_CALL(cb_, Run(true));
  delegate->OnDescriptorWriteResponse(kTestAddr1, true, cccd->handle());

  EXPECT_CALL(*observer_, OnCharacteristicNotification(
                              device, characteristics[0], kTestData1));
  delegate->OnNotification(kTestAddr1, characteristic->handle(), kTestData1);
  task_environment_.RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceCharacteristicSetRegisterIndication) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};
  const auto kServices = GenerateServices();
  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  scoped_refptr<RemoteService> service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_EQ(characteristics.size(), 4ul);

  // |characteristics[2]| supports indication only.
  ASSERT_TRUE(characteristics[2]);
  RemoteCharacteristicImpl* characteristic =
      static_cast<RemoteCharacteristicImpl*>(characteristics[2].get());

  scoped_refptr<RemoteDescriptor> cccd =
      characteristic->GetDescriptorByUuid(RemoteDescriptor::kCccdUuid);
  ASSERT_TRUE(cccd);

  EXPECT_CALL(*gatt_client_,
              SetCharacteristicNotification(
                  kTestAddr1, characteristic->characteristic(), true))
      .WillOnce(Return(true));
  std::vector<uint8_t> cccd_enable_indication = {
      std::begin(bluetooth::RemoteDescriptor::kEnableIndicationValue),
      std::end(bluetooth::RemoteDescriptor::kEnableIndicationValue)};
  EXPECT_CALL(*gatt_client_,
              WriteDescriptor(
                  kTestAddr1,
                  static_cast<RemoteDescriptorImpl*>(cccd.get())->descriptor(),
                  _, cccd_enable_indication))
      .WillOnce(Return(true));

  characteristic->SetRegisterNotificationOrIndication(true, cb_.Get());
  EXPECT_CALL(cb_, Run(true));
  delegate->OnDescriptorWriteResponse(kTestAddr1, true, cccd->handle());

  EXPECT_CALL(*observer_, OnCharacteristicNotification(
                              device, characteristics[2], kTestData1));
  delegate->OnNotification(kTestAddr1, characteristic->handle(), kTestData1);
  task_environment_.RunUntilIdle();

  // |characteristics[3]| supports both notification and indication.
  ASSERT_TRUE(characteristics[3]);
  characteristic =
      static_cast<RemoteCharacteristicImpl*>(characteristics[3].get());

  cccd = characteristic->GetDescriptorByUuid(RemoteDescriptor::kCccdUuid);
  ASSERT_TRUE(cccd);

  // Notification has higher priority than indication. So
  // SetRegisterNotificationOrIndication will behave the same as
  // SetRegisterNotification.
  EXPECT_CALL(*gatt_client_,
              SetCharacteristicNotification(
                  kTestAddr1, characteristic->characteristic(), true))
      .WillOnce(Return(true));
  std::vector<uint8_t> cccd_enable_notification = {
      std::begin(bluetooth::RemoteDescriptor::kEnableNotificationValue),
      std::end(bluetooth::RemoteDescriptor::kEnableNotificationValue)};
  EXPECT_CALL(*gatt_client_,
              WriteDescriptor(
                  kTestAddr1,
                  static_cast<RemoteDescriptorImpl*>(cccd.get())->descriptor(),
                  _, cccd_enable_notification))
      .WillOnce(Return(true));

  characteristic->SetRegisterNotificationOrIndication(true, cb_.Get());
  EXPECT_CALL(cb_, Run(true));
  delegate->OnDescriptorWriteResponse(kTestAddr1, true, cccd->handle());

  EXPECT_CALL(*observer_, OnCharacteristicNotification(
                              device, characteristics[3], kTestData1));
  delegate->OnNotification(kTestAddr1, characteristic->handle(), kTestData1);
  task_environment_.RunUntilIdle();
}

TEST_F(GattClientManagerTest, RemoteDeviceDescriptor) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};
  const std::vector<uint8_t> kTestData2 = {0x4, 0x5, 0x6};
  const bluetooth_v2_shlib::Gatt::Client::AuthReq kAuthReq =
      bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_MITM;
  const auto kServices = GenerateServices();
  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  auto service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_GE(characteristics.size(), 1ul);
  auto characteristic = characteristics[0];

  std::vector<scoped_refptr<RemoteDescriptor>> descriptors =
      characteristic->GetDescriptors();
  ASSERT_GE(descriptors.size(), 1ul);
  ASSERT_TRUE(descriptors[0]);
  auto* descriptor = static_cast<RemoteDescriptorImpl*>(descriptors[0].get());

  EXPECT_CALL(*gatt_client_,
              WriteDescriptor(kTestAddr1, descriptor->descriptor(), kAuthReq,
                              kTestData1))
      .WillOnce(Return(true));

  EXPECT_CALL(cb_, Run(true));
  descriptor->WriteAuth(kAuthReq, kTestData1, cb_.Get());
  delegate->OnDescriptorWriteResponse(kTestAddr1, true, descriptor->handle());

  EXPECT_CALL(*gatt_client_,
              ReadDescriptor(kTestAddr1, descriptor->descriptor(), kAuthReq))
      .WillOnce(Return(true));

  base::MockCallback<RemoteDescriptor::ReadCallback> read_cb;
  EXPECT_CALL(read_cb, Run(true, kTestData2));
  descriptor->ReadAuth(kAuthReq, read_cb.Get());
  delegate->OnDescriptorReadResponse(kTestAddr1, true, descriptor->handle(),
                                     kTestData2);
}

TEST_F(GattClientManagerTest, FakeCccd) {
  std::vector<bluetooth_v2_shlib::Gatt::Service> input_services(1);
  input_services[0].uuid = {{0x1}};
  input_services[0].handle = 0x1;
  input_services[0].primary = true;

  bluetooth_v2_shlib::Gatt::Characteristic input_characteristic;
  input_characteristic.uuid = {{0x1, 0x1}};
  input_characteristic.handle = 0x2;
  input_characteristic.permissions = bluetooth_v2_shlib::Gatt::PERMISSION_READ;
  input_characteristic.properties = bluetooth_v2_shlib::Gatt::PROPERTY_NOTIFY;
  input_services[0].characteristics.push_back(input_characteristic);

  // Test indicate as well
  input_characteristic.uuid = {{0x1, 0x2}};
  input_characteristic.handle = 0x3;
  input_characteristic.properties = bluetooth_v2_shlib::Gatt::PROPERTY_INDICATE;
  input_services[0].characteristics.push_back(input_characteristic);

  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, input_services);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(input_services.size(), services.size());

  auto service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_EQ(2u, characteristics.size());
  for (const auto& characteristic : characteristics) {
    // A CCCD should have been created.
    std::vector<scoped_refptr<RemoteDescriptor>> descriptors =
        characteristic->GetDescriptors();
    ASSERT_EQ(descriptors.size(), 1ul);
    auto descriptor = descriptors[0];
    EXPECT_EQ(RemoteDescriptor::kCccdUuid, descriptor->uuid());
    EXPECT_EQ(static_cast<bluetooth_v2_shlib::Gatt::Permissions>(
                  bluetooth_v2_shlib::Gatt::PERMISSION_READ |
                  bluetooth_v2_shlib::Gatt::PERMISSION_WRITE),
              descriptor->permissions());
  }
}

TEST_F(GattClientManagerTest, WriteType) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};

  bluetooth_v2_shlib::Gatt::Service service;

  service.uuid = {{0x1}};
  service.handle = 0x1;
  service.primary = true;

  {
    bluetooth_v2_shlib::Gatt::Characteristic characteristic;
    characteristic.uuid = {{0x1, 0x1}};
    characteristic.handle = 0x2;
    characteristic.permissions = bluetooth_v2_shlib::Gatt::PERMISSION_WRITE;
    characteristic.properties = bluetooth_v2_shlib::Gatt::PROPERTY_WRITE;
    service.characteristics.push_back(characteristic);

    characteristic.uuid = {{0x1, 0x2}};
    characteristic.handle = 0x3;
    characteristic.permissions = bluetooth_v2_shlib::Gatt::PERMISSION_WRITE;
    characteristic.properties =
        bluetooth_v2_shlib::Gatt::PROPERTY_WRITE_NO_RESPONSE;
    service.characteristics.push_back(characteristic);

    characteristic.uuid = {{0x1, 0x3}};
    characteristic.handle = 0x4;
    characteristic.permissions = bluetooth_v2_shlib::Gatt::PERMISSION_WRITE;
    characteristic.properties = bluetooth_v2_shlib::Gatt::PROPERTY_SIGNED_WRITE;
    service.characteristics.push_back(characteristic);
  }

  Connect(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, {service});

  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);

  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(1u, services.size());

  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      services[0]->GetCharacteristics();
  ASSERT_EQ(3u, characteristics.size());

  using WriteType = bluetooth_v2_shlib::Gatt::WriteType;

  // The current implementation of RemoteDevice will put the characteristics in
  // the order reported by libcast_bluetooth.
  const WriteType kWriteTypes[] = {WriteType::WRITE_TYPE_DEFAULT,
                                   WriteType::WRITE_TYPE_NO_RESPONSE,
                                   WriteType::WRITE_TYPE_SIGNED};

  for (size_t i = 0; i < characteristics.size(); ++i) {
    ASSERT_TRUE(characteristics[i]);
    auto* characteristic =
        static_cast<RemoteCharacteristicImpl*>(characteristics[i].get());
    EXPECT_CALL(
        *gatt_client_,
        WriteCharacteristic(kTestAddr1, characteristic->characteristic(),
                            bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_NONE,
                            kWriteTypes[i], kTestData1))
        .WillOnce(Return(true));

    base::MockCallback<RemoteCharacteristic::StatusCallback> write_cb;
    EXPECT_CALL(write_cb, Run(true));
    characteristic->Write(kTestData1, write_cb.Get());
    delegate->OnCharacteristicWriteResponse(kTestAddr1, true,
                                            characteristic->handle());
  }
}

TEST_F(GattClientManagerTest, ConnectMultiple) {
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  for (size_t i = 0; i < 5; ++i) {
    Connect(kTestAddr1);
    EXPECT_TRUE(device->IsConnected());
    EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
    device->Disconnect({});
    delegate->OnConnectChanged(kTestAddr1, true /* status */,
                               false /* connected */);
    EXPECT_FALSE(device->IsConnected());
  }
}

TEST_F(GattClientManagerTest, GetServicesFailOnConnect) {
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr1, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  device->Connect(connect_cb_.Get());
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();

  EXPECT_CALL(connect_cb_, Run(RemoteDevice::ConnectStatus::kFailure));
  EXPECT_CALL(*gatt_client_, GetServices(kTestAddr1)).WillOnce(Return(false));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             true /* connected */);
  EXPECT_FALSE(device->IsConnected());
}

TEST_F(GattClientManagerTest, GetServicesSuccessAfterConnectCallback) {
  const auto kServices = GenerateServices();
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);

  // Callback that checks when Connect()'s callback returns, GetServices returns
  // the correct services.
  bool cb_called = false;
  auto cb = base::BindOnce(
      [](GattClientManagerTest* gcmt,
         const std::vector<bluetooth_v2_shlib::Gatt::Service>*
             expected_services,
         bool* cb_called, RemoteDevice::ConnectStatus status) {
        EXPECT_EQ(RemoteDevice::ConnectStatus::kSuccess, status);
        *cb_called = true;

        auto device = gcmt->GetDevice(kTestAddr1);
        auto services = gcmt->GetServices(device.get());
        EXPECT_EQ(expected_services->size(), services.size());
      },
      this, &kServices, &cb_called);
  EXPECT_CALL(*gatt_client_,
              Connect(kTestAddr1, bluetooth_v2_shlib::Gatt::Client::Transport::kAuto))
    .WillOnce(Return(true));
  device->Connect(std::move(cb));

  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  EXPECT_CALL(*gatt_client_, GetServices(kTestAddr1)).WillOnce(Return(true));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             true /* connected */);

  // Connect's callback should not be called until service discovery is
  // complete.
  EXPECT_FALSE(cb_called);
  delegate->OnGetServices(kTestAddr1, kServices);
  EXPECT_TRUE(cb_called);
}

TEST_F(GattClientManagerTest, Queuing) {
  const std::vector<uint8_t> kTestData1 = {0x1, 0x2, 0x3};
  const std::vector<uint8_t> kTestData2 = {0x4, 0x5, 0x6};
  const std::vector<uint8_t> kTestData3 = {0x7, 0x8, 0x9};
  const auto kServices = GenerateServices();
  const bluetooth_v2_shlib::Gatt::Client::AuthReq kAuthReq =
      bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_MITM;
  const bluetooth_v2_shlib::Gatt::WriteType kWriteType =
      bluetooth_v2_shlib::Gatt::WRITE_TYPE_DEFAULT;

  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  auto service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_GE(characteristics.size(), 2ul);
  ASSERT_TRUE(characteristics[0]);
  ASSERT_TRUE(characteristics[1]);
  auto* characteristic1 =
      static_cast<RemoteCharacteristicImpl*>(characteristics[0].get());
  auto* characteristic2 =
      static_cast<RemoteCharacteristicImpl*>(characteristics[1].get());

  // Issue a write to one characteristic.
  EXPECT_CALL(*gatt_client_,
              WriteCharacteristic(kTestAddr1, characteristic1->characteristic(),
                                  kAuthReq, kWriteType, kTestData1))
      .WillOnce(Return(true));
  characteristic1->WriteAuth(kAuthReq, kWriteType, kTestData1, cb_.Get());

  // Issue a read to another characteristic. The shlib should not get the call
  // until after the read's callback.
  EXPECT_CALL(*gatt_client_,
              ReadCharacteristic(kTestAddr1, characteristic2->characteristic(),
                                 kAuthReq))
      .Times(0);
  base::MockCallback<RemoteCharacteristic::ReadCallback> read_cb;
  characteristic2->ReadAuth(kAuthReq, read_cb.Get());

  EXPECT_CALL(cb_, Run(true));
  EXPECT_CALL(*gatt_client_,
              ReadCharacteristic(kTestAddr1, characteristic2->characteristic(),
                                 kAuthReq))
      .WillOnce(Return(true));
  delegate->OnCharacteristicWriteResponse(kTestAddr1, true,
                                          characteristic1->handle());

  EXPECT_CALL(read_cb, Run(true, kTestData2));
  delegate->OnCharacteristicReadResponse(kTestAddr1, true,
                                         characteristic2->handle(), kTestData2);
  base::RunLoop().RunUntilIdle();
}

TEST_F(GattClientManagerTest, CommandTimeout) {
  const std::vector<uint8_t> kTestData = {0x7, 0x8, 0x9};
  const auto kServices = GenerateServices();
  const auto kAuthReq = bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_MITM;
  const auto kWriteType = bluetooth_v2_shlib::Gatt::WRITE_TYPE_DEFAULT;

  // Connect a device and get services.
  Connect(kTestAddr1);
  scoped_refptr<RemoteDevice> device = GetDevice(kTestAddr1);
  bluetooth_v2_shlib::Gatt::Client::Delegate* delegate =
      gatt_client_->delegate();
  delegate->OnServicesAdded(kTestAddr1, kServices);
  std::vector<scoped_refptr<RemoteService>> services =
      GetServices(device.get());
  ASSERT_EQ(kServices.size(), services.size());

  auto service = services[0];
  std::vector<scoped_refptr<RemoteCharacteristic>> characteristics =
      service->GetCharacteristics();
  ASSERT_GE(characteristics.size(), 1ul);
  ASSERT_TRUE(characteristics[0]);
  auto* characteristic =
      static_cast<RemoteCharacteristicImpl*>(characteristics[0].get());

  // Issue a write to one characteristic.
  EXPECT_CALL(*gatt_client_,
              WriteCharacteristic(kTestAddr1, characteristic->characteristic(),
                                  kAuthReq, kWriteType, kTestData))
      .WillOnce(Return(true));
  characteristic->WriteAuth(kAuthReq, kWriteType, kTestData, cb_.Get());

  // Let the command timeout
  // We should request a disconnect.
  EXPECT_CALL(*gatt_client_, Disconnect(kTestAddr1)).WillOnce(Return(true));
  task_environment_.FastForwardBy(RemoteDeviceImpl::kCommandTimeout);

  // Make sure we issued a disconnect.
  testing::Mock::VerifyAndClearExpectations(gatt_client_.get());

  // The operation should fail.
  EXPECT_CALL(cb_, Run(false));
  delegate->OnConnectChanged(kTestAddr1, true /* status */,
                             false /* connected */);
}

}  // namespace bluetooth
}  // namespace chromecast
