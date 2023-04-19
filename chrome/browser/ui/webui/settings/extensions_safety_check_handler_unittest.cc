// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/extensions_safety_check_handler.h"

#include <string>

#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestingExtensionsSafetyCheckHandler
    : public settings::ExtensionsSafetyCheckHandler {
 public:
  using ExtensionsSafetyCheckHandler::AllowJavascript;
  using ExtensionsSafetyCheckHandler::DisallowJavascript;
  using ExtensionsSafetyCheckHandler::GetExtensionsThatNeedReview;
};

}  // namespace

class ExtensionsSafetyCheckHandlerTest : public testing::Test {
 protected:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingExtensionsSafetyCheckHandler> entry_point_handler_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExtensionsSafetyCheckHandlerTest, GetExtensionsThatNeedReviewTest) {
  // Display string for 1 triggering extension.
  std::u16string display_string =
      u"1 potentially harmful extension is off. You can also remove it.";
  EXPECT_EQ(display_string,
            entry_point_handler_->GetExtensionsThatNeedReview());
}
