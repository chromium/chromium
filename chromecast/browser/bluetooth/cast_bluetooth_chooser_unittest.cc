// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/bluetooth/cast_bluetooth_chooser.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

class SimpleDeviceAccessProvider : public mojom::BluetoothDeviceAccessProvider {
 public:
  SimpleDeviceAccessProvider() = default;
  ~SimpleDeviceAccessProvider() override = default;

  // mojom::BluetoothDeviceAccessProvider implementation:
  void RequestDeviceAccess(
      mojo::PendingRemote<mojom::BluetoothDeviceAccessProviderClient> client)
      override {
    DCHECK(!client_);
    client_.Bind(std::move(client));
    client_.set_disconnect_handler(connection_closed_.Get());
    for (const auto& address : approved_devices_)
      client_->GrantAccess(address);
  }

  mojom::BluetoothDeviceAccessProviderClient* client() {
    return client_ ? client_.get() : nullptr;
  }
  void reset_client() { return client_.reset(); }
  base::MockCallback<base::OnceClosure>& connection_closed() {
    return connection_closed_;
  }
  std::vector<std::string>& approved_devices() { return approved_devices_; }

 private:
  mojo::Remote<mojom::BluetoothDeviceAccessProviderClient> client_;
  base::MockCallback<base::OnceClosure> connection_closed_;
  std::vector<std::string> approved_devices_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDeviceAccessProvider);
};

}  // namespace

using testing::AnyOf;

class CastBluetoothChooserTest : public testing::Test {
 public:
  CastBluetoothChooserTest() : provider_receiver_(&provider_) {
    cast_bluetooth_chooser_ = std::make_unique<CastBluetoothChooser>(
        handler_.Get(), provider_receiver_.BindNewPipeAndPassRemote());
    task_environment_.RunUntilIdle();
  }

  ~CastBluetoothChooserTest() override = default;

  void AddDeviceToChooser(const std::string& address) {
    chooser().AddOrUpdateDevice(address, false, base::string16(), false, false,
                                0);
  }

  SimpleDeviceAccessProvider& provider() { return provider_; }
  content::BluetoothChooser& chooser() { return *cast_bluetooth_chooser_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::MockCallback<content::BluetoothChooser::EventHandler> handler_;

 private:
  SimpleDeviceAccessProvider provider_;
  mojo::Receiver<mojom::BluetoothDeviceAccessProvider> provider_receiver_;
  std::unique_ptr<CastBluetoothChooser> cast_bluetooth_chooser_;

  DISALLOW_COPY_AND_ASSIGN(CastBluetoothChooserTest);
};

TEST_F(CastBluetoothChooserTest, GrantAccessBeforeDeviceAvailable) {
  // No devices have been made available to |chooser| yet. Grant it access to a
  // device. |handler| should not run yet.
  EXPECT_TRUE(provider().client());
  provider().client()->GrantAccess("aa:bb:cc:dd:ee:ff");
  task_environment_.RunUntilIdle();

  // Make some unapproved devices available. |handler| should not run yet.
  AddDeviceToChooser("11:22:33:44:55:66");
  AddDeviceToChooser("99:88:77:66:55:44");

  // Now make the approved device available. |handler| should be called.
  EXPECT_CALL(handler_, Run(content::BluetoothChooser::Event::SELECTED,
                            "aa:bb:cc:dd:ee:ff"));
  EXPECT_CALL(provider().connection_closed(), Run());
  AddDeviceToChooser("aa:bb:cc:dd:ee:ff");
  task_environment_.RunUntilIdle();
}

TEST_F(CastBluetoothChooserTest, DiscoverDeviceBeforeAccessGranted) {
  // Make some devices available before access is granted. |handler| should not
  // run yet.
  AddDeviceToChooser("11:22:33:44:55:66");
  AddDeviceToChooser("aa:bb:cc:dd:ee:ff");
  AddDeviceToChooser("00:00:00:11:00:00");
  AddDeviceToChooser("99:88:77:66:55:44");

  // Now approve one of those devices. |handler| should run.
  EXPECT_CALL(handler_, Run(content::BluetoothChooser::Event::SELECTED,
                            "00:00:00:11:00:00"));
  EXPECT_CALL(provider().connection_closed(), Run());
  provider().client()->GrantAccess("00:00:00:11:00:00");
  task_environment_.RunUntilIdle();
}

TEST_F(CastBluetoothChooserTest, GrantAccessToAllDevicesBeforeDiscovery) {
  // Grant access to all devices. |handler| should not run until the first
  // device is made available.
  provider().client()->GrantAccessToAllDevices();
  task_environment_.RunUntilIdle();

  // Now make the some device available. |handler| should be called.
  EXPECT_CALL(handler_, Run(content::BluetoothChooser::Event::SELECTED,
                            "aa:bb:cc:dd:ee:ff"));
  EXPECT_CALL(provider().connection_closed(), Run());
  AddDeviceToChooser("aa:bb:cc:dd:ee:ff");
  task_environment_.RunUntilIdle();
}

TEST_F(CastBluetoothChooserTest, GrantAccessToAllDevicesAfterDiscovery) {
  // Make some devices available before access is granted. |handler| should not
  // run yet.
  AddDeviceToChooser("11:22:33:44:55:66");
  AddDeviceToChooser("aa:bb:cc:dd:ee:ff");
  AddDeviceToChooser("00:00:00:11:00:00");

  // Now grant access to all devices. |handler| should be called with one of the
  // available devices.
  EXPECT_CALL(handler_, Run(content::BluetoothChooser::Event::SELECTED,
                            AnyOf("11:22:33:44:55:66", "aa:bb:cc:dd:ee:ff",
                                  "00:00:00:11:00:00")));
  EXPECT_CALL(provider().connection_closed(), Run());
  provider().client()->GrantAccessToAllDevices();
  task_environment_.RunUntilIdle();
}

TEST_F(CastBluetoothChooserTest, TearDownClientAfterAllAccessGranted) {
  // Grant access to all devices. |handler| should not run until the first
  // device is made available.
  provider().client()->GrantAccessToAllDevices();
  task_environment_.RunUntilIdle();

  // Tear down the client. Now that it has granted access to the client, it does
  // not need to keep a reference to it. However, the chooser should stay alive
  // and wait for devices to be made available.
  provider().reset_client();
  EXPECT_FALSE(provider().client());
  task_environment_.RunUntilIdle();

  // As soon as a device is available, run the handler.
  EXPECT_CALL(handler_, Run(content::BluetoothChooser::Event::SELECTED,
                            "aa:bb:cc:dd:ee:ff"));
  AddDeviceToChooser("aa:bb:cc:dd:ee:ff");
}

TEST_F(CastBluetoothChooserTest, TearDownClientBeforeApprovedDeviceDiscovered) {
  // Make some devices available before access is granted. |handler| should not
  // run yet.
  AddDeviceToChooser("11:22:33:44:55:66");
  AddDeviceToChooser("aa:bb:cc:dd:ee:ff");

  // Tear the client down before any access is granted. |handler| should run,
  // but with Event::CANCELLED.
  EXPECT_CALL(handler_, Run(content::BluetoothChooser::Event::CANCELLED, ""));
  provider().reset_client();
  EXPECT_FALSE(provider().client());
  task_environment_.RunUntilIdle();
}

}  // namespace chromecast
