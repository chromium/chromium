// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/trash_service/public/cpp/trash_service.h"

#include <limits.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom.h"
#include "chromeos/ash/components/trash_service/trash_service_impl.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::trash_service {

using ::base::test::RunClosure;
using ::testing::_;

namespace {

// A struct with default data used by tests to enable testing invalid fields
// with all remaining data being valid.
struct TrashInfoContents {
  std::string header = "[Trash Info]";
  std::string path_line = "Path=/foo/bar.txt";
  std::string date_line = "DeletionDate=2022-07-18T10:13:00.000Z";

  base::FilePath restore_path{"/foo/bar.txt"};

  std::string ToString() const {
    return base::StrCat({header, "\n", path_line, "\n", date_line});
  }

  base::Time GetDeletionDate() const {
    std::vector<std::string_view> key_value =
        base::SplitStringPiece(std::string_view(date_line), "=",
                               base::WhitespaceHandling::TRIM_WHITESPACE,
                               base::SplitResult::SPLIT_WANT_ALL);
    EXPECT_EQ(2UL, key_value.size());
    base::Time time;
    EXPECT_TRUE(base::Time::FromUTCString(key_value[1].data(), &time));
    return time;
  }
};

}  // namespace

class TrashServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_dir_ = temp_dir_.GetPath();

    trash_impl_ = std::make_unique<TrashServiceImpl>(
        trash_service_remote_.BindNewPipeAndPassReceiver());
  }

  void ExpectParsingFailedForFileContents(const std::string& file_contents) {
    const base::FilePath file_path = test_dir_.Append("foo.txt.trashinfo");
    ASSERT_TRUE(base::WriteFile(file_path, file_contents));
    base::File trash_info_file(file_path,
                               base::File::FLAG_OPEN | base::File::FLAG_READ);

    base::MockCallback<ParseTrashInfoCallback> complete_callback;
    base::RunLoop run_loop;
    EXPECT_CALL(complete_callback, Run(base::File::FILE_ERROR_FAILED,
                                       base::FilePath(), base::Time()))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    trash_impl_->ParseTrashInfoFile(std::move(trash_info_file),
                                    complete_callback.Get());
    run_loop.Run();
  }

  void ExpectParsingSucceedsForFileContents(
      const TrashInfoContents& file_contents) {
    const base::FilePath file_path = test_dir_.Append("foo.txt.trashinfo");
    ASSERT_TRUE(base::WriteFile(file_path, file_contents.ToString()));
    base::File trash_info_file(file_path,
                               base::File::FLAG_OPEN | base::File::FLAG_READ);

    base::MockCallback<ParseTrashInfoCallback> complete_callback;
    base::RunLoop run_loop;
    EXPECT_CALL(complete_callback,
                Run(base::File::FILE_OK, file_contents.restore_path,
                    file_contents.GetDeletionDate()))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    trash_impl_->ParseTrashInfoFile(std::move(trash_info_file),
                                    complete_callback.Get());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  base::FilePath test_dir_;

  std::unique_ptr<TrashServiceImpl> trash_impl_;
  mojo::Remote<mojom::TrashService> trash_service_remote_;
};

TEST_F(TrashServiceTest, NonexistingFileShouldReturnNotFound) {
  const base::FilePath file_path_does_not_exist =
      test_dir_.Append("foo.txt.trashinfo");
  base::File trash_info_file(file_path_does_not_exist,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);

  base::MockCallback<ParseTrashInfoCallback> complete_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(complete_callback, Run(base::File::FILE_ERROR_NOT_FOUND, _, _))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  trash_impl_->ParseTrashInfoFile(std::move(trash_info_file),
                                  complete_callback.Get());
  run_loop.Run();
}

TEST_F(TrashServiceTest, PathExceedingMaxAllowableLengthShouldFail) {
  // Create a valid path that exceeds `PATH_MAX`.
  TrashInfoContents file_contents;
  file_contents.restore_path =
      base::FilePath("/foo").Append(std::string(PATH_MAX, 'f')).Append("foo");
  file_contents.path_line =
      base::StrCat({"Path=", file_contents.restore_path.value()});

  // Setup the test file as a well-formed file but with a path that will cause
  // the read buffer to go over.
  ExpectParsingFailedForFileContents(file_contents.ToString());
}

TEST_F(TrashServiceTest, ValidFileWithExtraDataIgnoresOverflow) {
  TrashInfoContents contents;
  const base::FilePath file_path = test_dir_.Append("foo.txt.trashinfo");

  // Append 1024 random bytes to the end of the trashinfo file, this data
  // should be ignored when parsing.
  ASSERT_TRUE(base::WriteFile(file_path,
                              base::StrCat({contents.ToString(), "\n",
                                            base::RandBytesAsString(1024)})));
  base::File trash_info_file(file_path,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);

  base::MockCallback<ParseTrashInfoCallback> complete_callback;
  base::RunLoop run_loop;
  EXPECT_CALL(complete_callback, Run(base::File::FILE_OK, contents.restore_path,
                                     contents.GetDeletionDate()))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  trash_impl_->ParseTrashInfoFile(std::move(trash_info_file),
                                  complete_callback.Get());
  run_loop.Run();
}

