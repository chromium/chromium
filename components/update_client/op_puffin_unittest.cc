// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_puffin.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

class PuffOperationTest : public testing::Test {
 private:
  // env_ must be constructed before sequence_checker_.
  base::test::TaskEnvironment env_;

 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath TempPath(const std::string& basename) {
    return temp_dir_.GetPath().AppendUTF8(basename);
  }

  base::FilePath CopyToTemp(const std::string& src) {
    base::FilePath dest =
        TempPath(base::FilePath().AppendUTF8(src).BaseName().AsUTF8Unsafe());
    EXPECT_TRUE(base::CopyFile(GetTestFilePath(src.c_str()), dest));
    return dest;
  }

  base::RepeatingCallback<void(base::Value::Dict)> MakePingCallback() {
    return base::BindLambdaForTesting(
        [&](base::Value::Dict ping) { pings_.push_back(std::move(ping)); });
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::RunLoop loop_;
  std::vector<base::Value::Dict> pings_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(PuffOperationTest, Success) {
  auto cache = base::MakeRefCounted<CrxCache>(TempPath("cache"));

  // PuffOperation deletes the patch file, so copying it to the temp dir.
  base::FilePath patch_file =
      CopyToTemp("puffin_patch_test/puffin_app_v1_to_v2.puff");

  // Since CrxCache::Put moves the v1 file, make a copy in the temp dir.
  base::FilePath old_file = CopyToTemp("puffin_patch_test/puffin_app_v1.crx3");

  cache->Put(
      old_file, "appid", "hash1", "prev_fp",
      base::BindLambdaForTesting([&](base::expected<base::FilePath,
                                                    UnpackerError> r) {
        ASSERT_TRUE(r.has_value());
        PuffOperation(
            cache,
            base::MakeRefCounted<PatchChromiumFactory>(
                base::BindRepeating(&patch::LaunchInProcessFilePatcher))
                ->Create(),
            MakePingCallback(), "hash1",
            "c7f9a9230b82c8b3670e539d8034e5386f17bfa1bdcd4a2cc385844f9252052f",
            patch_file,
            base::BindLambdaForTesting(
                [&](base::expected<base::FilePath, CategorizedError> result) {
                  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
                  loop_.Quit();
                  ASSERT_TRUE(result.has_value());
                  EXPECT_TRUE(base::ContentsEqual(
                      GetTestFilePath("puffin_patch_test/puffin_app_v2.crx3"),
                      result.value()));
                }));
      }));
  loop_.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventPuff);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultSuccess);
  EXPECT_EQ(pings_[0].Find("errorcat"), nullptr);
  EXPECT_EQ(pings_[0].Find("errorcode"), nullptr);
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

TEST_F(PuffOperationTest, BadPatch) {
  auto cache = base::MakeRefCounted<CrxCache>(TempPath("cache"));

  // Since PuffOperation deletes the patch file, make a copy in the temp dir.
  // For this test, use an malformed puff file - a copy of v2.crx is good
  // enough.
  base::FilePath patch_file =
      CopyToTemp("puffin_patch_test/puffin_app_v2.crx3");

  // Since CrxCache::Put moves the v1 file, make a copy in the temp dir.
  base::FilePath old_file = CopyToTemp("puffin_patch_test/puffin_app_v1.crx3");

  cache->Put(
      old_file, "appid", "hash1", "prev_fp",
      base::BindLambdaForTesting([&](base::expected<base::FilePath,
                                                    UnpackerError> r) {
        ASSERT_TRUE(r.has_value());
        PuffOperation(
            cache,
            base::MakeRefCounted<PatchChromiumFactory>(
                base::BindRepeating(&patch::LaunchInProcessFilePatcher))
                ->Create(),
            MakePingCallback(), "hash1",
            "c7f9a9230b82c8b3670e539d8034e5386f17bfa1bdcd4a2cc385844f9252052f",
            patch_file,
            base::BindLambdaForTesting(
                [&](base::expected<base::FilePath, CategorizedError> result) {
                  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
                  loop_.Quit();
                  ASSERT_FALSE(result.has_value());
                  EXPECT_EQ(
                      result.error().code,
                      static_cast<int>(UnpackerError::kDeltaOperationFailure));
                }));
      }));
  loop_.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventPuff);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultError);
  EXPECT_EQ(pings_[0].FindInt("errorcat"),
            static_cast<int>(ErrorCategory::kUnpack));
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(UnpackerError::kDeltaOperationFailure));
  EXPECT_EQ(pings_[0].FindInt("extracode1"),
            static_cast<int>(Error::INVALID_ARGUMENT));
}

