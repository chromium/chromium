// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_controller.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class MockBluetoothScanningPromptView : public ChooserController::View {
 public:
  MockBluetoothScanningPromptView() {}

  // ChooserController::View:
  MOCK_METHOD0(OnOptionsInitialized, void());
  MOCK_METHOD1(OnOptionAdded, void(size_t index));
  MOCK_METHOD1(OnOptionRemoved, void(size_t index));
  MOCK_METHOD1(OnOptionUpdated, void(size_t index));
  MOCK_METHOD1(OnAdapterEnabledChanged, void(bool enabled));
  MOCK_METHOD1(OnRefreshStateChanged, void(bool enabled));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothScanningPromptView);
};

}  // namespace

class BluetoothScanningPromptControllerTest : public testing::Test {
 public:
  BluetoothScanningPromptControllerTest()
      : bluetooth_scanning_prompt_controller_(
            nullptr,
            base::BindRepeating(&BluetoothScanningPromptControllerTest::
                                    OnBluetoothScanningPromptEvent,
                                base::Unretained(this))) {
    bluetooth_scanning_prompt_controller_.set_view(
        &mock_bluetooth_scanning_prompt_view_);
  }

 protected:
  void OnBluetoothScanningPromptEvent(
      content::BluetoothScanningPrompt::Event event) {
    last_event_ = event;
  }

  BluetoothScanningPromptController bluetooth_scanning_prompt_controller_;
  MockBluetoothScanningPromptView mock_bluetooth_scanning_prompt_view_;
  content::BluetoothScanningPrompt::Event last_event_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothScanningPromptControllerTest);
};

class BluetoothScanningPromptControllerWithDevicesAddedTest
    : public BluetoothScanningPromptControllerTest {
 public:
  BluetoothScanningPromptControllerWithDevicesAddedTest() {
    bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
        "id_a", /*should_update_name=*/false, base::ASCIIToUTF16("a"));
    bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
        "id_b", /*should_update_name=*/false, base::ASCIIToUTF16("b"));
    bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
        "id_c", /*should_update_name=*/false, base::ASCIIToUTF16("c"));
  }
};

TEST_F(BluetoothScanningPromptControllerTest, AddDevice) {
  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionAdded(0)).Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a", /*should_update_name=*/false, base::ASCIIToUTF16("a"));
  EXPECT_EQ(1u, bluetooth_scanning_prompt_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_scanning_prompt_controller_.GetOption(0));
  testing::Mock::VerifyAndClearExpectations(
      &mock_bluetooth_scanning_prompt_view_);

  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionAdded(1)).Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_b", /*should_update_name=*/false, base::ASCIIToUTF16("b"));
  EXPECT_EQ(2u, bluetooth_scanning_prompt_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("b"),
            bluetooth_scanning_prompt_controller_.GetOption(1));
  testing::Mock::VerifyAndClearExpectations(
      &mock_bluetooth_scanning_prompt_view_);

  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionAdded(2)).Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_c", /*should_update_name=*/false, base::ASCIIToUTF16("c"));
  EXPECT_EQ(3u, bluetooth_scanning_prompt_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("c"),
            bluetooth_scanning_prompt_controller_.GetOption(2));
}

TEST_F(BluetoothScanningPromptControllerTest,
       MultipleDevicesWithSameNameShowIds) {
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a_1", /*should_update_name=*/false, base::ASCIIToUTF16("a"));
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_scanning_prompt_controller_.GetOption(0));

  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_b", /*should_update_name=*/false, base::ASCIIToUTF16("b"));
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a_2", /*should_update_name=*/false, base::ASCIIToUTF16("a"));
  EXPECT_EQ(base::ASCIIToUTF16("a (id_a_1)"),
            bluetooth_scanning_prompt_controller_.GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("b"),
            bluetooth_scanning_prompt_controller_.GetOption(1));
  EXPECT_EQ(base::ASCIIToUTF16("a (id_a_2)"),
            bluetooth_scanning_prompt_controller_.GetOption(2));
}

TEST_F(BluetoothScanningPromptControllerTest, UpdateDeviceName) {
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a", /*should_update_name=*/false, base::ASCIIToUTF16("a"));
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_scanning_prompt_controller_.GetOption(0));

  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionUpdated(0))
      .Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a", /*should_update_name=*/false, base::ASCIIToUTF16("aa"));
  // The name is still "a" since |should_update_name| is false.
  EXPECT_EQ(base::ASCIIToUTF16("a"),
            bluetooth_scanning_prompt_controller_.GetOption(0));
  testing::Mock::VerifyAndClearExpectations(
      &mock_bluetooth_scanning_prompt_view_);

  EXPECT_CALL(mock_bluetooth_scanning_prompt_view_, OnOptionUpdated(0))
      .Times(1);
  bluetooth_scanning_prompt_controller_.AddOrUpdateDevice(
      "id_a", true /* should_update_name */, base::ASCIIToUTF16("aa"));
  EXPECT_EQ(1u, bluetooth_scanning_prompt_controller_.NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("aa"),
            bluetooth_scanning_prompt_controller_.GetOption(0));
}

TEST_F(BluetoothScanningPromptControllerWithDevicesAddedTest,
       InitialNoOptionsText) {
  EXPECT_EQ(base::ASCIIToUTF16("No nearby devices found."),
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
