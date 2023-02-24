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
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace update_client {
namespace {

base::FilePath BuildCrxFilePathForTest(const base::FilePath& dir_path,
                                       const std::string& id,
                                       const std::string& fp) {
  return dir_path.AppendASCII("crx_cache_unittest_cache_dir")
      .AppendASCII(base::JoinString({id, fp}, "_"));
}

base::FilePath DuplicateTestFile(const char* file) {
  base::FilePath source_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path);
  source_path = source_path.AppendASCII("components")
                    .AppendASCII("test")
                    .AppendASCII("data")
                    .AppendASCII("update_client")
                    .AppendASCII(file);
  base::FilePath dest_path;
  base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &dest_path);
  dest_path = dest_path.AppendASCII("test_dir")
                  .AppendASCII("test")
                  .AddExtensionASCII("crx3");
  if (!base::PathExists(dest_path.DirName())) {
    base::CreateDirectory(dest_path.DirName());
  }
  EXPECT_TRUE(base::CopyFile(source_path, dest_path));
  return dest_path;
}

}  // namespace

class CrxCacheTest : public testing::Test {
 public:
  CrxCacheTest() = default;
  ~CrxCacheTest() override = default;

 protected:
  // TODO(crbug.com/1353588): We clearly have a TaskEnvironment member
  // (see env_ below) so why are we getting this warning?
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
      DuplicateTestFile("jebgalgnebhfojomionfpkfelancnnkf.crx"), id, fp,
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
      DuplicateTestFile("jebgalgnebhfojomionfpkfelancnnkf.crx"), id, fp,
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
  base::FilePath jebg_duplicate_path =
      DuplicateTestFile("jebgalgnebhfojomionfpkfelancnnkf.crx");
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
              DuplicateTestFile("jebgalgnebhfojomionfpkfelancnnkf.crx"),
              result.crx_cache_path));
          EXPECT_TRUE(base::DeleteFile(result.crx_cache_path));
          std::string corrupted_data("c0rrupt3d d4t4");
          EXPECT_TRUE(base::WriteFile(result.crx_cache_path, corrupted_data));
          EXPECT_FALSE(base::ContentsEqual(
              DuplicateTestFile("jebgalgnebhfojomionfpkfelancnnkf.crx"),
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
    cache->Put(
        jebg_duplicate_path, id, fp,
        base::BindLambdaForTesting([&loop](const CrxCache::Result& result) {
          EXPECT_EQ(result.error, UnpackerError::kNone);
          EXPECT_TRUE(base::PathExists(result.crx_cache_path));
          EXPECT_TRUE(base::ContentsEqual(
              DuplicateTestFile("jebgalgnebhfojomionfpkfelancnnkf.crx"),
              result.crx_cache_path));
          loop.Quit();
        }));
    loop.Run();
  }
  EXPECT_TRUE(base::DeletePathRecursively(temp_dir.GetPath()));
}

}  // namespace update_client