TEST_F(TrashServiceTest, InvalidTrashInfoHeaderScenarios) {
  // Invalid header.
  {
    TrashInfoContents contents;
    contents.header = "invalid header";
    ExpectParsingFailedForFileContents(contents.ToString());
  }

  // Valid structure but misspelled.
  {
    TrashInfoContents contents;
    contents.header = "[Trash Imfo]";
    ExpectParsingFailedForFileContents(contents.ToString());
  }

  // First line is the "Path=" key value pair.
  {
    TrashInfoContents contents;
    std::string file_contents =
        base::StrCat({contents.path_line, "\n", contents.date_line});
    ExpectParsingFailedForFileContents(file_contents);
  }
}

TEST_F(TrashServiceTest, InvalidPathKeyValueScenarios) {
  // No path key value pair.
  {
    TrashInfoContents contents;
    std::string file_contents =
        base::StrCat({contents.header, "\n", contents.date_line});
    ExpectParsingFailedForFileContents(file_contents);
  }

  // Create a too-long path where each component is valid.
  std::string long_path;
  for (int i = 0; long_path.size() < PATH_MAX; ++i) {
    long_path += '/';
    long_path.append(200, static_cast<char>('a' + i));
  }
  long_path.resize(PATH_MAX);

  const std::vector<std::string> lines = {{
      "/foo/bar",                // Missing "Path=" key.
      "Patn=/foo/bar",           // Misspelled "Path=" key.
      "Path=/foo/../bar",        // Path references parent.
      "Path=/foo/%2e%2E/bar",    // Path references parent in a sneaky way.
      "Path=/foo/./bar",         // Path references current dir.
      "Path=/foo/%2e/bar",       // Path references current dir in a sneaky way.
      "Path=relative/path.txt",  // Relative file path.
      "Path=",                   // Empty path.
      "Path=bar"                 // Relative folder path.
      "Path=foo/bar"             // Relative path.
      "Path=/",                  // Root path.
      "Path=%2f",                // Root path in a sneaky way.
      "Path=/////",              // Root path.
      "Path=//server/foo/bar",   // UNC-style path.
      "Path=/foo%00/bar",        // Embedded NUL byte.
      "Path=/foo%ff/bar",        // Non UTF-8.
      base::StrCat(
          {"Path=/", std::string(NAME_MAX + 1, 'x')}),  // Long component.
      base::StrCat({"Path=", long_path}),               // Long path.
  }};

  for (const std::string& line : lines) {
    TrashInfoContents contents;
    contents.path_line = line;
    ExpectParsingFailedForFileContents(contents.ToString());
  }
}

TEST_F(TrashServiceTest, InvalidDeletionDateKeyValueScenarios) {
  // No deletion date key value pair.
  {
    TrashInfoContents contents;
    std::string file_contents =
        base::StrCat({contents.header, "\n", contents.path_line});
    ExpectParsingFailedForFileContents(file_contents);
  }

  const std::vector<std::string> kInvalidDeletionDates = {{
      "2022-07-18T10:13:00.000Z",              // Missing "DeletionDate=" key.
      "DeletedDate=2022-07-18T10:13:00.000Z",  // Misspelled "DeletionDate="
                                               // key.
      "DeletionDate=2022-07-1810:13:00.000Z",  // Not the required size (missing
                                               // the T character for time).
      "DeletionDate=abcdefghijklmnopqrstuvw",  // Same ISO-8601 size but invalid
                                               // date.
  }};
  for (const auto& date : kInvalidDeletionDates) {
    TrashInfoContents contents;
    contents.date_line = date;
    ExpectParsingFailedForFileContents(contents.ToString());
  }
}

TEST_F(TrashServiceTest, ValidPathKeyValueScenarios) {
  TrashInfoContents contents;

  const std::vector<std::string> kValidPaths = {{
      "Path=/foo/bar.txt",      // Happy path.
      "   Path=/foo/bar.txt",   // Leading whitespace is ignored.
      "Path=/foo/bar.txt    ",  // Trailing whitespace is ignored.
  }};
  for (const auto& path : kValidPaths) {
    contents.path_line = path;
    ExpectParsingSucceedsForFileContents(contents);
  }

  contents.path_line = "Path=/%09new%0aline%25%20";
  contents.restore_path = base::FilePath("/\tnew\nline% ");
  ExpectParsingSucceedsForFileContents(contents);
}

TEST_F(TrashServiceTest, ValidDeletionDateKeyValueScenarios) {
  const std::vector<std::string> kValidDeletionDates = {{
      "DeletionDate=2022-07-18T10:13:00.000Z",      // Happy path.
      "   DeletionDate=2022-07-18T10:13:00.000Z",   // Leading whitespace is
                                                    // ignored.
      "DeletionDate=2022-07-18T10:13:00.000Z    ",  // Trailing whitespace is
                                                    // ignored.
  }};
  for (const auto& date : kValidDeletionDates) {
    TrashInfoContents contents;
    contents.date_line = date;
    ExpectParsingSucceedsForFileContents(contents);
  }
}

}  // namespace ash::trash_service
