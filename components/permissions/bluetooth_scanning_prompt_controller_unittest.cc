// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/permissions/bluetooth_scanning_prompt_controller.h"
#include "components/permissions/mock_chooser_controller_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::NiceMock;

namespace permissions {

class BluetoothScanningPromptControllerTest : public testing::Test {
 public:
  BluetoothScanningPromptControllerTest()
      : bluetooth_scanning_prompt_controller_(
            nullptr,
            base::BindRepeating(&BluetoothScanningPromptControllerTest::
                                    OnBluetoothScanningPromptEvent,
                                base::Unretained(this)),
            u"title") {
    bluetooth_scanning_prompt_controller_.set_view(
        &mock_bluetooth_scanning_prompt_view_);
  }

  BluetoothScanningPromptControllerTest(
      const BluetoothScanningPromptControllerTest&) = delete;
  BluetoothScanningPromptControllerTest& operator=(
      const BluetoothScanningPromptControllerTest&) = delete;

 protected:
  void OnBluetoothScanningPromptEvent(
      content::BluetoothScanningPrompt::Event event) {
    last_event_ = event;
  }

  BluetoothScanningPromptController bluetooth_scanning_prompt_controller_;
  NiceMock<MockChooserControllerView> mock_bluetooth_scanning_prompt_view_;
  content::BluetoothScanningPrompt::Event last_event_;
};

class BluetoothScanningPromptControllerWithDevicesAddedTest
    : public BluetoothScanningPromptControllerTest {
 public:
  BluetoothScanningPromptControllerWithDevicesAddedTest() {
    bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
        "id_a", /*should_update_name=*/false, u"a");
    bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
        "id_b", /*should_update_name=*/false, u"b");
    bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
        "id_c", /*should_update_name=*/false, u"c");
  }
};

TEST_F(BluetoothScanningPromptControllerTest, AddDevice) {
  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionAdded(0)).Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a", /*should_update_name=*/false, u"a");
  EXPECT_EQ(1u, bluetooth_scanning_prompt_controller_.NumOptions());
  EXPECT_EQ(u"a", bluetooth_scanning_prompt_controller_.GetOption(0));
  testing::Mock::VerifyAndClearExpectations(
      &mock_bluetooth_scanning_prompt_view_);

  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionAdded(1)).Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_b", /*should_update_name=*/false, u"b");
  EXPECT_EQ(2u, bluetooth_scanning_prompt_controller_.NumOptions());
  EXPECT_EQ(u"b", bluetooth_scanning_prompt_controller_.GetOption(1));
  testing::Mock::VerifyAndClearExpectations(
      &mock_bluetooth_scanning_prompt_view_);

  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionAdded(2)).Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_c", /*should_update_name=*/false, u"c");
  EXPECT_EQ(3u, bluetooth_scanning_prompt_controller_.NumOptions());
  EXPECT_EQ(u"c", bluetooth_scanning_prompt_controller_.GetOption(2));
}

TEST_F(BluetoothScanningPromptControllerTest,
       MultipleDevicesWithSameNameShowIds) {
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a_1", /*should_update_name=*/false, u"a");
  EXPECT_EQ(u"a", bluetooth_scanning_prompt_controller_.GetOption(0));

  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_b", /*should_update_name=*/false, u"b");
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a_2", /*should_update_name=*/false, u"a");
  EXPECT_EQ(u"a (id_a_1)", bluetooth_scanning_prompt_controller_.GetOption(0));
  EXPECT_EQ(u"b", bluetooth_scanning_prompt_controller_.GetOption(1));
  EXPECT_EQ(u"a (id_a_2)", bluetooth_scanning_prompt_controller_.GetOption(2));
}

TEST_F(BluetoothScanningPromptControllerTest, UpdateDeviceName) {
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a", /*should_update_name=*/false, u"a");
  EXPECT_EQ(u"a", bluetooth_scanning_prompt_controller_.GetOption(0));

  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionUpdated(0))
      .Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a", /*should_update_name=*/false, u"aa");
  // The name is still "a" since |should_update_name| is false.
  EXPECT_EQ(u"a", bluetooth_scanning_prompt_controller_.GetOption(0));
  testing::Mock::VerifyAndClearExpectations(
      &mock_bluetooth_scanning_prompt_view_);

  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionUpdated(0))
      .Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a", true /* should_update_name */, u"aa");
  EXPECT_EQ(1u, bluetooth_scanning_prompt_controller_.NumOptions());
  EXPECT_EQ(u"aa", bluetooth_scanning_prompt_controller_.GetOption(0));
}

TEST_F(BluetoothScanningPromptControllerWithDevicesAddedTest,
       InitialNoOptionsText) {
  EXPECT_EQ(u"No nearby devices found.",
            bluetooth_scanning_prompt_controller_.GetNoOptionsText());
}

TEST_F(BluetoothScanningPromptControllerWithDevicesAddedTest,
       AllowShouldCallEventHandler) {
  std::vector<size_t> indices;
  bluetooth_scanning_prompt_controller_.Select(indices);
  EXPECT_EQ(content::BluetoothScanningPrompt::Event::kAllow, last_event_);
}

TEST_F(BluetoothScanningPromptControllerWithDevicesAddedTest,
       BlockShouldCallEventHandler) {
  bluetooth_scanning_prompt_controller_.Cancel();
  EXPECT_EQ(content::BluetoothScanningPrompt::Event::kBlock, last_event_);
}

TEST_F(BluetoothScanningPromptControllerWithDevicesAddedTest,
       CloseShouldCallEventHandler) {
  bluetooth_scanning_prompt_controller_.Close();
  EXPECT_EQ(content::BluetoothScanningPrompt::Event::kCanceled, last_event_);
}

}  // namespace permissions
