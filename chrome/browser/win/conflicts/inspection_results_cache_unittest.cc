// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/inspection_results_cache.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

ModuleInspectionResult CreateTestModuleInspectionResult() {
  ModuleInspectionResult inspection_result;

  inspection_result.location = u"location";
  inspection_result.basename = u"basename";
  inspection_result.product_name = u"product_name";
  inspection_result.description = u"description";
  inspection_result.version = u"version";
  inspection_result.certificate_info.type =
      CertificateInfo::Type::CERTIFICATE_IN_FILE;
  inspection_result.certificate_info.path =
      base::FilePath(L"certificate_info_path");
  inspection_result.certificate_info.subject = u"certificate_info_subject";

  return inspection_result;
}

bool InspectionResultsEqual(const ModuleInspectionResult& lhs,
                            const ModuleInspectionResult& rhs) {
  return std::tie(lhs.location, lhs.basename, lhs.product_name, lhs.description,
                  lhs.version, lhs.certificate_info.type,
                  lhs.certificate_info.path, lhs.certificate_info.subject) ==
         std::tie(rhs.location, rhs.basename, rhs.product_name, rhs.description,
                  rhs.version, rhs.certificate_info.type,
                  rhs.certificate_info.path, rhs.certificate_info.subject);
}

}  // namespace

class InspectionResultsCacheTest : public testing::Test {
 public:
  InspectionResultsCacheTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  InspectionResultsCacheTest(const InspectionResultsCacheTest&) = delete;
  InspectionResultsCacheTest& operator=(const InspectionResultsCacheTest&) =
      delete;

  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::FilePath GetCacheFilePath() {
    return scoped_temp_dir_.GetPath().Append(L"cache.bin");
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(InspectionResultsCacheTest, ReadMissingCache) {
  InspectionResultsCache read_inspection_results_cache;
  EXPECT_EQ(ReadCacheResult::kFailReadFile,
            ReadInspectionResultsCache(GetCacheFilePath(), 0,
                                       &read_inspection_results_cache));
}

TEST_F(InspectionResultsCacheTest, WriteAndRead) {
  ModuleInfoKey module_key(base::FilePath(L"file_path.exe"), 0x1234, 0xABCD);
  ModuleInspectionResult inspection_result = CreateTestModuleInspectionResult();

  InspectionResultsCache inspection_results_cache;
  AddInspectionResultToCache(module_key, inspection_result,
                             &inspection_results_cache);

  EXPECT_TRUE(WriteInspectionResultsCache(GetCacheFilePath(),
                                          inspection_results_cache));

  // Now check that a cache read from the file is identical to the cache that
  // was written.
  InspectionResultsCache read_inspection_results_cache;
  EXPECT_EQ(ReadCacheResult::kSuccess,
            ReadInspectionResultsCache(GetCacheFilePath(), 0,
                                       &read_inspection_results_cache));

  auto read_inspection_result =
      GetInspectionResultFromCache(module_key, &read_inspection_results_cache);
  ASSERT_TRUE(read_inspection_result);
  EXPECT_TRUE(
      InspectionResultsEqual(inspection_result, *read_inspection_result));
}

TEST_F(InspectionResultsCacheTest, WriteAndReadExpired) {
  ModuleInfoKey module_key(base::FilePath(L"file_path.exe"), 0x1234, 0xABCD);
  ModuleInspectionResult inspection_result = CreateTestModuleInspectionResult();

  InspectionResultsCache inspection_results_cache;
  AddInspectionResultToCache(module_key, inspection_result,
                             &inspection_results_cache);

  EXPECT_TRUE(WriteInspectionResultsCache(GetCacheFilePath(),
                                          inspection_results_cache));

  // Now read the cache from disk with a minimum time stamp higher than
  // base::Time::Now() and it should be empty because the only element it
  // contains is expired.
  InspectionResultsCache read_inspection_results_cache;
  EXPECT_EQ(ReadCacheResult::kSuccess,
            ReadInspectionResultsCache(
                GetCacheFilePath(), CalculateTimeStamp(base::Time::Now()) + 1,
                &read_inspection_results_cache));

  EXPECT_TRUE(read_inspection_results_cache.empty());
  auto read_inspection_result =
      GetInspectionResultFromCache(module_key, &read_inspection_results_cache);
  EXPECT_FALSE(read_inspection_result);
}
