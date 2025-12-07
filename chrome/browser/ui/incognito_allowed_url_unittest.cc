// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/incognito_allowed_url.h"

#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class IncognitoAllowedUrlUnitTest : public testing::Test {
 public:
  IncognitoAllowedUrlUnitTest() = default;
  IncognitoAllowedUrlUnitTest(const IncognitoAllowedUrlUnitTest&) = delete;
  IncognitoAllowedUrlUnitTest& operator=(const IncognitoAllowedUrlUnitTest&) =
      delete;
  ~IncognitoAllowedUrlUnitTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Ensure empty view source is allowed in Incognito.
TEST_F(IncognitoAllowedUrlUnitTest, EmptyViewSourceIncognito) {
  EXPECT_TRUE(IsURLAllowedInIncognito(GURL("view-source:")));
}