TEST_F(PuffOperationTest, NotInCache) {
  auto cache = base::MakeRefCounted<CrxCache>(TempPath("cache"));

  // PuffOperation deletes the patch file, so copying it to the temp dir.
  base::FilePath patch_file =
      CopyToTemp("puffin_patch_test/puffin_app_v1_to_v2.puff");

  PuffOperation(
      cache,
      base::MakeRefCounted<PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))
          ->Create(),
      MakePingCallback(), "prev_fp",
      "c7f9a9230b82c8b3670e539d8034e5386f17bfa1bdcd4a2cc385844f9252052f",
      patch_file,
      base::BindLambdaForTesting(
          [&](base::expected<base::FilePath, CategorizedError> result) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
            loop_.Quit();
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error().code,
                      static_cast<int>(UnpackerError::kCrxCacheFileNotCached));
          }));
  loop_.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventPuff);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultError);
  EXPECT_EQ(pings_[0].FindInt("errorcat"),
            static_cast<int>(ErrorCategory::kUnpack));
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(UnpackerError::kCrxCacheFileNotCached));
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

TEST_F(PuffOperationTest, NoCache) {
  // PuffOperation deletes the patch file, so copying it to the temp dir.
  base::FilePath patch_file =
      CopyToTemp("puffin_patch_test/puffin_app_v1_to_v2.puff");

  PuffOperation(
      base::MakeRefCounted<CrxCache>(std::nullopt),
      base::MakeRefCounted<PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))
          ->Create(),
      MakePingCallback(), "prev_fp",
      "c7f9a9230b82c8b3670e539d8034e5386f17bfa1bdcd4a2cc385844f9252052f",
      patch_file,
      base::BindLambdaForTesting(
          [&](base::expected<base::FilePath, CategorizedError> result) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
            loop_.Quit();
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error().code,
                      static_cast<int>(UnpackerError::kCrxCacheNotProvided));
          }));
  loop_.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventPuff);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultError);
  EXPECT_EQ(pings_[0].FindInt("errorcat"),
            static_cast<int>(ErrorCategory::kUnpack));
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(UnpackerError::kCrxCacheNotProvided));
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

TEST_F(PuffOperationTest, OutHashMismatch) {
  auto cache = base::MakeRefCounted<CrxCache>(TempPath("cache"));

  // PuffOperation deletes the patch file, so copying it to the temp dir.
  base::FilePath patch_file =
      CopyToTemp("puffin_patch_test/puffin_app_v1_to_v2.puff");

  // Since CrxCache::Put moves the v1 file, make a copy in the temp dir.
  base::FilePath old_file = CopyToTemp("puffin_patch_test/puffin_app_v1.crx3");

  cache->Put(
      old_file, "appid", "hash1", "prev_fp",
      base::BindLambdaForTesting([&](base::expected<base::FilePath,
                                                    UnpackerError> r) {
        ASSERT_TRUE(r.has_value());
        PuffOperation(
            cache,
            base::MakeRefCounted<PatchChromiumFactory>(
                base::BindRepeating(&patch::LaunchInProcessFilePatcher))
                ->Create(),
            MakePingCallback(), "hash1", "incorrecthash", patch_file,
            base::BindLambdaForTesting(
                [&](base::expected<base::FilePath, CategorizedError> result) {
                  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
                  loop_.Quit();
                  ASSERT_FALSE(result.has_value());
                  EXPECT_EQ(
                      result.error().code,
                      static_cast<int>(UnpackerError::kPatchOutHashMismatch));
                }));
      }));
  loop_.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventPuff);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultError);
  EXPECT_EQ(pings_[0].FindInt("errorcat"),
            static_cast<int>(ErrorCategory::kUnpack));
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(UnpackerError::kPatchOutHashMismatch));
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

}  // namespace update_client
