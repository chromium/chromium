// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_extensions_handler.h"

#include <string>

#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestingSafetyCheckExtensionsHandler
    : public settings::SafetyCheckExtensionsHandler {
 public:
  using SafetyCheckExtensionsHandler::AllowJavascript;
  using SafetyCheckExtensionsHandler::DisallowJavascript;
  using SafetyCheckExtensionsHandler::GetNumberOfExtensionsThatNeedReview;
};

}  // namespace

class SafetyCheckExtensionsHandlerTest : public testing::Test {
 protected:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingSafetyCheckExtensionsHandler> entry_point_handler_;
  base::test::ScopedFeatureList feature_list_;
};

// TODO(psarouthakis): Add comprehensive unit tests once CWSInfo Service is in
// place and can return real values.
TEST_F(SafetyCheckExtensionsHandlerTest,
       GetNumberOfExtensionsThatNeedReviewTest) {
  // Display string for 2 triggering extensions.
  int expected_number_of_extensions = 2;
  EXPECT_EQ(expected_number_of_extensions,
            entry_point_handler_->GetNumberOfExtensionsThatNeedReview());
}
