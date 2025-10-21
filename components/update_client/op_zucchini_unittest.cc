// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_zucchini.h"

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_client_errors.h"
#include "components/zucchini/zucchini.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

class ZucchiniOperationTest : public testing::Test {
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

  base::RepeatingCallback<void(update_client::ComponentState)>
  MakeStateCallback() {
    return base::BindRepeating([](update_client::ComponentState state) {
      ASSERT_EQ(state, update_client::ComponentState::kPatching);
    });
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::RunLoop loop_;
  std::vector<base::Value::Dict> pings_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(ZucchiniOperationTest, Success) {
  auto cache = base::MakeRefCounted<CrxCache>(TempPath("cache"));

  // `ZucchiniOperation` deletes the patch file, so copy it to the temp dir.
  base::FilePath patch_file =
      CopyToTemp("zucchini_patch_test/app1_to_app2.zucchini");

  // `CrxCache::Put` will move the v1 file, so copy it into the temp dir.
  base::FilePath old_file = CopyToTemp("zucchini_patch_test/app1.zip");

  cache->Put(
      old_file, "appid", "hash1",
      base::BindLambdaForTesting([&](base::expected<base::FilePath,
                                                    UnpackerError> r) {
        ASSERT_TRUE(r.has_value());
        ZucchiniOperation(
            cache,
            base::MakeRefCounted<PatchChromiumFactory>(
                base::BindRepeating(&patch::LaunchInProcessFilePatcher))
                ->Create(),
            MakePingCallback(), MakeStateCallback(), "hash1",
            "30ab1a10edb5b33a63f61263f702b71c2ad3043773fef2a2122111ea542b765a",
            patch_file,
            base::BindLambdaForTesting(
                [&](base::expected<base::FilePath, CategorizedError> result) {
                  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
                  loop_.Quit();
                  ASSERT_TRUE(result.has_value());
                  EXPECT_TRUE(base::ContentsEqual(
                      GetTestFilePath("zucchini_patch_test/app2.zip"),
                      result.value()));
                }));
      }));
  loop_.Run();
  EXPECT_FALSE(base::PathExists(patch_file));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventZucchini);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultSuccess);
  EXPECT_EQ(pings_[0].Find("errorcat"), nullptr);
  EXPECT_EQ(pings_[0].Find("errorcode"), nullptr);
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

TEST_F(ZucchiniOperationTest, BadPatch) {
  auto cache = base::MakeRefCounted<CrxCache>(TempPath("cache"));

  // ZucchiniOperation deletes the patch file, so copy it to the temp dir. For
  // this test, use an malformed patch file - a copy of app2.zip is good
  // enough.
  base::FilePath patch_file = CopyToTemp("zucchini_patch_test/app2.zip");

  // CrxCache::Put will move the v1 file, so copy it into the temp dir.
  base::FilePath old_file = CopyToTemp("zucchini_patch_test/app1.zip");

  cache->Put(
      old_file, "appid", "hash1",
      base::BindLambdaForTesting([&](base::expected<base::FilePath,
                                                    UnpackerError> r) {
        ASSERT_TRUE(r.has_value());
        ZucchiniOperation(
            cache,
            base::MakeRefCounted<PatchChromiumFactory>(
                base::BindRepeating(&patch::LaunchInProcessFilePatcher))
                ->Create(),
            MakePingCallback(), MakeStateCallback(), "hash1",
            "30ab1a10edb5b33a63f61263f702b71c2ad3043773fef2a2122111ea542b765a",
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
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventZucchini);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultError);
  EXPECT_EQ(pings_[0].FindInt("errorcat"),
            static_cast<int>(ErrorCategory::kUnpack));
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(UnpackerError::kDeltaOperationFailure));
  EXPECT_EQ(pings_[0].FindInt("extracode1"),
            static_cast<int>(zucchini::status::kStatusPatchReadError));
}

TEST_F(ZucchiniOperationTest, NotInCache) {
  auto cache = base::MakeRefCounted<CrxCache>(TempPath("cache"));

  // ZucchiniOperation deletes the patch file, so copy it to the temp dir.
  base::FilePath patch_file =
      CopyToTemp("zucchini_patch_test/app1_to_app2.zucchini");

  ZucchiniOperation(
      cache,
      base::MakeRefCounted<PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))
          ->Create(),
      MakePingCallback(), MakeStateCallback(), {},
      "30ab1a10edb5b33a63f61263f702b71c2ad3043773fef2a2122111ea542b765a",
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
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventZucchini);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultError);
  EXPECT_EQ(pings_[0].FindInt("errorcat"),
            static_cast<int>(ErrorCategory::kUnpack));
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(UnpackerError::kCrxCacheFileNotCached));
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

TEST_F(ZucchiniOperationTest, NoCache) {
  // ZucchiniOperation deletes the patch file, so copy it to the temp dir.
  base::FilePath patch_file =
      CopyToTemp("zucchini_patch_test/app1_to_app2.zucchini");

  ZucchiniOperation(
      base::MakeRefCounted<CrxCache>(std::nullopt),
      base::MakeRefCounted<PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))
          ->Create(),
      MakePingCallback(), MakeStateCallback(), {},
      "30ab1a10edb5b33a63f61263f702b71c2ad3043773fef2a2122111ea542b765a",
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
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventZucchini);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultError);
  EXPECT_EQ(pings_[0].FindInt("errorcat"),
            static_cast<int>(ErrorCategory::kUnpack));
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(UnpackerError::kCrxCacheNotProvided));
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

TEST_F(ZucchiniOperationTest, OutHashMismatch) {
  auto cache = base::MakeRefCounted<CrxCache>(TempPath("cache"));

  // `ZucchiniOperation` deletes the patch file, so copy it to the temp dir.
  base::FilePath patch_file =
      CopyToTemp("zucchini_patch_test/app1_to_app2.zucchini");

  // `CrxCache::Put` will move the v1 file, so copy it into the temp dir.
  base::FilePath old_file = CopyToTemp("zucchini_patch_test/app1.zip");

  cache->Put(
      old_file, "appid", "hash1",
      base::BindLambdaForTesting([&](base::expected<base::FilePath,
                                                    UnpackerError> r) {
        ASSERT_TRUE(r.has_value());
        ZucchiniOperation(
            cache,
            base::MakeRefCounted<PatchChromiumFactory>(
                base::BindRepeating(&patch::LaunchInProcessFilePatcher))
                ->Create(),
            MakePingCallback(), MakeStateCallback(), "hash1", "incorrecthash",
            patch_file,
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
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventZucchini);
  EXPECT_EQ(pings_[0].FindInt("eventresult"),
            protocol_request::kEventResultError);
  EXPECT_EQ(pings_[0].FindInt("errorcat"),
            static_cast<int>(ErrorCategory::kUnpack));
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(UnpackerError::kPatchOutHashMismatch));
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

}  // namespace update_client
