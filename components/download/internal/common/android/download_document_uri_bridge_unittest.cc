// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/android/download_document_uri_bridge.h"

#include <string>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/android/content_uri_test_utils.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

class DownloadDocumentUriBridgeTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::ScopedTempDir temp_dir_;
};

TEST_F(DownloadDocumentUriBridgeTest, IsDocumentUri) {
  base::FilePath local_file = temp_dir_.GetPath().Append("test.txt");
  ASSERT_TRUE(base::WriteFile(local_file, ""));
  EXPECT_FALSE(DownloadDocumentUriBridge::IsDocumentUri(local_file));

  auto doc_uri =
      base::test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(
          local_file);
  ASSERT_TRUE(doc_uri.has_value());
  EXPECT_TRUE(DownloadDocumentUriBridge::IsDocumentUri(*doc_uri));
}

TEST_F(DownloadDocumentUriBridgeTest, MoveFileToDocumentUriSuccess) {
  base::FilePath source_file = temp_dir_.GetPath().Append("source.txt");
  const std::string content = "test content for document uri";
  ASSERT_TRUE(base::WriteFile(source_file, content));

  base::FilePath dest_file = temp_dir_.GetPath().Append("dest.txt");
  ASSERT_TRUE(base::WriteFile(dest_file, ""));
  auto dest_uri =
      base::test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(
          dest_file);
  ASSERT_TRUE(dest_uri.has_value());

  EXPECT_EQ(
      DOWNLOAD_INTERRUPT_REASON_NONE,
      DownloadDocumentUriBridge::MoveFileToDocumentUri(source_file, *dest_uri));
  // Source file should be deleted on success.
  EXPECT_FALSE(base::PathExists(source_file));

  // Destination should have new content.
  std::string read_content;
  EXPECT_TRUE(base::ReadFileToString(*dest_uri, &read_content));
  EXPECT_EQ(content, read_content);
}

TEST_F(DownloadDocumentUriBridgeTest, MoveFileToDocumentUriNonExistentSource) {
  base::FilePath source_file = temp_dir_.GetPath().Append("non_existent.txt");
  base::FilePath dest_file = temp_dir_.GetPath().Append("dest.txt");
  ASSERT_TRUE(base::WriteFile(dest_file, ""));
  auto dest_uri =
      base::test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(
          dest_file);
  ASSERT_TRUE(dest_uri.has_value());

  EXPECT_EQ(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
      DownloadDocumentUriBridge::MoveFileToDocumentUri(source_file, *dest_uri));
}

TEST_F(DownloadDocumentUriBridgeTest,
       MoveFileToDocumentUriUnwritableDestination) {
  base::FilePath source_file = temp_dir_.GetPath().Append("source.txt");
  ASSERT_TRUE(base::WriteFile(source_file, "source content"));
  base::FilePath unwritable_uri("content://org.chromium.invalid.authority/doc");

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
            DownloadDocumentUriBridge::MoveFileToDocumentUri(source_file,
                                                             unwritable_uri));
  // Source file should NOT be deleted if moving failed.
  EXPECT_TRUE(base::PathExists(source_file));
}

TEST_F(DownloadDocumentUriBridgeTest, OpenDocumentUri) {
  base::FilePath target_file = temp_dir_.GetPath().Append("target.txt");
  ASSERT_TRUE(base::WriteFile(target_file, ""));
  auto doc_uri =
      base::test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(
          target_file);
  ASSERT_TRUE(doc_uri.has_value());

  base::File file = DownloadDocumentUriBridge::OpenDocumentUri(*doc_uri);
  EXPECT_TRUE(file.IsValid());

  const std::string text = "open_document_test";
  auto bytes_written = file.Write(0, base::as_byte_span(text));
  EXPECT_EQ(text.size(), bytes_written.value_or(0));
  file.Close();

  std::string read_content;
  EXPECT_TRUE(base::ReadFileToString(*doc_uri, &read_content));
  EXPECT_EQ(text, read_content);
}

TEST_F(DownloadDocumentUriBridgeTest, DeleteDocumentUri) {
  base::FilePath target_file = temp_dir_.GetPath().Append("delete_me.txt");
  ASSERT_TRUE(base::WriteFile(target_file, "to be deleted"));
  auto doc_uri =
      base::test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(
          target_file);
  ASSERT_TRUE(doc_uri.has_value());
  EXPECT_TRUE(base::PathExists(*doc_uri));

  DownloadDocumentUriBridge::DeleteDocumentUri(*doc_uri);
  EXPECT_FALSE(base::PathExists(*doc_uri));
}

TEST_F(DownloadDocumentUriBridgeTest, PublishDownload) {
  base::FilePath target_file = temp_dir_.GetPath().Append("publish.txt");
  ASSERT_TRUE(base::WriteFile(target_file, ""));
  auto doc_uri =
      base::test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(
          target_file);
  ASSERT_TRUE(doc_uri.has_value());

  // PublishDownload is a no-op returning the same URI.
  EXPECT_EQ(*doc_uri, DownloadDocumentUriBridge::PublishDownload(*doc_uri));
}

TEST_F(DownloadDocumentUriBridgeTest, GetDisplayName) {
  base::FilePath target_file = temp_dir_.GetPath().Append("my_document.pdf");
  ASSERT_TRUE(base::WriteFile(target_file, ""));
  auto doc_uri =
      base::test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(
          target_file);
  ASSERT_TRUE(doc_uri.has_value());

  base::FilePath display_name =
      DownloadDocumentUriBridge::GetDisplayName(*doc_uri);
  EXPECT_EQ(base::FilePath("my_document.pdf"), display_name);

  // Query failure / inaccessible document URI returns an empty FilePath.
  base::FilePath non_existent_uri(
      "content://org.chromium.invalid.authority/123");
  EXPECT_TRUE(
      DownloadDocumentUriBridge::GetDisplayName(non_existent_uri).empty());
}

TEST_F(DownloadDocumentUriBridgeTest, RenameDocumentUriFailure) {
  base::FilePath non_existent_uri(
      "content://org.chromium.invalid.authority/123");
  EXPECT_FALSE(DownloadDocumentUriBridge::RenameDocumentUri(
      non_existent_uri, base::FilePath("new_name.pdf")));
}

}  // namespace download
