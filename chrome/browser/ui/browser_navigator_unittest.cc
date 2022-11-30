// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class BrowserNavigatorUnitTest : public testing::Test {
 public:
  BrowserNavigatorUnitTest() = default;
  BrowserNavigatorUnitTest(const BrowserNavigatorUnitTest&) = delete;
  BrowserNavigatorUnitTest& operator=(const BrowserNavigatorUnitTest&) = delete;
  ~BrowserNavigatorUnitTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Ensure empty view source is allowed in Incognito.
TEST_F(BrowserNavigatorUnitTest, EmptyViewSourceIncognito) {
  TestingProfile profile;
  EXPECT_TRUE(IsURLAllowedInIncognito(GURL("view-source:"), &profile));
}
