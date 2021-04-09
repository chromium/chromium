// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/storage_manager/arc_storage_manager.h"

#include "components/arc/arc_service_manager.h"
#include "components/arc/test/test_browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcStorageManagerTest : public testing::Test {
 protected:
  ArcStorageManagerTest()
      : bridge_(ArcStorageManager::GetForBrowserContextForTesting(&context_)) {}
  ArcStorageManagerTest(const ArcStorageManagerTest&) = delete;
  ArcStorageManagerTest& operator=(const ArcStorageManagerTest&) = delete;
  ~ArcStorageManagerTest() override = default;

  ArcStorageManager* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  ArcStorageManager* const bridge_;
};

TEST_F(ArcStorageManagerTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace
}  // namespace arc
