// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/read_stream.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/utility/safe_browsing/mac/dmg_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace dmg {

using ::testing::ElementsAre;

struct MemoryReadStreamTest {
  void SetUp() {}

  const char* TestName() {
    return "MemoryReadStream";
  }

  std::vector<uint8_t> data;
};

struct FileReadStreamTest {
  void SetUp() {
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  }

  const char* TestName() {
    return "FileReadStream";
  }

  base::ScopedTempDir temp_dir;
  base::File file;
};

template <typename T>
class ReadStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    test_helper_.SetUp();
  }

  void TearDown() override {
    if (HasFailure())
      ADD_FAILURE() << "Failing type is " << test_helper_.TestName();
  }

  std::unique_ptr<ReadStream> CreateStream(size_t data_size);

 private:
  T test_helper_;
};

template <>
std::unique_ptr<ReadStream> ReadStreamTest<MemoryReadStreamTest>::CreateStream(
    size_t data_size) {
  test_helper_.data.resize(data_size);
  for (size_t i = 0; i < data_size; ++i) {
    test_helper_.data[i] = i % 255;
  }
  return std::make_unique<MemoryReadStream>(test_helper_.data);
}

template <>
std::unique_ptr<ReadStream> ReadStreamTest<FileReadStreamTest>::CreateStream(
    size_t data_size) {
  base::FilePath path = test_helper_.temp_dir.GetPath().Append("stream");
  test_helper_.file.Initialize(path,
      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!test_helper_.file.IsValid()) {
    ADD_FAILURE() << "Failed to create temp file";
    return nullptr;
  }

  for (size_t i = 0; i < data_size; ++i) {
    char value = i % 255;
    EXPECT_TRUE(test_helper_.file.WriteAtCurrentPosAndCheck(
        base::byte_span_from_ref(value)));
  }

  test_helper_.file.Close();

  test_helper_.file.Initialize(path,
      base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!test_helper_.file.IsValid()) {
    ADD_FAILURE() << "Failed to open temp file";
    return nullptr;
  }

  return base::WrapUnique(
      new FileReadStream(test_helper_.file.GetPlatformFile()));
}

using ReadStreamImpls = testing::Types<MemoryReadStreamTest,
                                       FileReadStreamTest>;
TYPED_TEST_SUITE(ReadStreamTest, ReadStreamImpls);

TYPED_TEST(ReadStreamTest, Read) {
  std::unique_ptr<ReadStream> stream =
      ReadStreamTest<TypeParam>::CreateStream(128);
  uint8_t buf[128] = {0};
  size_t bytes_read;

  {
    EXPECT_TRUE(stream->Read(base::span(buf).first(4u), &bytes_read));
    EXPECT_EQ(4u, bytes_read);
    uint8_t expected[] = { 0, 1, 2, 3, 0, 0, 0 };
    EXPECT_EQ(0, memcmp(expected, buf, sizeof(expected)));
  }

  {
    EXPECT_TRUE(stream->Read(base::span(buf).first(9u), &bytes_read));
    EXPECT_EQ(9u, bytes_read);
    uint8_t expected[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 0 };
    EXPECT_EQ(0, memcmp(expected, buf, sizeof(expected)));
  }
}

TYPED_TEST(ReadStreamTest, CopyStreamToFileTest) {
  constexpr size_t kStreamSize = 4242;
  std::unique_ptr<ReadStream> stream =
      ReadStreamTest<TypeParam>::CreateStream(kStreamSize);
  base::FilePath temp_path;
  base::File temp_file;
  base::CreateTemporaryFile(&temp_path);
  temp_file.Initialize(
      temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                  base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                  base::File::FLAG_DELETE_ON_CLOSE));
  EXPECT_TRUE(CopyStreamToFile(*stream, temp_file));
  EXPECT_EQ(kStreamSize, static_cast<size_t>(temp_file.GetLength()));
  std::array<uint8_t, kStreamSize> file_buf;
  std::array<uint8_t, 15> expected = {0, 1, 2,  3,  4,  5,  6, 7,
                                      8, 9, 10, 11, 12, 13, 14};
  temp_file.ReadAndCheck(0, file_buf);
  // Range is set to 1020 - 1035 to check the copying loop point of
  // the CopyStreamToFile function which is 1024.
  for (int i = 1020; i < 1035; i++) {
    EXPECT_EQ(expected[i - 1020], file_buf[i]);
  }
}

