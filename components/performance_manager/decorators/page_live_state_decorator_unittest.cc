// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class PageLiveStateDecoratorTest : public PerformanceManagerTestHarness {
 protected:
  PageLiveStateDecoratorTest() = default;
  ~PageLiveStateDecoratorTest() override = default;
  PageLiveStateDecoratorTest(const PageLiveStateDecoratorTest& other) = delete;
  PageLiveStateDecoratorTest& operator=(const PageLiveStateDecoratorTest&) =
      delete;

  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();
    SetContents(CreateTestWebContents());
  }

  void TearDown() override {
    DeleteContents();
    PerformanceManagerTestHarness::TearDown();
  }
};

TEST_F(PageLiveStateDecoratorTest, OnIsConnectedToUSBDeviceChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::IsConnectedToUSBDevice,
      &PageLiveStateDecorator::OnIsConnectedToUSBDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsConnectedToBluetoothDeviceChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(),
      &PageLiveStateDecorator::Data::IsConnectedToBluetoothDevice,
      &PageLiveStateDecorator::OnIsConnectedToBluetoothDeviceChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingVideoChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::IsCapturingVideo,
      &PageLiveStateDecorator::OnIsCapturingVideoChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingAudioChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::IsCapturingAudio,
      &PageLiveStateDecorator::OnIsCapturingAudioChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsBeingMirroredChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::IsBeingMirrored,
      &PageLiveStateDecorator::OnIsBeingMirroredChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingWindowChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::IsCapturingWindow,
      &PageLiveStateDecorator::OnIsCapturingWindowChanged);
}

TEST_F(PageLiveStateDecoratorTest, OnIsCapturingDisplayChanged) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::IsCapturingDisplay,
      &PageLiveStateDecorator::OnIsCapturingDisplayChanged);
}

TEST_F(PageLiveStateDecoratorTest, SetIsAutoDiscardable) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &PageLiveStateDecorator::Data::IsAutoDiscardable,
      &PageLiveStateDecorator::SetIsAutoDiscardable,
      /*default_state=*/true);
}

}  // namespace performance_manager
