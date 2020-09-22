// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chooser_controller/mock_chooser_controller_view.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class BluetoothChooserControllerTest : public testing::Test {
 public:
  BluetoothChooserControllerTest()
      : bluetooth_chooser_controller_(
            nullptr,
            base::Bind(&BluetoothChooserControllerTest::OnBluetoothChooserEvent,
                       base::Unretained(this))) {
    bluetooth_chooser_controller_.set_view(&mock_bluetooth_chooser_view_);
  }

 protected:
  void OnBluetoothChooserEvent(content::BluetoothChooserEvent event,
                               const std::string& device_id) {
    last_event_ = event;
    last_device_id_ = device_id;
  }

  BluetoothChooserController bluetooth_chooser_controller_;
  MockChooserControllerView mock_bluetooth_chooser_view_;
  content::BluetoothChooserEvent last_event_;
  std::string last_device_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothChooserControllerTest);
};

class BluetoothChooserControllerWithDevicesAddedTest
    : public BluetoothChooserControllerTest {
 public:
  BluetoothChooserControllerWithDevicesAddedTest() {
    bluetooth_chooser_controller_.AddOrUpdateDevice(
        "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
        true /* is_gatt_connected */, true /* is_paired */,
        -1 /* signal_strength_level */);
    bluetooth_chooser_controller_.AddOrUpdateDevice(
        "id_b", false /* should_update_name */, base::ASCIIToUTF16("b"),
        true /* is_gatt_connected */, true /* is_paired */,
        0 /* signal_strength_level */);
    bluetooth_chooser_controller_.AddOrUpdateDevice(
        "id_c", false /* should_update_name */, base::ASCIIToUTF16("c"),
        true /* is_gatt_connected */, true /* is_paired */,
        1 /* signal_strength_level */);
  }
};

TEST_F(BluetoothChooserControllerTest, AddDevice) {
  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionAdded(0)).Times(1);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_EQ(1u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_chooser_controller_.GetOption(0));
  EXPECT_EQ(-1, bluetooth_chooser_controller_.GetSignalStrengthLevel(0));
  EXPECT_TRUE(bluetooth_chooser_controller_.IsConnected(0));
  EXPECT_TRUE(bluetooth_chooser_controller_.IsPaired(0));
  testing::Mock::VerifyAndClearExpectations(&mock_bluetooth_chooser_view_);

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionAdded(1)).Times(1);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_b", false /* should_update_name */, base::ASCIIToUTF16("b"),
      true /* is_gatt_connected */, true /* is_paired */,
      0 /* signal_strength_level */);
  EXPECT_EQ(2u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("b"),
            bluetooth_chooser_controller_.GetOption(1));
  EXPECT_EQ(0, bluetooth_chooser_controller_.GetSignalStrengthLevel(1));
  testing::Mock::VerifyAndClearExpectations(&mock_bluetooth_chooser_view_);

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionAdded(2)).Times(1);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_c", false /* should_update_name */, base::ASCIIToUTF16("c"),
      true /* is_gatt_connected */, true /* is_paired */,
      1 /* signal_strength_level */);
  EXPECT_EQ(3u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("c"),
            bluetooth_chooser_controller_.GetOption(2));
  EXPECT_EQ(1, bluetooth_chooser_controller_.GetSignalStrengthLevel(2));
}

