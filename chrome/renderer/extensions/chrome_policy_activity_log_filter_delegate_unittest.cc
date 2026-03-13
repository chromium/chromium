// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_policy_activity_log_filter_delegate.h"

#include "base/values.h"
#include "extensions/common/dom_action_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class ChromePolicyActivityLogFilterDelegateTest : public testing::Test {
 public:
  ChromePolicyActivityLogFilterDelegateTest() = default;
  ~ChromePolicyActivityLogFilterDelegateTest() override = default;

 protected:
  void SetUp() override {
    filter_delegate_ =
        std::make_unique<ChromePolicyActivityLogFilterDelegate>();
  }

  void TearDown() override { filter_delegate_.reset(); }

  ChromePolicyActivityLogFilterDelegate* filter() {
    return filter_delegate_.get();
  }

 private:
  std::unique_ptr<ChromePolicyActivityLogFilterDelegate> filter_delegate_;
};

// Verifies that the delegate correctly forwards and returns true for a known
// high-risk event (tested exhaustively in ActivityLogPolicyUtilTest).
TEST_F(ChromePolicyActivityLogFilterDelegateTest, ReturnsTrueForHighRisk) {
  const GURL kUrl("https://www.google.com");
  EXPECT_TRUE(filter()->IsHighRiskEvent("ext", DomActionType::GETTER,
                                        "Document.cookie", base::ListValue(),
                                        kUrl));
}

// Verifies that the delegate correctly forwards and returns false for a known
// benign event.
TEST_F(ChromePolicyActivityLogFilterDelegateTest, ReturnsFalseForBenign) {
  const GURL kUrl("https://www.google.com");
  base::ListValue div_args;
  div_args.Append("div");
  EXPECT_FALSE(filter()->IsHighRiskEvent("ext", DomActionType::METHOD,
                                         "blinkAddElement", div_args, kUrl));
}

}  // namespace extensions
