// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_cache.h"

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {
namespace {

auto ExpectPathExists() {
  return base::BindOnce(
      [](base::expected<base::FilePath, UnpackerError> result) {
        ASSERT_TRUE(result.has_value()) << static_cast<int>(result.error());
        EXPECT_TRUE(base::PathExists(result.value())) << result.value().value();
      });
}

auto ExpectError(UnpackerError error) {
  return base::BindOnce(
      [](UnpackerError error,
         base::expected<base::FilePath, UnpackerError> result) {
        ASSERT_FALSE(result.has_value()) << result.value();
        EXPECT_EQ(result.error(), error);
      },
      error);
}

auto ExpectHashes(
    const std::multimap<std::string, std::string>& expected_hashes) {
  return base::BindOnce(
      [](const std::multimap<std::string, std::string>& expected_hashes,
         const std::multimap<std::string, std::string>& result) {
        EXPECT_EQ(result, expected_hashes);
      },
      expected_hashes);
}

}  // namespace

class CrxCacheTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath TempPath(const std::string& basename) {
    return temp_dir_.GetPath().AppendUTF8(basename);
  }

  base::FilePath MakeFile() {
    base::FilePath out;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &out));
    return out;
  }

  void RunLoop() {
    loop_->Run();
    loop_ = std::make_unique<base::RunLoop>();
  }

  base::RepeatingClosure Quit() { return loop_->QuitClosure(); }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment env_;
  std::unique_ptr<base::RunLoop> loop_ = std::make_unique<base::RunLoop>();
};

TEST_F(CrxCacheTest, PutGet) {
  scoped_refptr<CrxCache> cache =
      base::MakeRefCounted<CrxCache>(TempPath("cache_dir"));
  cache->Put(MakeFile(), "appid", "hash", "fp", ExpectPathExists());
  cache->GetByFp("fp", ExpectPathExists());
  cache->GetByHash("hash", ExpectPathExists().Then(Quit()));
  RunLoop();
}

TEST_F(CrxCacheTest, GetMissing) {
  scoped_refptr<CrxCache> cache =
      base::MakeRefCounted<CrxCache>(TempPath("cache_dir"));
  cache->Put(MakeFile(), "appid", "hash", "fp", ExpectPathExists());
  cache->GetByFp("fp2", ExpectError(UnpackerError::kCrxCacheFileNotCached));
  cache->GetByHash(
      "hash2", ExpectError(UnpackerError::kCrxCacheFileNotCached).Then(Quit()));
  RunLoop();
}

TEST_F(CrxCacheTest, PutReplacesByAppId) {
  scoped_refptr<CrxCache> cache =
      base::MakeRefCounted<CrxCache>(TempPath("cache_dir"));
  cache->Put(MakeFile(), "appid", "hash", "fp", ExpectPathExists());
  cache->GetByHash("hash", ExpectPathExists().Then(Quit()));
  RunLoop();
  cache->Put(MakeFile(), "appid", "hash2", "fp2", ExpectPathExists());
  cache->GetByHash("hash", ExpectError(UnpackerError::kCrxCacheFileNotCached));
  cache->GetByHash("hash2", ExpectPathExists().Then(Quit()));
  RunLoop();
}

TEST_F(CrxCacheTest, PutAlreadyCached) {
  scoped_refptr<CrxCache> cache =
      base::MakeRefCounted<CrxCache>(TempPath("cache_dir"));
  cache->Put(MakeFile(), "appid", "hash", "fp", ExpectPathExists());
  cache->GetByHash(
      "hash",
      base::BindLambdaForTesting(
          [&](base::expected<base::FilePath, UnpackerError> result) {
            if (!result.has_value()) {
              Quit().Run();
            }
            ASSERT_TRUE(result.has_value()) << static_cast<int>(result.error());
            cache->Put(
                result.value(), "appid", "hash", "fp",
                base::BindLambdaForTesting(
                    [&](base::expected<base::FilePath, UnpackerError> result2) {
                      if (!result2.has_value()) {
                        Quit().Run();
                      }
                      ASSERT_TRUE(result2.has_value())
                          << static_cast<int>(result2.error());
                      cache->GetByHash("hash", ExpectPathExists().Then(Quit()));
                    }));
          }));
  RunLoop();
}

TEST_F(CrxCacheTest, CacheNotProvided) {
  scoped_refptr<CrxCache> cache = base::MakeRefCounted<CrxCache>(std::nullopt);
  cache->Put(base::FilePath(FILE_PATH_LITERAL("crxcache_test_file")), "appid",
             "hash", "fp", ExpectError(UnpackerError::kCrxCacheNotProvided));
  cache->GetByFp("fp", ExpectError(UnpackerError::kCrxCacheNotProvided));
  cache->GetByHash("hash", ExpectError(UnpackerError::kCrxCacheNotProvided));
  cache->RemoveAll("appid", base::DoNothing());
  cache->ListHashesByAppId(ExpectHashes({}).Then(Quit()));
  RunLoop();
}

TEST_F(CrxCacheTest, ListHashesByAppId) {
  scoped_refptr<CrxCache> cache =
      base::MakeRefCounted<CrxCache>(TempPath("cache_dir"));
  cache->ListHashesByAppId(ExpectHashes({}));
  cache->Put(MakeFile(), "appid", "hash", "fp", ExpectPathExists());
  cache->ListHashesByAppId(ExpectHashes({{"appid", "hash"}}));
  cache->Put(MakeFile(), "appid2", "hash2", "fp2", ExpectPathExists());
  cache->ListHashesByAppId(
      ExpectHashes({{"appid", "hash"}, {"appid2", "hash2"}}).Then(Quit()));
  RunLoop();
  cache->Put(MakeFile(), "appid", "hash3", "fp3", ExpectPathExists());
  cache->ListHashesByAppId(
      ExpectHashes({{"appid", "hash3"}, {"appid2", "hash2"}}).Then(Quit()));
  RunLoop();
}

TEST_F(CrxCacheTest, RemoveAll) {
  scoped_refptr<CrxCache> cache =
      base::MakeRefCounted<CrxCache>(TempPath("cache_dir"));
  cache->Put(MakeFile(), "appid", "hash", "fp", ExpectPathExists());
  cache->Put(MakeFile(), "appid2", "hash2", "fp2", ExpectPathExists());
  cache->ListHashesByAppId(
      ExpectHashes({{"appid", "hash"}, {"appid2", "hash2"}}).Then(Quit()));
  RunLoop();
  cache->RemoveAll("appid", base::DoNothing());
  cache->ListHashesByAppId(ExpectHashes({{"appid2", "hash2"}}).Then(Quit()));
  RunLoop();
  cache->RemoveAll("appid2", base::DoNothing());
  cache->ListHashesByAppId(ExpectHashes({}).Then(Quit()));
  RunLoop();
}

}  // namespace update_client
