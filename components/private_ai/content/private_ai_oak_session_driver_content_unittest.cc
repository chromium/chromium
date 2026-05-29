// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/content/private_ai_oak_session_driver_content.h"

#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

TEST(PrivateAiOakSessionDriverContentTest, Instance) {
  content::BrowserTaskEnvironment task_environment;
  PrivateAiOakSessionDriverContent driver;
  // This is a minimal test to ensure compilation.
  EXPECT_TRUE(true);
}

}  // namespace private_ai
