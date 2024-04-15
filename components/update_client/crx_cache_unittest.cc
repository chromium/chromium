// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_cache.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {
namespace {

base::FilePath BuildCrxFilePathForTest(const base::FilePath& dir_path,
                                       const std::string& id,
                                       const std::string& fp) {
  return dir_path.AppendASCII("crx_cache_unittest_cache_dir")
      .AppendASCII(base::JoinString({id, fp}, "_"));
}

}  // namespace

class CrxCacheTest : public testing::Test {
 public:
  CrxCacheTest() = default;
  ~CrxCacheTest() override = default;

 private:
  base::test::TaskEnvironment env_;
};

TEST_F(CrxCacheTest, CheckGetSucceeds) {
  std::string id = "rightid";
  std::string fp = "rightfp";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath expected_crx_path =
      BuildCrxFilePathForTest(temp_dir.GetPath(), id, fp);
  EXPECT_TRUE(base::CreateDirectory(expected_crx_path.DirName()));
  {
    base::File crx_file(expected_crx_path, base::File::FLAG_CREATE_ALWAYS |
                                               base::File::FLAG_WRITE |
                                               base::File::FLAG_READ);
  }
  CrxCache::Options options(expected_crx_path.DirName());
  scoped_refptr<CrxCache> cache = base::MakeRefCounted<CrxCache>(options);
  base::RunLoop loop;
  cache->Get(id, fp,
             base::BindLambdaForTesting(
                 [&loop, &expected_crx_path](const CrxCache::Result& result) {
                   EXPECT_EQ(result.error, UnpackerError::kNone);
                   base::FilePath crx_cache_path = result.crx_cache_path;
                   EXPECT_TRUE(base::DirectoryExists(crx_cache_path.DirName()));
                   EXPECT_TRUE(base::PathExists(crx_cache_path));
                   EXPECT_EQ(crx_cache_path, expected_crx_path);
                   loop.Quit();
                 }));
  loop.Run();
  EXPECT_TRUE(base::DeletePathRecursively(temp_dir.GetPath()));
}