TEST_F(BluetoothChooserControllerTest, RemoveDevice) {
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_b", false /* should_update_name */, base::ASCIIToUTF16("b"),
      true /* is_gatt_connected */, true /* is_paired */,
      0 /* signal_strength_level */);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_c", false /* should_update_name */, base::ASCIIToUTF16("c"),
      true /* is_gatt_connected */, true /* is_paired */,
      1 /* signal_strength_level */);

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionRemoved(1)).Times(1);
  bluetooth_chooser_controller_.RemoveDevice("id_b");
  EXPECT_EQ(2u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_chooser_controller_.GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("c"),
            bluetooth_chooser_controller_.GetOption(1));
  testing::Mock::VerifyAndClearExpectations(&mock_bluetooth_chooser_view_);

  // Remove a non-existent device, the number of devices should not change.
  bluetooth_chooser_controller_.RemoveDevice("non-existent");
  EXPECT_EQ(2u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_chooser_controller_.GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("c"),
            bluetooth_chooser_controller_.GetOption(1));

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionRemoved(0)).Times(1);
  bluetooth_chooser_controller_.RemoveDevice("id_a");
  EXPECT_EQ(1u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("c"),
            bluetooth_chooser_controller_.GetOption(0));
  testing::Mock::VerifyAndClearExpectations(&mock_bluetooth_chooser_view_);

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionRemoved(0)).Times(1);
  bluetooth_chooser_controller_.RemoveDevice("id_c");
  EXPECT_EQ(0u, bluetooth_chooser_controller_.NumOptions());
}

TEST_F(BluetoothChooserControllerTest, MultipleDevicesWithSameNameShowIds) {
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a_1", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_chooser_controller_.GetOption(0));

  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_b", false /* should_update_name */, base::ASCIIToUTF16("b"),
      true /* is_gatt_connected */, true /* is_paired */,
      0 /* signal_strength_level */);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a_2", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      1 /* signal_strength_level */);
  EXPECT_EQ(base::ASCIIToUTF16("a (id_a_1)"),
            bluetooth_chooser_controller_.GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("b"),
            bluetooth_chooser_controller_.GetOption(1));
  EXPECT_EQ(base::ASCIIToUTF16("a (id_a_2)"),
            bluetooth_chooser_controller_.GetOption(2));

  bluetooth_chooser_controller_.RemoveDevice("id_a_1");
  EXPECT_EQ(base::ASCIIToUTF16("b"),
            bluetooth_chooser_controller_.GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_chooser_controller_.GetOption(1));
}

TEST_F(BluetoothChooserControllerTest, UpdateDeviceName) {
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_chooser_controller_.GetOption(0));

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionUpdated(0)).Times(1);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("aa"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  // The name is still "a" since |should_update_name| is false.
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_chooser_controller_.GetOption(0));
  testing::Mock::VerifyAndClearExpectations(&mock_bluetooth_chooser_view_);

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionUpdated(0)).Times(1);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", true /* should_update_name */, base::ASCIIToUTF16("aa"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_EQ(1u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("aa"),
            bluetooth_chooser_controller_.GetOption(0));

  bluetooth_chooser_controller_.RemoveDevice("id_a");
  EXPECT_EQ(0u, bluetooth_chooser_controller_.NumOptions());
}

TEST_F(BluetoothChooserControllerTest, UpdateDeviceSignalStrengthLevel) {
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_EQ(-1, bluetooth_chooser_controller_.GetSignalStrengthLevel(0));

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionUpdated(0)).Times(1);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      1 /* signal_strength_level */);
  EXPECT_EQ(1, bluetooth_chooser_controller_.GetSignalStrengthLevel(0));
  testing::Mock::VerifyAndClearExpectations(&mock_bluetooth_chooser_view_);

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionUpdated(0)).Times(1);
  // When Bluetooth device scanning stops, an update is sent and the signal
  // strength level is -1, and in this case, should still use the previously
  // stored signal strength level. So here the signal strength level is
  // still 1.
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_EQ(1, bluetooth_chooser_controller_.GetSignalStrengthLevel(0));
}

TEST_F(BluetoothChooserControllerTest, UpdateConnectedStatus) {
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      false /* is_gatt_connected */, false /* is_paired */,
      1 /* signal_strength_level */);
  EXPECT_FALSE(bluetooth_chooser_controller_.IsConnected(0));

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionUpdated(0)).Times(1);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, false /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_TRUE(bluetooth_chooser_controller_.IsConnected(0));
}