TYPED_TEST(ReadStreamTest, ReadAll) {
  const size_t kStreamSize = 4242;
  std::unique_ptr<ReadStream> stream =
      ReadStreamTest<TypeParam>::CreateStream(kStreamSize);

  auto maybe_data = ReadEntireStream(*stream);
  ASSERT_TRUE(maybe_data.has_value());
  EXPECT_EQ(kStreamSize, maybe_data->size());
}

TYPED_TEST(ReadStreamTest, SeekSet) {
  std::unique_ptr<ReadStream> stream =
      ReadStreamTest<TypeParam>::CreateStream(255);
  uint8_t buf[32] = {0};
  size_t bytes_read;

  {
    EXPECT_EQ(250, stream->Seek(250, SEEK_SET));
    EXPECT_TRUE(stream->Read(buf, &bytes_read));
    EXPECT_EQ(5u, bytes_read);
    uint8_t expected[] = { 250, 251, 252, 253, 254, 0, 0 };
    EXPECT_EQ(0, memcmp(expected, buf, sizeof(expected)));
  }

  {
    EXPECT_EQ(5, stream->Seek(5, SEEK_SET));
    EXPECT_TRUE(stream->Read(base::span(buf).first(3u), &bytes_read));
    EXPECT_EQ(3u, bytes_read);
    uint8_t expected[] = { 5, 6, 7, 253, 254, 0, 0 };
    EXPECT_EQ(0, memcmp(expected, buf, sizeof(expected)));
  }
}

TYPED_TEST(ReadStreamTest, SeekEnd) {
  std::unique_ptr<ReadStream> stream =
      ReadStreamTest<TypeParam>::CreateStream(32);
  uint8_t buf[32] = {0};
  size_t bytes_read;

  {
    EXPECT_EQ(32, stream->Seek(0, SEEK_END));
    EXPECT_TRUE(stream->Read(buf, &bytes_read));
    EXPECT_EQ(0u, bytes_read);
  }

  {
    EXPECT_EQ(28, stream->Seek(-4, SEEK_END));
    EXPECT_TRUE(stream->Read(buf, &bytes_read));
    EXPECT_EQ(4u, bytes_read);
    uint8_t expected[] = { 28, 29, 30, 31, 0, 0, 0 };
    EXPECT_EQ(0, memcmp(expected, buf, sizeof(expected)));
  }
}

TYPED_TEST(ReadStreamTest, SeekCur) {
  std::unique_ptr<ReadStream> stream =
      ReadStreamTest<TypeParam>::CreateStream(100);
  std::array<uint8_t, 32> buf;
  size_t bytes_read;

  {
    EXPECT_EQ(0, stream->Seek(0, SEEK_CUR));
  }

  {
    EXPECT_TRUE(stream->Read(buf, &bytes_read));
    EXPECT_EQ(buf.size(), bytes_read);
    for (size_t i = 0; i < buf.size(); ++i) {
      EXPECT_EQ(i, buf[i]);
    }
    EXPECT_EQ(32, stream->Seek(0, SEEK_CUR));
  }

  {
    EXPECT_EQ(30, stream->Seek(-2, SEEK_CUR));
    EXPECT_TRUE(stream->Read(base::span(buf).first(3u), &bytes_read));
    EXPECT_EQ(3u, bytes_read);
    EXPECT_THAT(base::span(buf).first(3u), ElementsAre(30, 31, 32));
  }

  {
    EXPECT_EQ(100, stream->Seek(0, SEEK_END));
    EXPECT_EQ(100, stream->Seek(0, SEEK_CUR));
  }
}

}  // namespace dmg
}  // namespace safe_browsing
