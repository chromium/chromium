// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_bridge.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/at_memory_suggestion_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace autofill {
namespace {

class MockAtMemorySuggestionController : public AtMemorySuggestionController {
 public:
  MockAtMemorySuggestionController()
      : AtMemorySuggestionController(
            nullptr,
            nullptr,
            PopupControllerCommon({},
                                  gfx::RectF(),
                                  base::i18n::UNKNOWN_DIRECTION)) {}
  MOCK_METHOD(void, OnDismissed, (), (override));
};

class AtMemoryBottomSheetBridgeTest : public testing::Test {
 protected:
  void SetUp() override {
    window_ = ui::WindowAndroid::CreateForTesting();
    controller_ = std::make_unique<MockAtMemorySuggestionController>();
    bridge_ = std::make_unique<AtMemoryBottomSheetBridge>(
        window_->get(), &profile_, controller_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<MockAtMemorySuggestionController> controller_;
  std::unique_ptr<AtMemoryBottomSheetBridge> bridge_;
};

TEST_F(AtMemoryBottomSheetBridgeTest, OnDismissedCallsDelegate) {
  EXPECT_CALL(*controller_, OnDismissed());
  bridge_->OnDismissed(nullptr);
}

TEST_F(AtMemoryBottomSheetBridgeTest, HideDoesNotCrash) {
  bridge_->Hide();
}

TEST_F(AtMemoryBottomSheetBridgeTest, RequestShowContentWithChildren) {
  Suggestion child(u"Child label", SuggestionType::kAtMemorySearchResult);
  child.labels = {{Suggestion::Text(u"Child sublabel")}};
  Suggestion parent(u"Parent label", SuggestionType::kAtMemorySearchResult);
  parent.labels = {{Suggestion::Text(u"Parent sublabel")}};
  parent.children = {std::move(child)};

  EXPECT_CALL(*controller_, OnDismissed());
  bridge_->RequestShowContent({parent});
}

TEST_F(AtMemoryBottomSheetBridgeTest, RequestShowContentWithAtMemoryPayload) {
  Suggestion suggestion(u"Passport", SuggestionType::kAtMemorySearchResult);
  suggestion.labels = {{Suggestion::Text(u"Passport sublabel")}};
  suggestion.payload =
      Suggestion::AtMemoryPayload(u"Passport", MemoryDataType::kPassportNumber);

  EXPECT_CALL(*controller_, OnDismissed());
  bridge_->RequestShowContent({suggestion});
}

}  // namespace
}  // namespace autofill