TEST_F(BluetoothChooserControllerTest, UpdatePairedStatus) {
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, false /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_FALSE(bluetooth_chooser_controller_.IsPaired(0));

  EXPECT_CALL(mock_bluetooth_chooser_view_, OnOptionUpdated(0)).Times(1);
  bluetooth_chooser_controller_.AddOrUpdateDevice(
      "id_a", false /* should_update_name */, base::ASCIIToUTF16("a"),
      true /* is_gatt_connected */, true /* is_paired */,
      -1 /* signal_strength_level */);
  EXPECT_TRUE(bluetooth_chooser_controller_.IsPaired(0));
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest,
       InitialNoOptionsTextAndStatusText) {
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT),
            bluetooth_chooser_controller_.GetNoOptionsText());
  EXPECT_EQ(base::string16(), bluetooth_chooser_controller_.GetStatus());
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest,
       BluetoothAdapterTurnedOff) {
  EXPECT_CALL(
      mock_bluetooth_chooser_view_,
      OnAdapterEnabledChanged(false /* Bluetooth adapter is turned off */))
      .Times(1);
  bluetooth_chooser_controller_.OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_OFF);
  EXPECT_EQ(0u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(base::string16(), bluetooth_chooser_controller_.GetStatus());
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest,
       BluetoothAdapterTurnedOn) {
  EXPECT_CALL(
      mock_bluetooth_chooser_view_,
      OnAdapterEnabledChanged(true /* Bluetooth adapter is turned on */))
      .Times(1);
  bluetooth_chooser_controller_.OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence::POWERED_ON);
  EXPECT_EQ(0u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT),
            bluetooth_chooser_controller_.GetNoOptionsText());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN),
            bluetooth_chooser_controller_.GetStatus());
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest, DiscoveringState) {
  EXPECT_CALL(
      mock_bluetooth_chooser_view_,
      OnRefreshStateChanged(true /* Refreshing options is in progress */))
      .Times(1);
  bluetooth_chooser_controller_.OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::DISCOVERING);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING),
            bluetooth_chooser_controller_.GetStatus());
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest, IdleState) {
  EXPECT_CALL(mock_bluetooth_chooser_view_,
              OnRefreshStateChanged(false /* Refreshing options is complete */))
      .Times(1);
  bluetooth_chooser_controller_.OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::IDLE);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN),
            bluetooth_chooser_controller_.GetStatus());
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest, FailedToStartState) {
  EXPECT_CALL(mock_bluetooth_chooser_view_,
              OnRefreshStateChanged(false /* Refreshing options is complete */))
      .Times(1);
  bluetooth_chooser_controller_.OnDiscoveryStateChanged(
      content::BluetoothChooser::DiscoveryState::FAILED_TO_START);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN),
            bluetooth_chooser_controller_.GetStatus());
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest, RefreshOptions) {
  bluetooth_chooser_controller_.RefreshOptions();
  EXPECT_EQ(0u, bluetooth_chooser_controller_.NumOptions());
  EXPECT_EQ(content::BluetoothChooserEvent::RESCAN, last_event_);
  EXPECT_EQ(std::string(), last_device_id_);
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest,
       SelectingOneDeviceShouldCallEventHandler) {
  std::vector<size_t> indices{0};
  bluetooth_chooser_controller_.Select(indices);
  EXPECT_EQ(content::BluetoothChooserEvent::SELECTED, last_event_);
  EXPECT_EQ("id_a", last_device_id_);
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest,
       CancelShouldCallEventHandler) {
  bluetooth_chooser_controller_.Cancel();
  EXPECT_EQ(content::BluetoothChooserEvent::CANCELLED, last_event_);
  EXPECT_EQ(std::string(), last_device_id_);
}

TEST_F(BluetoothChooserControllerWithDevicesAddedTest,
       CloseShouldCallEventHandler) {
  bluetooth_chooser_controller_.Close();
  EXPECT_EQ(content::BluetoothChooserEvent::CANCELLED, last_event_);
  EXPECT_EQ(std::string(), last_device_id_);
}
