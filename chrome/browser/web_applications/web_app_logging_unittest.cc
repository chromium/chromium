// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_logging.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {
namespace {

class WebAppLoggingTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    log_path_ = scoped_temp_dir_.GetPath().AppendASCII("Test.log");
    log_writing_task_runner_ =
        base::MakeRefCounted<base::TestSimpleTaskRunner>();
    log_deletion_task_runner_ =
        base::MakeRefCounted<base::TestSimpleTaskRunner>();
  }

 protected:
  base::FilePath log_path() { return log_path_; }

  void RunLoggingTasks() {
    log_writing_task_runner()->RunUntilIdle();
    log_deletion_task_runner()->RunUntilIdle();
    // Run any tasks in this environment, as replies occur too.
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  base::TestSimpleTaskRunner* log_writing_task_runner() {
    return log_writing_task_runner_.get();
  }

  base::TestSimpleTaskRunner* log_deletion_task_runner() {
    return log_deletion_task_runner_.get();
  }

  base::SimpleTestClock clock_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath log_path_;
  scoped_refptr<base::TestSimpleTaskRunner> log_writing_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> log_deletion_task_runner_;
};

TEST_F(WebAppLoggingTest, InMemoryModeDeletesLog) {
  base::WriteFile(log_path(), "test data");
  ASSERT_TRUE(base::PathExists(log_path()));

  auto log = PersistableLog::CreateForTesting(
      log_path(), PersistableLogMode::kInMemory, 10,
      base::MakeRefCounted<FileUtilsWrapper>(), log_writing_task_runner(),
      log_deletion_task_runner(), &clock_);

  RunLoggingTasks();
  ASSERT_FALSE(base::PathExists(log_path().DirName()));
}

TEST_F(WebAppLoggingTest, LogIsWrittenToDisk) {
  auto log = PersistableLog::CreateForTesting(
      log_path(), PersistableLogMode::kPersistToDisk, 10,
      base::MakeRefCounted<FileUtilsWrapper>(), log_writing_task_runner(),
      log_deletion_task_runner(), &clock_);
  RunLoggingTasks();

  clock_.SetNow(base::Time::FromMillisecondsSinceUnixEpoch(12345));
  log->Append(base::DictValue().Set("key", "value"));
  // The log should write on destruction.
  log.reset();
  RunLoggingTasks();

  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(log_path(), &file_contents));
  // Parse the JSON and check that the values are present.
  ASSERT_OK_AND_ASSIGN(base::Value log_value,
                       base::JSONReader::ReadAndReturnValueWithError(
                           file_contents, base::JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_THAT(
      log_value,
      base::test::IsJson(R"([{"timestamp_ms": 2147483647, "key": "value"}])"));
}

TEST_F(WebAppLoggingTest, LogRotation) {
  auto log = PersistableLog::CreateForTesting(
      log_path(), PersistableLogMode::kPersistToDisk, 10,
      base::MakeRefCounted<FileUtilsWrapper>(), log_writing_task_runner(),
      log_deletion_task_runner(), &clock_);
  RunLoggingTasks();

  base::Time time;
  ASSERT_TRUE(base::Time::FromString("2025-01-01 12:00:00 UTC", &time));
  clock_.SetNow(time);

  base::AutoReset<int> size_reset =
      PersistableLog::SetMaxLogFileSizeBytesForTesting(1);

  // Since the size is set to 1 byte, it should automatically schedule to write
  // the log immediately for each append.
  log->AppendValue(base::Value("value1"));
  RunLoggingTasks();
  // A second write is always needed, as we cannot rotate a log file that was
  // never written.
  log->AppendValue(base::Value("value2"));
  RunLoggingTasks();

  EXPECT_TRUE(base::PathExists(log_path()));
  base::FileEnumerator enumerator(log_path().DirName(), /*recursive=*/false,
                                  base::FileEnumerator::FILES);
  std::vector<base::FilePath> files;
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    files.push_back(name);
  }
  EXPECT_THAT(files, testing::UnorderedElementsAre(
                         log_path(),
                         log_path().InsertBeforeExtensionASCII("2025-01-01")));
}

TEST_F(WebAppLoggingTest, LogDownloadedIconsErrors) {
  const GURL kIconUrl("https://example.com/icon.png");
  IconsMap icons_map;
  DownloadedIconsHttpResults icons_http_results;
  icons_http_results.insert({{kIconUrl, gfx::Size(1, 1)}, net::HTTP_NOT_FOUND});
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  icons_map[kIconUrl].push_back(bitmap);

  // No error if bitmap is present.
  auto errors = LogDownloadedIconsErrors(IconsDownloadedResult::kCompleted,
                                         icons_map, icons_http_results);
  EXPECT_TRUE(errors.empty());

  // Error if bitmap is missing.
  icons_map.clear();
  errors = LogDownloadedIconsErrors(IconsDownloadedResult::kCompleted,
                                    icons_map, icons_http_results);
  EXPECT_FALSE(errors.empty());
  EXPECT_THAT(errors, base::test::IsJson(R"({
    "icons_downloaded_result": "Completed",
    "icons_http_results": [ {
      "http_code_desc": "Not Found",
      "http_status_code": 404,
      "icon_size": "1x1",
      "icon_url": "https://example.com/icon.png"
    } ]
  })"));
}

}  // namespace
}  // namespace web_app
