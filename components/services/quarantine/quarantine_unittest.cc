// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine.h"

#include <iterator>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace quarantine {

namespace {

const char kTestData[] = "It's okay to have a trailing nul.";
const char kInternetURL[] = "http://example.com/some-url";
const char kInternetReferrerURL[] = "http://example.com/some-other-url";
const char kTestGUID[] = "69f8621d-c46a-4e88-b915-1ce5415cb008";

void CheckQuarantineResult(QuarantineFileResult result,
                           QuarantineFileResult expected_result) {
  EXPECT_EQ(expected_result, result);
}

class QuarantineTest : public testing::Test {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    ASSERT_TRUE(com_initializer_.Succeeded());
#endif
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::WriteFile(GetTestFilePath(), {kTestData, std::size(kTestData)}));
  }

 protected:
  base::FilePath GetTestFilePath() {
    return test_dir_.GetPath().AppendASCII("foo.class");
  }

 private:
#if BUILDFLAG(IS_WIN)
  base::win::ScopedCOMInitializer com_initializer_;
#endif
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir test_dir_;
};

}  // namespace

TEST_F(QuarantineTest, FileCanBeOpenedForReadAfterAnnotation) {
  base::FilePath test_file = GetTestFilePath();
  QuarantineFile(
      test_file, GURL(kInternetURL), GURL(kInternetReferrerURL),
      /*request_initiator=*/std::nullopt, kTestGUID,
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_file, &contents));
  EXPECT_EQ(std::string(std::begin(kTestData), std::end(kTestData)), contents);
}

TEST_F(QuarantineTest, FileCanBeAnnotatedWithNoGUID) {
  QuarantineFile(
      GetTestFilePath(), GURL(kInternetURL), GURL(kInternetReferrerURL),
      /*request_initiator=*/std::nullopt, std::string(),
      base::BindOnce(&CheckQuarantineResult, QuarantineFileResult::OK));
  base::RunLoop().RunUntilIdle();
}

}  // namespace quarantine
