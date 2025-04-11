// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/persistent_cache/backend_params_manager.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/persistent_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace persistent_cache {

class BackendParamsManagerTest : public testing::Test {
  void SetUp() override { CHECK(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment;
};

TEST_F(BackendParamsManagerTest, UnknownKeyTypePairQueryServedAsynchronously) {
  BackendParamsManager params_manager(temp_dir_.GetPath());

  base::RunLoop run_loop;

  BackendParams backend_params;
  params_manager.GetParamsSyncOrCreateAsync(
      BackendType::kSqlite, "key",
      BackendParamsManager::AccessRights::kReadWrite,
      base::BindLambdaForTesting(
          [&backend_params, &run_loop](const BackendParams& result) {
            backend_params = result.Copy();
            run_loop.Quit();
          }));

  // The callback was not invoked yet so files are not populated.
  EXPECT_FALSE(backend_params.db_file.IsValid());
  EXPECT_FALSE(backend_params.journal_file.IsValid());

  // Wait for the callback to be invoked. If never invoked the test will time
  // out.
  run_loop.Run();

  // Once received the params contain valid files.
  EXPECT_TRUE(backend_params.db_file.IsValid());
  EXPECT_TRUE(backend_params.journal_file.IsValid());
}

TEST_F(BackendParamsManagerTest, ExistingKeyTypePairQueryServedSynchronously) {
  BackendParamsManager params_manager(temp_dir_.GetPath());

  {
    BackendParams backend_params;
    base::RunLoop run_loop;
    params_manager.GetParamsSyncOrCreateAsync(
        BackendType::kSqlite, "key",
        BackendParamsManager::AccessRights::kReadWrite,
        base::BindLambdaForTesting(
            [&backend_params, &run_loop](const BackendParams& result) {
              backend_params = result.Copy();
              run_loop.Quit();
            }));

    // The callback was not invoked yet so files are not populated.
    EXPECT_FALSE(backend_params.db_file.IsValid());
    EXPECT_FALSE(backend_params.journal_file.IsValid());

    // Makes sure the callback runs on the ThreadPool.
    run_loop.Run();

    // Once received the params contain valid files.
    EXPECT_TRUE(backend_params.db_file.IsValid());
    EXPECT_TRUE(backend_params.journal_file.IsValid());
  }

  {
    BackendParams backend_params;
    base::RunLoop run_loop;
    params_manager.GetParamsSyncOrCreateAsync(
        BackendType::kSqlite, "key",
        BackendParamsManager::AccessRights::kReadWrite,
        base::BindLambdaForTesting(
            [&backend_params, &run_loop](const BackendParams& result) {
              backend_params = result.Copy();
              run_loop.Quit();
            }));

    // No need to run `run_loop` since the callback was invoked synchronously.

    // Once received the params contain valid files.
    EXPECT_TRUE(backend_params.db_file.IsValid());
    EXPECT_TRUE(backend_params.journal_file.IsValid());
  }
}

}  // namespace persistent_cache
