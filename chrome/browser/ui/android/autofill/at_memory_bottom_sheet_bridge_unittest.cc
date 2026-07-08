// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_bridge.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/android/at_memory_bottom_sheet_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace autofill {
namespace {

class MockAtMemoryBottomSheetDelegate : public AtMemoryBottomSheetDelegate {
 public:
  MOCK_METHOD(void, OnDismissed, (), (override));
  MOCK_METHOD(void,
              OnQuerySubmitted,
              (const std::u16string& query),
              (override));
  MOCK_METHOD(void,
              OnQueryTextChanged,
              (const std::u16string& query),
              (override));
  MOCK_METHOD(void, OnSuggestionSelected, (int position), (override));
  MOCK_METHOD(void, OnChildSuggestionsShown, (int parent_position), (override));
  MOCK_METHOD(void,
              OnChildSuggestionSelected,
              (int parent_position, int child_position),
              (override));
  MOCK_METHOD(bool, IsSearching, (), (const, override));
};

class AtMemoryBottomSheetBridgeTest : public testing::Test {
 protected:
  void SetUp() override {
    window_ = ui::WindowAndroid::CreateForTesting();
    bridge_ =
        std::make_unique<AtMemoryBottomSheetBridge>(window_->get(), &profile_);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<AtMemoryBottomSheetBridge> bridge_;
};

TEST_F(AtMemoryBottomSheetBridgeTest, OnDismissedCallsDelegate) {
  auto delegate = std::make_unique<MockAtMemoryBottomSheetDelegate>();
  MockAtMemoryBottomSheetDelegate* delegate_ptr = delegate.get();

  EXPECT_CALL(*delegate_ptr, OnDismissed());
  bridge_->RequestShowContent(std::move(delegate), {});
}

TEST_F(AtMemoryBottomSheetBridgeTest, HideDoesNotCrash) {
  bridge_->Hide();
}

TEST_F(AtMemoryBottomSheetBridgeTest, RequestShowContentWithChildren) {
  auto delegate = std::make_unique<MockAtMemoryBottomSheetDelegate>();
  MockAtMemoryBottomSheetDelegate* delegate_ptr = delegate.get();

  Suggestion child(u"Child label", SuggestionType::kAtMemorySearchResult);
  child.labels = {{Suggestion::Text(u"Child sublabel")}};
  Suggestion parent(u"Parent label", SuggestionType::kAtMemorySearchResult);
  parent.labels = {{Suggestion::Text(u"Parent sublabel")}};
  parent.children = {std::move(child)};

  EXPECT_CALL(*delegate_ptr, OnDismissed());
  bridge_->RequestShowContent(std::move(delegate), {parent});
}

}  // namespace
}  // namespace autofill
