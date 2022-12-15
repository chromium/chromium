// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/single_file_tar_file_extractor.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/public/mojom/single_file_extractor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

using ::testing::StrictMock;

class SingleFileTarFileExtractorTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  SingleFileTarFileExtractorTest() = default;
  ~SingleFileTarFileExtractorTest() override = default;

  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("image_writer_private").AppendASCII(file_name);
  }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

  // BindNewPipeAndPassRemote() requires a sequenced context.
  base::test::TaskEnvironment task_environment_;

 private:
  base::ScopedTempDir temp_dir_;
};

// TODO(b/254591810): Make MockSingleFileExtractorListener a mock.
class MockSingleFileExtractorListener
    : public chrome::mojom::SingleFileExtractorListener {
  // chrome::mojom::SingleFileExtractorListener implementation.
  void OnProgress(uint64_t total_bytes, uint64_t progress_bytes) override {}
};

TEST_F(SingleFileTarFileExtractorTest, Extract) {
  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;

  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("test.tar"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(src_file.IsValid());

  base::FilePath out_path = temp_dir().AppendASCII("Extract_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  auto mock_listener = std::make_unique<MockSingleFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      mock_listener.get()};

  SingleFileTarFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  const chrome::file_util::mojom::ExtractionResult& result = future.Get();
  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kSuccess, result);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ("foo\n", contents);
}

TEST_F(SingleFileTarFileExtractorTest, ExtractTarLargerThanChunk) {
  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;

  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("test_large.tar"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(src_file.IsValid());

  base::FilePath out_path =
      temp_dir().AppendASCII("ExtractTarLargerThanChunk_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  auto mock_listener = std::make_unique<MockSingleFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      mock_listener.get()};

  SingleFileTarFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  const chrome::file_util::mojom::ExtractionResult& result = future.Get();
  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kSuccess, result);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ(10u * 1024u, contents.size());
}

TEST_F(SingleFileTarFileExtractorTest, ExtractNonExistentTar) {
  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;

  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("non_existent.tar"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_FALSE(src_file.IsValid());

  base::FilePath out_path =
      temp_dir().AppendASCII("ExtractNonExistentTar_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  auto mock_listener = std::make_unique<MockSingleFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      mock_listener.get()};

  SingleFileTarFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  const chrome::file_util::mojom::ExtractionResult& result = future.Get();
  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kGenericError, result);
}

TEST_F(SingleFileTarFileExtractorTest, ZeroByteFile) {
  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;

  // Use a tar.xz containing an empty file.
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("empty_file.tar"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(src_file.IsValid());

  base::FilePath out_path = temp_dir().AppendASCII("ZeroByteFile_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  auto mock_listener = std::make_unique<MockSingleFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      mock_listener.get()};

  SingleFileTarFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  const chrome::file_util::mojom::ExtractionResult& result = future.Get();
  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kSuccess, result);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  ASSERT_TRUE(contents.empty());
}

TEST_F(SingleFileTarFileExtractorTest, CorruptedFile) {
  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;

  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("test_corrupted.tar"));
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(src_file.IsValid());

  // test_corrupted.tar is a file that is cut off from the middle of the
  // archived file contents.
  base::FilePath out_path = temp_dir().AppendASCII("CorruptedFile_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  auto mock_listener = std::make_unique<MockSingleFileExtractorListener>();
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      mock_listener.get()};

  SingleFileTarFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  const chrome::file_util::mojom::ExtractionResult& result = future.Get();
  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kGenericError, result);
}

}  // namespace chrome
