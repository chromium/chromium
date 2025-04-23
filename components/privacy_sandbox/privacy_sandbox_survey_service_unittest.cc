// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

class PrivacySandboxSurveyServiceTest : public testing::Test {
 public:
  PrivacySandboxSurveyServiceTest() = default;

 protected:
  PrivacySandboxSurveyService survey_service_;
};

// Basic test to ensure the service can be instantiated and destroyed.
TEST_F(PrivacySandboxSurveyServiceTest, CreateAndDestroy) {
  SUCCEED();
}

}  // namespace
}  // namespace privacy_sandbox
