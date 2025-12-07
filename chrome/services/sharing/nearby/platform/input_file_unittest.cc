// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/input_file.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace nearby::chrome {

namespace {

// 64KB, the common upper chunk size throughout Nearby Connections.
constexpr int64_t kChunkSize = 1024 * 64;

// 1MB.
constexpr int kTestDataSize = 1024 * 1000;

}  // namespace

class InputFileTest : public PlatformTest {
 public:
  InputFileTest() = default;
  ~InputFileTest() override = default;
  InputFileTest(const InputFileTest&) = delete;
  InputFileTest& operator=(const InputFileTest&) = delete;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    CreateValidInputFile(kTestDataSize);
  }

  void CreateValidInputFile(int file_size) {
    if (file_size < 0) {
      input_file_ = std::make_unique<InputFile>(base::File());
      return;
    }

    expected_data_ = std::string(file_size, 'a');

    base::FilePath file_path =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("InputFileTest"));

    ASSERT_TRUE(WriteFile(file_path, expected_data_));

    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    ASSERT_EQ(file.GetLength(), file_size);

    input_file_ = std::make_unique<InputFile>(std::move(file));
  }

  void CreateInvalidInputFile() {
    input_file_ = std::make_unique<InputFile>(base::File());
  }

  void VerifyRead(int64_t chunk_size) {
    std::string file_data;
    while (true) {
      auto exception_or_byte_array = input_file_->Read(chunk_size);
      ASSERT_TRUE(exception_or_byte_array.ok());

      auto byte_array = exception_or_byte_array.result();
      if (byte_array.Empty())
        break;

      file_data.append(byte_array.AsStringView());
    }

    EXPECT_EQ(expected_data_, file_data);
  }

  std::string expected_data_;
  std::unique_ptr<InputFile> input_file_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(InputFileTest, TestGetTotalSize_Valid) {
  EXPECT_EQ(kTestDataSize, input_file_->GetTotalSize());
}

TEST_F(InputFileTest, TestGetTotalSize_Invalid) {
  CreateInvalidInputFile();
  EXPECT_EQ(-1, input_file_->GetTotalSize());
}

TEST_F(InputFileTest, TestRead_Valid) {
  VerifyRead(kChunkSize);
}

// TODO(crbug.com/40148372): Fix these tests from crashing on Windows.
#if !BUILDFLAG(IS_WIN)
TEST_F(InputFileTest, TestRead_Valid_ChunkLargerThanFileSize) {
  VerifyRead(kTestDataSize * 2);
}

TEST_F(InputFileTest, TestRead_Valid_LargeFileSize) {
  // 100MB
  CreateValidInputFile(kTestDataSize * 100);
  VerifyRead(kChunkSize);
}
#endif  // !BUILDFLAG(IS_WIN)

TEST_F(InputFileTest, TestRead_Invalid) {
  CreateInvalidInputFile();
  EXPECT_FALSE(input_file_->Read(kChunkSize).ok());
}

TEST_F(InputFileTest, TestClose_Valid) {
  EXPECT_TRUE(input_file_->Close().Ok());
}

TEST_F(InputFileTest, TestClose_Invalid) {
  CreateInvalidInputFile();
  EXPECT_FALSE(input_file_->Close().Ok());
}

TEST_F(InputFileTest, TestExtractUnderlyingFile_Valid) {
  base::File file = input_file_->ExtractUnderlyingFile();
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(kTestDataSize, file.GetLength());
}

TEST_F(InputFileTest, TestExtractUnderlyingFile_Invalid) {
  CreateInvalidInputFile();
  base::File file = input_file_->ExtractUnderlyingFile();
  EXPECT_FALSE(file.IsValid());
}

}  // namespace nearby::chrome