TEST_F(CrxCacheTest, CheckGetWithMissingFileFails) {
  std::string id = "rightid";
  std::string fp = "rightfp";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath expected_crx_path =
      BuildCrxFilePathForTest(temp_dir.GetPath(), id, fp);
  EXPECT_TRUE(base::CreateDirectory(expected_crx_path.DirName()));
  {
    base::File crx_file(expected_crx_path, base::File::FLAG_CREATE_ALWAYS |
                                               base::File::FLAG_WRITE |
                                               base::File::FLAG_READ);
  }
  CrxCache::Options options(expected_crx_path.DirName());
  scoped_refptr<CrxCache> cache = base::MakeRefCounted<CrxCache>(options);
  base::RunLoop loop;
  cache->Get(
      "wrong_id", "wrong_fp",
      base::BindLambdaForTesting([&loop](const CrxCache::Result& result) {
        EXPECT_EQ(result.error, UnpackerError::kPuffinMissingPreviousCrx);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_TRUE(base::DeletePathRecursively(temp_dir.GetPath()));
}

TEST_F(CrxCacheTest, CheckPutWithExistingEmptyCrxCachePathSucceeds) {
  std::string id = "rightid";
  std::string fp = "rightfp";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath expected_crx_path =
      BuildCrxFilePathForTest(temp_dir.GetPath(), id, fp);
  EXPECT_TRUE(base::CreateDirectory(expected_crx_path.DirName()));
  base::RunLoop loop;
  CrxCache::Options options(expected_crx_path.DirName());
  scoped_refptr<CrxCache> cache = base::MakeRefCounted<CrxCache>(options);
  cache->Put(
      DuplicateTestFile(temp_dir.GetPath(),
                        "jebgalgnebhfojomionfpkfelancnnkf.crx"),
      id, fp,
      base::BindLambdaForTesting([&loop](const CrxCache::Result& result) {
        EXPECT_EQ(result.error, UnpackerError::kNone);
        base::FilePath crx_cache_path = result.crx_cache_path;
        EXPECT_TRUE(base::DirectoryExists(crx_cache_path.DirName()));
        loop.Quit();
      }));
  loop.Run();
  EXPECT_TRUE(base::DeletePathRecursively(temp_dir.GetPath()));
}

TEST_F(CrxCacheTest, CheckPutWithNonExistentCrxCacheDirSucceeds) {
  std::string id = "rightid";
  std::string fp = "rightfp";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath expected_crx_path =
      BuildCrxFilePathForTest(temp_dir.GetPath(), id, fp);
  base::RunLoop loop;
  CrxCache::Options options(expected_crx_path.DirName());
  scoped_refptr<CrxCache> cache = base::MakeRefCounted<CrxCache>(options);
  cache->Put(
      DuplicateTestFile(temp_dir.GetPath(),
                        "jebgalgnebhfojomionfpkfelancnnkf.crx"),
      id, fp,
      base::BindLambdaForTesting([&loop](const CrxCache::Result& result) {
        EXPECT_EQ(result.error, UnpackerError::kNone);
        base::FilePath crx_cache_path = result.crx_cache_path;
        EXPECT_TRUE(base::DirectoryExists(crx_cache_path.DirName()));
        loop.Quit();
      }));
  loop.Run();
  EXPECT_TRUE(base::DeletePathRecursively(temp_dir.GetPath()));
}

TEST_F(CrxCacheTest, CheckPutPreexistingCrxReplacementSucceeds) {
  std::string id = "rightid";
  std::string fp = "rightfp";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath expected_crx_path =
      BuildCrxFilePathForTest(temp_dir.GetPath(), id, fp);
  base::FilePath jebg_duplicate_path = DuplicateTestFile(
      temp_dir.GetPath(), "jebgalgnebhfojomionfpkfelancnnkf.crx");
  {
    base::RunLoop loop;
    // Put jebg successfully.
    CrxCache::Options options(expected_crx_path.DirName());
    scoped_refptr<CrxCache> cache = base::MakeRefCounted<CrxCache>(options);
    cache->Put(
        jebg_duplicate_path, id, fp,
        base::BindLambdaForTesting([&loop](const CrxCache::Result& result) {
          EXPECT_EQ(result.error, UnpackerError::kNone);
          EXPECT_TRUE(base::ContentsEqual(
              GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
              result.crx_cache_path));
          EXPECT_TRUE(base::DeleteFile(result.crx_cache_path));
          // Corrupt the file for the next test, so we can verify it was
          // actually replaced.
          std::string corrupted_data("c0rrupt3d d4t4");
          EXPECT_TRUE(base::WriteFile(result.crx_cache_path, corrupted_data));
          EXPECT_FALSE(base::ContentsEqual(
              GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
              result.crx_cache_path));
          loop.Quit();
        }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    // Put replaces existing jebg to avoid error path.
    CrxCache::Options options(expected_crx_path.DirName());
    scoped_refptr<CrxCache> cache = base::MakeRefCounted<CrxCache>(options);
    // Duplicate the input file (again) since the old file was moved to the
    // cache.
    cache->Put(
        DuplicateTestFile(temp_dir.GetPath(),
                          "jebgalgnebhfojomionfpkfelancnnkf.crx"),
        id, fp,
        base::BindLambdaForTesting([&loop](const CrxCache::Result& result) {
          EXPECT_EQ(result.error, UnpackerError::kNone);
          EXPECT_TRUE(base::PathExists(result.crx_cache_path));
          EXPECT_TRUE(base::ContentsEqual(
              GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
              result.crx_cache_path));
          loop.Quit();
        }));
    loop.Run();
  }
  EXPECT_TRUE(base::DeletePathRecursively(temp_dir.GetPath()));
}

TEST_F(CrxCacheTest, CheckPutCachedCrxSucceeds) {
  std::string id = "rightid";
  std::string fp = "rightfp";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath expected_crx_path =
      BuildCrxFilePathForTest(temp_dir.GetPath(), id, fp);
  EXPECT_TRUE(base::CreateDirectory(expected_crx_path.DirName()));
  {
    base::File crx_file(expected_crx_path, base::File::FLAG_CREATE_ALWAYS |
                                               base::File::FLAG_WRITE |
                                               base::File::FLAG_READ);
  }
  CrxCache::Options options(expected_crx_path.DirName());
  scoped_refptr<CrxCache> cache = base::MakeRefCounted<CrxCache>(options);
  base::RunLoop loop;
  // The CRX is already in the cache so Put should succeed immediately.
  cache->Put(expected_crx_path, id, fp,
             base::BindLambdaForTesting(
                 [&loop, &expected_crx_path](const CrxCache::Result& result) {
                   EXPECT_EQ(result.error, UnpackerError::kNone);
                   base::FilePath crx_cache_path = result.crx_cache_path;
                   EXPECT_TRUE(base::DirectoryExists(crx_cache_path.DirName()));
                   EXPECT_TRUE(base::PathExists(crx_cache_path));
                   EXPECT_EQ(crx_cache_path, expected_crx_path);
                   loop.Quit();
                 }));
  loop.Run();
  EXPECT_TRUE(base::DeletePathRecursively(temp_dir.GetPath()));
}

}  // namespace update_client
