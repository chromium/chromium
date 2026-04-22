// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_bridge.h"

#include <memory>

#include "chrome/browser/autofill/android/at_memory_bottom_sheet_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace autofill {
namespace {

class MockAtMemoryBottomSheetDelegate : public AtMemoryBottomSheetDelegate {
 public:
  MOCK_METHOD(void, OnDismissed, (), (override));
};

class AtMemoryBottomSheetBridgeTest : public testing::Test {
 protected:
  void SetUp() override {
    window_ = ui::WindowAndroid::CreateForTesting();
    bridge_ = std::make_unique<AtMemoryBottomSheetBridge>(window_->get());
  }

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<AtMemoryBottomSheetBridge> bridge_;
};

TEST_F(AtMemoryBottomSheetBridgeTest, OnDismissedCallsDelegate) {
  auto delegate = std::make_unique<MockAtMemoryBottomSheetDelegate>();
  MockAtMemoryBottomSheetDelegate* delegate_ptr = delegate.get();

  EXPECT_CALL(*delegate_ptr, OnDismissed());
  bridge_->RequestShowContent(std::move(delegate));
}

}  // namespace
}  // namespace autofill
