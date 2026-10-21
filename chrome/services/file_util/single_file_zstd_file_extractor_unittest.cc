// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/single_file_zstd_file_extractor.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/public/mojom/single_file_extractor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSingleFileExtractorListener
    : public chrome::mojom::SingleFileExtractorListener {
 public:
  void OnProgress(uint64_t total_bytes, uint64_t progress_bytes) override {
    last_total_bytes_ = total_bytes;
    last_progress_bytes_ = progress_bytes;
    ++progress_call_count_;
  }

  uint64_t last_total_bytes() const { return last_total_bytes_; }
  uint64_t last_progress_bytes() const { return last_progress_bytes_; }
  int progress_call_count() const { return progress_call_count_; }

 private:
  uint64_t last_total_bytes_ = 0;
  uint64_t last_progress_bytes_ = 0;
  int progress_call_count_ = 0;
};

}  // namespace

class SingleFileZstdFileExtractorTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  SingleFileZstdFileExtractorTest() = default;
  ~SingleFileZstdFileExtractorTest() override = default;

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

  base::test::TaskEnvironment task_environment_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(SingleFileZstdFileExtractorTest, Extract) {
  const base::FilePath zstd_path = temp_dir().AppendASCII("test.zst");
  // RFC 8878 Zstandard frame magic number (0xFD2FB528 little-endian) + valid
  // single-segment raw block containing "foo\n".
  constexpr uint8_t kZstdTestFrame[] = {
      0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x04, 0x21, 0x00, 0x00, 'f', 'o', 'o', '\n',
  };
  ASSERT_TRUE(base::WriteFile(zstd_path, kZstdTestFrame));

  base::File src_file(zstd_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(src_file.IsValid());

  const base::FilePath out_path = temp_dir().AppendASCII("Extract_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  MockSingleFileExtractorListener mock_listener;
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      &mock_listener};

  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;
  SingleFileZstdFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kSuccess, future.Get());
  listener.FlushForTesting();
  EXPECT_GT(mock_listener.progress_call_count(), 0);
  EXPECT_EQ(sizeof(kZstdTestFrame), mock_listener.last_total_bytes());
  EXPECT_EQ(sizeof(kZstdTestFrame), mock_listener.last_progress_bytes());

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ("foo\n", contents);
}

TEST_F(SingleFileZstdFileExtractorTest, ExtractNonExistentZstd) {
  const base::FilePath path = temp_dir().AppendASCII("non_existent.zst");
  base::File src_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_FALSE(src_file.IsValid());

  const base::FilePath out_path =
      temp_dir().AppendASCII("ExtractNonExistentZstd_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  MockSingleFileExtractorListener mock_listener;
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      &mock_listener};

  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;
  SingleFileZstdFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kGenericError,
            future.Get());
}

TEST_F(SingleFileZstdFileExtractorTest, ZeroBytePayload) {
  const base::FilePath zstd_path = temp_dir().AppendASCII("empty.zst");
  // Valid Zstandard frame encoding a 0-byte uncompressed payload.
  constexpr uint8_t kEmptyZstdFrame[] = {
      0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x00, 0x01, 0x00, 0x00,
  };
  ASSERT_TRUE(base::WriteFile(zstd_path, kEmptyZstdFrame));

  base::File src_file(zstd_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(src_file.IsValid());

  const base::FilePath out_path =
      temp_dir().AppendASCII("ZeroBytePayload_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  MockSingleFileExtractorListener mock_listener;
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      &mock_listener};

  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;
  SingleFileZstdFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kSuccess, future.Get());
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_TRUE(contents.empty());
}

TEST_F(SingleFileZstdFileExtractorTest, CorruptedFile) {
  const base::FilePath corrupt_path = temp_dir().AppendASCII("corrupt.zst");
  // Valid magic header followed by truncated/corrupt frame data.
  constexpr uint8_t kCorruptZstdFrame[] = {0x28, 0xb5, 0x2f, 0xfd, 0xff};
  ASSERT_TRUE(base::WriteFile(corrupt_path, kCorruptZstdFrame));

  base::File src_file(corrupt_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(src_file.IsValid());

  const base::FilePath out_path =
      temp_dir().AppendASCII("CorruptedFile_dst_file");
  base::File dst_file(out_path,
                      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(dst_file.IsValid());

  MockSingleFileExtractorListener mock_listener;
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener{
      &mock_listener};

  base::test::TestFuture<chrome::file_util::mojom::ExtractionResult> future;
  SingleFileZstdFileExtractor extractor;
  extractor.Extract(std::move(src_file), std::move(dst_file),
                    listener.BindNewPipeAndPassRemote(), future.GetCallback());

  EXPECT_EQ(chrome::file_util::mojom::ExtractionResult::kGenericError,
            future.Get());
}
