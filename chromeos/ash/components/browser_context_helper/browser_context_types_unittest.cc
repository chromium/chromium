// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"

#include <memory>

#include "base/files/file_path.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(BrowserContextTypeTest, Test) {
  content::BrowserTaskEnvironment env;

  auto signin_browser_context = std::make_unique<content::TestBrowserContext>(
      base::FilePath(kSigninBrowserContextBaseName));
  auto lock_screen_app_browser_context =
      std::make_unique<content::TestBrowserContext>(
          base::FilePath(kLockScreenAppBrowserContextBaseName));
  auto lock_screen_browser_context =
      std::make_unique<content::TestBrowserContext>(
          base::FilePath(kLockScreenBrowserContextBaseName));
  auto shimless_rma_app_browser_context =
      std::make_unique<content::TestBrowserContext>(
          base::FilePath(kShimlessRmaAppBrowserContextBaseName));
  auto user_browser_context = std::make_unique<content::TestBrowserContext>(
      base::FilePath("test-user"));

  EXPECT_FALSE(IsSigninBrowserContext(nullptr));
  EXPECT_TRUE(IsSigninBrowserContext(signin_browser_context.get()));
  EXPECT_FALSE(IsSigninBrowserContext(lock_screen_app_browser_context.get()));
  EXPECT_FALSE(IsSigninBrowserContext(lock_screen_browser_context.get()));
  EXPECT_FALSE(IsSigninBrowserContext(shimless_rma_app_browser_context.get()));
  EXPECT_FALSE(IsSigninBrowserContext(user_browser_context.get()));

  EXPECT_FALSE(IsLockScreenAppBrowserContext(nullptr));
  EXPECT_FALSE(IsLockScreenAppBrowserContext(signin_browser_context.get()));
  EXPECT_TRUE(
      IsLockScreenAppBrowserContext(lock_screen_app_browser_context.get()));
  EXPECT_FALSE(
      IsLockScreenAppBrowserContext(lock_screen_browser_context.get()));
  EXPECT_FALSE(
      IsLockScreenAppBrowserContext(shimless_rma_app_browser_context.get()));
  EXPECT_FALSE(IsLockScreenAppBrowserContext(user_browser_context.get()));

  EXPECT_FALSE(IsLockScreenBrowserContext(nullptr));
  EXPECT_FALSE(IsLockScreenBrowserContext(signin_browser_context.get()));
  EXPECT_FALSE(
      IsLockScreenBrowserContext(lock_screen_app_browser_context.get()));
  EXPECT_TRUE(IsLockScreenBrowserContext(lock_screen_browser_context.get()));
  EXPECT_FALSE(
      IsLockScreenBrowserContext(shimless_rma_app_browser_context.get()));
  EXPECT_FALSE(IsLockScreenBrowserContext(user_browser_context.get()));

  EXPECT_FALSE(IsShimlessRmaAppBrowserContext(nullptr));
  EXPECT_FALSE(IsShimlessRmaAppBrowserContext(signin_browser_context.get()));
  EXPECT_FALSE(
      IsShimlessRmaAppBrowserContext(lock_screen_app_browser_context.get()));
  EXPECT_FALSE(
      IsShimlessRmaAppBrowserContext(lock_screen_browser_context.get()));
  EXPECT_TRUE(
      IsShimlessRmaAppBrowserContext(shimless_rma_app_browser_context.get()));
  EXPECT_FALSE(IsShimlessRmaAppBrowserContext(user_browser_context.get()));

  EXPECT_FALSE(IsUserBrowserContext(nullptr));
  EXPECT_FALSE(IsUserBrowserContext(signin_browser_context.get()));
  EXPECT_FALSE(IsUserBrowserContext(lock_screen_app_browser_context.get()));
  EXPECT_FALSE(IsUserBrowserContext(lock_screen_browser_context.get()));
  EXPECT_FALSE(IsUserBrowserContext(shimless_rma_app_browser_context.get()));
  EXPECT_TRUE(IsUserBrowserContext(user_browser_context.get()));

  EXPECT_FALSE(IsUserBrowserContextBaseName(
      signin_browser_context->GetPath().BaseName()));
  EXPECT_FALSE(IsUserBrowserContextBaseName(
      lock_screen_app_browser_context->GetPath().BaseName()));
  EXPECT_FALSE(IsUserBrowserContextBaseName(
      lock_screen_browser_context->GetPath().BaseName()));
  EXPECT_FALSE(IsUserBrowserContextBaseName(
      shimless_rma_app_browser_context->GetPath().BaseName()));
  EXPECT_TRUE(
      IsUserBrowserContextBaseName(user_browser_context->GetPath().BaseName()));
}

}  // namespace ash
