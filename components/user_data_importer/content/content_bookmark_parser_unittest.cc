// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/content/content_bookmark_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/user_data_importer/content/fake_bookmark_html_parser.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_data_importer {

class ContentBookmarkParserTest : public testing::Test {
 public:
  ContentBookmarkParserTest() = default;
  ~ContentBookmarkParserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    parser_ = std::make_unique<ContentBookmarkParser>();
    test_parser_ = std::make_unique<FakeBookmarkHtmlParser>();
    mojo::PendingRemote<mojom::BookmarkHtmlParser> remote;
    receiver_ = std::make_unique<mojo::Receiver<mojom::BookmarkHtmlParser>>(
        test_parser_.get(), remote.InitWithNewPipeAndPassReceiver());
    parser_->SetServiceForTesting(std::move(remote));
  }

  void TearDown() override {
    // Reset the parser and receiver to allow the test parser to be deleted.
    parser_.reset();
    receiver_.reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ContentBookmarkParser> parser_;
  std::unique_ptr<FakeBookmarkHtmlParser> test_parser_;
  std::unique_ptr<mojo::Receiver<mojom::BookmarkHtmlParser>> receiver_;
};

TEST_F(ContentBookmarkParserTest, ParseFile) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("bookmarks.html");
  std::string file_content = "<html><body>...</body></html>";
  ASSERT_TRUE(base::WriteFile(file_path, file_content));

  base::test::TestFuture<BookmarkParser::BookmarkParsingResult> future;
  parser_->Parse(file_path, future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
}

TEST_F(ContentBookmarkParserTest, ParseBaseFile) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("bookmarks.html");
  std::string file_content = "<html><body>...</body></html>";
  ASSERT_TRUE(base::WriteFile(file_path, file_content));

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());

  base::test::TestFuture<BookmarkParser::BookmarkParsingResult> future;
  parser_->Parse(std::move(file), future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
}

TEST_F(ContentBookmarkParserTest, ParseNonExistentFile) {
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("non_existent.html");
  base::test::TestFuture<BookmarkParser::BookmarkParsingResult> future;
  parser_->Parse(file_path, future.GetCallback());
  const auto& result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(BookmarkParser::BookmarkParsingError::kFailedToReadFile,
            result.error());
}

TEST_F(ContentBookmarkParserTest, ParseEmptyFile) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("bookmarks.html");
  std::string file_content;
  ASSERT_TRUE(base::WriteFile(file_path, file_content));
  base::test::TestFuture<BookmarkParser::BookmarkParsingResult> future;
  parser_->Parse(file_path, future.GetCallback());
  const auto& result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(BookmarkParser::BookmarkParsingError::kFailedToReadFile,
            result.error());
}

TEST_F(ContentBookmarkParserTest, ParseInvalidBaseFile) {
  base::File file;
  ASSERT_FALSE(file.IsValid());

  base::test::TestFuture<BookmarkParser::BookmarkParsingResult> future;
  parser_->Parse(std::move(file), future.GetCallback());
  const auto& result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(BookmarkParser::BookmarkParsingError::kFailedToReadFile,
            result.error());
}

}  // namespace user_data_importer
