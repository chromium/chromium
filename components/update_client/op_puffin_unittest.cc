// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_puffin.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

TEST(PuffOperationTest, Success) {
  base::test::TaskEnvironment env;
  SEQUENCE_CHECKER(sequence_checker);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto cache =
      base::MakeRefCounted<CrxCache>(CrxCache::Options(temp_dir.GetPath()));
  base::RunLoop loop;

  // PuffOperation deletes the patch file, so copying it to the temp dir.
  base::FilePath patch_file = temp_dir.GetPath().AppendASCII("patchfile1");
  ASSERT_TRUE(base::CopyFile(
      GetTestFilePath("puffin_patch_test/puffin_app_v1_to_v2.puff"),
      patch_file));

  // CrxCache::Put will move the v1 file, so copy it into the temp dir.
  base::FilePath old_file = temp_dir.GetPath().AppendASCII("v1");
  ASSERT_TRUE(base::CopyFile(
      GetTestFilePath("puffin_patch_test/puffin_app_v1.crx3"), old_file));

  cache->Put(
      old_file, "appid", "prev_fp",
      base::BindLambdaForTesting([&](const CrxCache::Result& r) {
        ASSERT_EQ(r.error, UnpackerError::kNone);
        PuffOperation(
            cache,
            base::MakeRefCounted<PatchChromiumFactory>(
                base::BindRepeating(&patch::LaunchInProcessFilePatcher))
                ->Create(),
            base::DoNothing(), "appid", "prev_fp", patch_file,
            temp_dir.GetPath(),
            base::BindLambdaForTesting(
                [&](const base::expected<base::FilePath, CategorizedError>&
                        result) {
                  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
                  loop.Quit();
                  ASSERT_TRUE(result.has_value());
                  EXPECT_TRUE(base::ContentsEqual(
                      GetTestFilePath("puffin_patch_test/puffin_app_v2.crx3"),
                      result.value()));
                }));
      }));
  loop.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
}

TEST(PuffOperationTest, BadPatch) {
  base::test::TaskEnvironment env;
  SEQUENCE_CHECKER(sequence_checker);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto cache =
      base::MakeRefCounted<CrxCache>(CrxCache::Options(temp_dir.GetPath()));
  base::RunLoop loop;

  // PuffOperation deletes the patch file, so copy it to the temp dir. For this
  // test, use an malformed puff file - a copy of v1.crx is good enough.
  base::FilePath patch_file = temp_dir.GetPath().AppendASCII("patchfile1");
  ASSERT_TRUE(base::CopyFile(
      GetTestFilePath("puffin_patch_test/puffin_app_v1.crx3"), patch_file));

  // CrxCache::Put will move the v1 file, so copy it into the temp dir.
  base::FilePath old_file = temp_dir.GetPath().AppendASCII("v1");
  ASSERT_TRUE(base::CopyFile(
      GetTestFilePath("puffin_patch_test/puffin_app_v1.crx3"), old_file));

  cache->Put(
      old_file, "appid", "prev_fp",
      base::BindLambdaForTesting([&](const CrxCache::Result& r) {
        ASSERT_EQ(r.error, UnpackerError::kNone);
        PuffOperation(
            cache,
            base::MakeRefCounted<PatchChromiumFactory>(
                base::BindRepeating(&patch::LaunchInProcessFilePatcher))
                ->Create(),
            base::DoNothing(), "appid", "prev_fp", patch_file,
            temp_dir.GetPath(),
            base::BindLambdaForTesting(
                [&](const base::expected<base::FilePath, CategorizedError>&
                        result) {
                  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
                  loop.Quit();
                  ASSERT_FALSE(result.has_value());
                  EXPECT_EQ(
                      result.error().code_,
                      static_cast<int>(UnpackerError::kDeltaOperationFailure));
                }));
      }));
  loop.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
}

TEST(PuffOperationTest, NotInCache) {
  base::test::TaskEnvironment env;
  SEQUENCE_CHECKER(sequence_checker);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto cache =
      base::MakeRefCounted<CrxCache>(CrxCache::Options(temp_dir.GetPath()));
  base::RunLoop loop;

  // PuffOperation deletes the patch file, so copying it to the temp dir.
  base::FilePath patch_file = temp_dir.GetPath().AppendASCII("patchfile1");
  ASSERT_TRUE(base::CopyFile(
      GetTestFilePath("puffin_patch_test/puffin_app_v1_to_v2.puff"),
      patch_file));

  PuffOperation(
      cache,
      base::MakeRefCounted<PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))
          ->Create(),
      base::DoNothing(), "appid", "prev_fp", patch_file, temp_dir.GetPath(),
      base::BindLambdaForTesting(
          [&](const base::expected<base::FilePath, CategorizedError>& result) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
            loop.Quit();
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(
                result.error().code_,
                static_cast<int>(UnpackerError::kPuffinMissingPreviousCrx));
          }));
  loop.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
}

TEST(PuffOperationTest, NoCache) {
  base::test::TaskEnvironment env;
  SEQUENCE_CHECKER(sequence_checker);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::RunLoop loop;

  // PuffOperation deletes the patch file, so copying it to the temp dir.
  base::FilePath patch_file = temp_dir.GetPath().AppendASCII("patchfile1");
  ASSERT_TRUE(base::CopyFile(
      GetTestFilePath("puffin_patch_test/puffin_app_v1_to_v2.puff"),
      patch_file));

  PuffOperation(
      std::nullopt,
      base::MakeRefCounted<PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))
          ->Create(),
      base::DoNothing(), "appid", "prev_fp", patch_file, temp_dir.GetPath(),
      base::BindLambdaForTesting(
          [&](const base::expected<base::FilePath, CategorizedError>& result) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
            loop.Quit();
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error().code_,
                      static_cast<int>(UnpackerError::kCrxCacheNotProvided));
          }));
  loop.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
}

}  // namespace update_client
