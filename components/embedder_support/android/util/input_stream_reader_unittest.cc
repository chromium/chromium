// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/input_stream_reader.h"

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "net/base/io_buffer.h"
#include "net/http/http_byte_range.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using embedder_support::InputStream;
using embedder_support::InputStreamReader;
using testing::_;
using testing::DoAll;
using testing::Ge;
using testing::InSequence;
using testing::Lt;
using testing::Ne;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;

class MockInputStream : public embedder_support::InputStream {
 public:
  MockInputStream() = default;
  ~MockInputStream() override = default;

  MOCK_CONST_METHOD1(BytesAvailable, bool(int* bytes_available));
  MOCK_METHOD2(Skip, bool(int64_t n, int64_t* bytes_skipped));
  MOCK_METHOD3(Read, bool(net::IOBuffer* dest, int length, int* bytes_read));
};

class InputStreamReaderTest : public Test {
 public:
  InputStreamReaderTest() : input_stream_reader_(&input_stream_) {}

 protected:
  int SeekRange(int first_byte, int last_byte) {
    net::HttpByteRange byte_range;
    byte_range.set_first_byte_position(first_byte);
    byte_range.set_last_byte_position(last_byte);
    return input_stream_reader_.Seek(byte_range);
  }

  int ReadRawData(scoped_refptr<net::IOBuffer> buffer, int bytesToRead) {
    return input_stream_reader_.ReadRawData(buffer.get(), bytesToRead);
  }

  MockInputStream input_stream_;
  InputStreamReader input_stream_reader_;
};

TEST_F(InputStreamReaderTest, BytesAvailableFailurePropagationOnSeek) {
  EXPECT_CALL(input_stream_, BytesAvailable(NotNull())).WillOnce(Return(false));

  ASSERT_GT(0, SeekRange(0, 0));
}

TEST_F(InputStreamReaderTest, SkipFailurePropagationOnSeek) {
  const int streamSize = 10;
  const int bytesToSkip = 5;

  EXPECT_CALL(input_stream_, BytesAvailable(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(streamSize), Return(true)));

  EXPECT_CALL(input_stream_, Skip(bytesToSkip, NotNull()))
      .WillOnce(Return(false));

  ASSERT_GT(0, SeekRange(bytesToSkip, streamSize - 1));
}

TEST_F(InputStreamReaderTest, SeekToMiddle) {
  const int streamSize = 10;
  const int bytesToSkip = 5;

  EXPECT_CALL(input_stream_, BytesAvailable(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(streamSize), Return(true)));

  EXPECT_CALL(input_stream_, Skip(bytesToSkip, NotNull()))
      .WillOnce(DoAll(SetArgPointee<1>(bytesToSkip), Return(true)));

  ASSERT_EQ(bytesToSkip, SeekRange(bytesToSkip, streamSize - 1));
}

TEST_F(InputStreamReaderTest, SeekToMiddleInSteps) {
  const int streamSize = 10;
  const int bytesToSkip = 5;

  EXPECT_CALL(input_stream_, BytesAvailable(NotNull()))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<0>(streamSize), Return(true)));

  EXPECT_CALL(input_stream_, Skip(_, _)).Times(0);
  {
    InSequence s;
    EXPECT_CALL(input_stream_, Skip(bytesToSkip, NotNull()))
        .WillOnce(DoAll(SetArgPointee<1>(bytesToSkip - 3), Return(true)))
        .RetiresOnSaturation();
    EXPECT_CALL(input_stream_, Skip(3, NotNull()))
        .WillOnce(DoAll(SetArgPointee<1>(1), Return(true)))
        .RetiresOnSaturation();
    EXPECT_CALL(input_stream_, Skip(2, NotNull()))
        .WillOnce(DoAll(SetArgPointee<1>(2), Return(true)))
        .RetiresOnSaturation();
  }

  ASSERT_EQ(bytesToSkip, SeekRange(bytesToSkip, streamSize - 1));
}

TEST_F(InputStreamReaderTest, SeekEmpty) {
  EXPECT_CALL(input_stream_, BytesAvailable(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)));

  ASSERT_EQ(0, SeekRange(0, 0));
}

TEST_F(InputStreamReaderTest, SeekMoreThanAvailable) {
  const int bytesAvailable = 256;
  EXPECT_CALL(input_stream_, BytesAvailable(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(bytesAvailable), Return(true)));

  ASSERT_GT(0, SeekRange(bytesAvailable, 2 * bytesAvailable));
}

TEST_F(InputStreamReaderTest, ReadFailure) {
  const int bytesToRead = 128;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(bytesToRead);
  EXPECT_CALL(input_stream_, Read(buffer.get(), bytesToRead, NotNull()))
      .WillOnce(Return(false));

  ASSERT_GT(0, ReadRawData(buffer, bytesToRead));
}

TEST_F(InputStreamReaderTest, ReadNothing) {
  const int bytesToRead = 0;
  // Size of net::IOBuffer can't be 0.
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  EXPECT_CALL(input_stream_, Read(buffer.get(), bytesToRead, NotNull()))
      .Times(0);

  ASSERT_EQ(0, ReadRawData(buffer, bytesToRead));
}

TEST_F(InputStreamReaderTest, ReadSuccess) {
  const int bytesToRead = 128;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(bytesToRead);

  EXPECT_CALL(input_stream_, Read(buffer.get(), bytesToRead, NotNull()))
      .WillOnce(DoAll(SetArgPointee<2>(bytesToRead), Return(true)));

  ASSERT_EQ(bytesToRead, ReadRawData(buffer, bytesToRead));
}
