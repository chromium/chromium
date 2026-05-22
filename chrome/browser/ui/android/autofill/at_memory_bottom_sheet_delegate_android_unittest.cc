// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_delegate_android.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AtMemoryBottomSheetDelegateAndroidTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient client_;
};

TEST_F(AtMemoryBottomSheetDelegateAndroidTest, OnDismissedHidesSuggestions) {
  AtMemoryBottomSheetDelegateAndroid delegate(&client_);

  delegate.OnDismissed();

  EXPECT_EQ(client_.popup_hiding_reason(),
            SuggestionHidingReason::kUserAborted);
}

}  // namespace autofill
