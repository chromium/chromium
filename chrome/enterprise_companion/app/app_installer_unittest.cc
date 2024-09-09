// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/lock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#endif

namespace enterprise_companion {

namespace {

using ::testing::Return;

std::unique_ptr<ScopedLock> CreateLockForTest(base::TimeDelta) {
  std::string lock_name = base::UnguessableToken::Create().ToString();
#if BUILDFLAG(IS_WIN)
  CSecurityAttributes sa;
  return ScopedLock::Create(base::ASCIIToWide(lock_name), &sa,
                            base::Seconds(0));
#else
  return ScopedLock::Create(lock_name, base::Seconds(0));
#endif
}

}  // namespace

class AppInstallerTest : public ::testing::Test {
 private:
  base::test::TaskEnvironment environment_;
};

TEST_F(AppInstallerTest, ShutdownRemote) {
  base::MockCallback<base::OnceCallback<EnterpriseCompanionStatus()>>
      mock_shutdown_callback;
  EXPECT_CALL(mock_shutdown_callback, Run())
      .WillOnce(Return(EnterpriseCompanionStatus::Success()));

  EnterpriseCompanionStatus status =
      CreateAppInstaller(mock_shutdown_callback.Get(),
                         base::BindOnce(&CreateLockForTest),
                         /*task=*/base::BindOnce([] { return true; }))
          ->Run();

  EXPECT_TRUE(status.ok());
}

TEST_F(AppInstallerTest, LockContested) {
  EnterpriseCompanionStatus status =
      CreateAppInstaller(
          /*shutdown_remote_task=*/base::BindOnce(
              &EnterpriseCompanionStatus::Success),
          /*lock_provider=*/base::BindOnce([](base::TimeDelta) {
            return base::WrapUnique<ScopedLock>(nullptr);
          }),
          /*task=*/base::BindOnce([] { return true; }))
          ->Run();

  EXPECT_EQ(status,
            EnterpriseCompanionStatus(ApplicationError::kCannotAcquireLock));
}

TEST_F(AppInstallerTest, InstallFails) {
  EnterpriseCompanionStatus status =
      CreateAppInstaller(
          /*shutdown_remote_task=*/base::BindOnce(
              &EnterpriseCompanionStatus::Success),
          /*lock_provider=*/base::BindOnce(&CreateLockForTest),
          /*task=*/base::BindOnce([] { return false; }))
          ->Run();

  EXPECT_EQ(status,
            EnterpriseCompanionStatus(ApplicationError::kInstallationFailed));
}

TEST_F(AppInstallerTest, InstallSuccess) {
  EnterpriseCompanionStatus status =
      CreateAppInstaller(
          /*shutdown_remote_task=*/base::BindOnce(
              &EnterpriseCompanionStatus::Success),
          /*lock_provider=*/base::BindOnce(&CreateLockForTest),
          /*task=*/base::BindOnce([] { return true; }))
          ->Run();

  EXPECT_TRUE(status.ok());
}

}  // namespace enterprise_companion
