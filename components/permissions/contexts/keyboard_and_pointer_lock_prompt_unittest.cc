// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/permissions/contexts/keyboard_lock_permission_context.h"
#include "components/permissions/contexts/pointer_lock_permission_context.h"
#include "components/permissions/features.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

using blink::mojom::PermissionStatus;

class KeyboardAndPointerLockPromptTests : public testing::Test {
 public:
  content::TestBrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestPermissionsClient client_;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(KeyboardAndPointerLockPromptTests,
       KeyboardAndPointerLockPromptDisabled) {
  GURL url("https://www.example.com");

  feature_list_.Reset();
  feature_list_.InitWithFeatureState(features::kKeyboardAndPointerLockPrompt,
                                     false);

  KeyboardLockPermissionContext keyboard_permission_context(browser_context());
  PointerLockPermissionContext pointer_permission_context(browser_context());

  EXPECT_EQ(PermissionStatus::GRANTED,
            keyboard_permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr, url, url)
                .status);

  EXPECT_EQ(PermissionStatus::GRANTED,
            pointer_permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr, url, url)
                .status);
}

TEST_F(KeyboardAndPointerLockPromptTests, KeyboardAndPointerLockPromptEnabled) {
  GURL url("https://www.example.com");

  feature_list_.Reset();
  feature_list_.InitWithFeatureState(features::kKeyboardAndPointerLockPrompt,
                                     true);

  KeyboardLockPermissionContext keyboard_permission_context(browser_context());
  PointerLockPermissionContext pointer_permission_context(browser_context());

  EXPECT_EQ(PermissionStatus::ASK,
            keyboard_permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr, url, url)
                .status);

  EXPECT_EQ(PermissionStatus::ASK,
            pointer_permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr, url, url)
                .status);
}
#endif

}  // namespace permissions
