// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/adb_client_socket.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_span.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockSocket : public net::MockClientSocket {
 public:
  explicit MockSocket(base::span<std::string> return_values_array)
      : MockClientSocket(net::NetLogWithSource()),
        return_values_array(return_values_array) {}

  MockSocket() : MockClientSocket(net::NetLogWithSource()) {}

  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    int result = ReadHelper(buf, buf_len);
    if (result == net::ERR_IO_PENDING) {
      // Note this really should be posted as a task, but that is a pain to
      // figure out.
      std::move(callback).Run(ReadLoop(buf, buf_len));
    }
    return result;
  }

  int ReadLoop(net::IOBuffer* buf, int buf_len) {
    int result;
    do {
      result = ReadHelper(buf, buf_len);
    } while (result == net::ERR_IO_PENDING);
    return result;
  }

  int ReadHelper(net::IOBuffer* buf, int buf_len) {
    if (return_values_array.empty()) {
      return 0;
    }
    int chunk_length = return_values_array.front().length();
    if (chunk_length > buf_len) {
      strncpy(buf->data(), return_values_array.front().data(), buf_len);
      return_values_array.front() = return_values_array.front().substr(buf_len);
      return buf_len;
    }
    strncpy(buf->data(), return_values_array.front().data(), chunk_length);
    return_values_array = return_values_array.subspan(1);
    if (chunk_length == 0) {
      return net::ERR_IO_PENDING;
    }
    return chunk_length;
  }

  int Write(
      net::IOBuffer* buf,
      int buf_len,
      const net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    return 0;
  }

  // The following functions are not expected to be used.
  int Connect(const net::CompletionOnceCallback callback) override {
    return net::ERR_UNEXPECTED;
  }
  bool GetSSLInfo(net::SSLInfo* ssl_info) override { return false; }
  bool WasEverUsed() const override { return false; }

  base::raw_span<std::string> return_values_array;
};

class AdbClientSocketTest : public testing::Test {
 public:
  void TestParsing(const char* adb_output,
                   bool has_length,
                   int expected_result_code,
                   const char* expected_response) {
    base::MockCallback<AdbClientSocket::CommandCallback> callback;
    EXPECT_CALL(callback, Run(expected_result_code, expected_response));
    AdbClientSocket::ParseOutput(has_length, callback.Get(),
                                 std::string(adb_output));
  }

  void TestReadUntilEOF_EOF(const char* data_on_buffer) {
    std::unique_ptr<MockSocket> socket = std::make_unique<MockSocket>();
    AdbClientSocket adb_socket(3);

    base::MockCallback<AdbClientSocket::ParserCallback> parse_callback;
    EXPECT_CALL(parse_callback, Run(data_on_buffer));

    base::MockCallback<AdbClientSocket::CommandCallback> response_callback;
    // The following means "expect not to be called."
    EXPECT_CALL(response_callback, Run(0, "")).Times(0);

    scoped_refptr<net::GrowableIOBuffer> buffer =
        base::MakeRefCounted<net::GrowableIOBuffer>();
    buffer->SetCapacity(100);
    strcpy(buffer->data(), data_on_buffer);
    buffer->set_offset(strlen(data_on_buffer));

    adb_socket.ReadUntilEOF(parse_callback.Get(), response_callback.Get(),
                            buffer, 0);
  }

  void TestReadUntilEOF_Error() {
    int error_code = -1;
    std::unique_ptr<MockSocket> socket = std::make_unique<MockSocket>();
    // 3 is an arbitrary meaningless number in the following call.
    AdbClientSocket adb_socket(3);

    base::MockCallback<AdbClientSocket::ParserCallback> parse_callback;
    // The following means "expect not to be called."
    EXPECT_CALL(parse_callback, Run("")).Times(0);

    base::MockCallback<AdbClientSocket::CommandCallback> response_callback;
    EXPECT_CALL(response_callback, Run(error_code, "IO error")).Times(1);
    scoped_refptr<net::GrowableIOBuffer> buffer =
        base::MakeRefCounted<net::GrowableIOBuffer>();
    buffer->SetCapacity(100);
    adb_socket.ReadUntilEOF(parse_callback.Get(), response_callback.Get(),
                            buffer, error_code);
  }

  void TestReadUntilEOF_Recurse(base::span<std::string> chunks,
                                std::string expected_result) {
    // 3 is an arbitrary meaningless number in the following call.
    AdbClientSocket adb_socket(3);
    adb_socket.socket_ = std::make_unique<MockSocket>(chunks);

    base::MockCallback<AdbClientSocket::ParserCallback> parse_callback;
    EXPECT_CALL(parse_callback, Run(expected_result.c_str())).Times(1);

    base::MockCallback<AdbClientSocket::CommandCallback> response_callback;
    // The following means "expect not to be called."
    EXPECT_CALL(response_callback, Run(0, "")).Times(0);
    scoped_refptr<net::GrowableIOBuffer> buffer =
        base::MakeRefCounted<net::GrowableIOBuffer>();
    int initial_capacity = 4;
    buffer->SetCapacity(initial_capacity);
    int result = adb_socket.socket_->Read(
        buffer.get(), initial_capacity,
        base::BindOnce(&AdbClientSocket::ReadUntilEOF,
                       base::Unretained(&adb_socket), parse_callback.Get(),
                       response_callback.Get(), buffer));
    if (result != net::ERR_IO_PENDING) {
      adb_socket.ReadUntilEOF(parse_callback.Get(), response_callback.Get(),
                              buffer, result);
    }
  }
  void TestReadStatus(const char* buffer_data,
                      int result,
                      const char* expected_result_string,
                      int expected_result_code) {
    base::MockCallback<AdbClientSocket::ParserCallback> parse_callback;
    // The following means "expect not to be called."
    EXPECT_CALL(parse_callback, Run("")).Times(0);

    base::MockCallback<AdbClientSocket::CommandCallback> response_callback;
    EXPECT_CALL(response_callback,
                Run(expected_result_code, expected_result_string))
        .Times(1);

    scoped_refptr<net::GrowableIOBuffer> buffer =
        base::MakeRefCounted<net::GrowableIOBuffer>();
    int initial_capacity = 100;
    buffer->SetCapacity(initial_capacity);
    if (result > 0) {
      strncpy(buffer->data(), buffer_data, result);
    }
    AdbClientSocket::ReadStatusOutput(response_callback.Get(), buffer, result);
  }
};

TEST_F(AdbClientSocketTest, ParseOutput) {
  TestParsing("OKAY000512345", true, 0, "12345");
  TestParsing("FAIL000512345", true, 1, "12345");
  TestParsing("OKAY12345", false, 0, "12345");
  TestParsing("FAIL12345", false, 1, "12345");
  TestParsing("ABC", false, 1, "ABC");
  TestParsing("ABC", true, 1, "ABC");
  TestParsing("OKAYAB", false, 0, "AB");
  TestParsing("OKAYAB", true, 1, "AB");
  TestParsing("OKAYOKAYOKAY", false, 0, "OKAY");
  TestParsing("", false, 1, "");
  TestParsing("", true, 1, "");
  TestParsing("OKAYOKAY", true, 0, "");
  TestParsing("OKAYOKAY", false, 0, "");
}

TEST_F(AdbClientSocketTest, ReadUntilEOF_EOFEmptyString) {
  TestReadUntilEOF_EOF("");
}
TEST_F(AdbClientSocketTest, ReadUntilEOF_EOFNonEmptyString1) {
  TestReadUntilEOF_EOF("blah");
}
TEST_F(AdbClientSocketTest, ReadUntilEOF_EOFNonEmptyString2) {
  TestReadUntilEOF_EOF("blahbla");
}

TEST_F(AdbClientSocketTest, ReadUntilEOF_Error) {
  TestReadUntilEOF_Error();
}
TEST_F(AdbClientSocketTest, ReadUntilEOF_GrowBuffer) {
  std::string chunks[] = {"This", "",     " data",   " should",
                          "",     " be ", "read in."};
  TestReadUntilEOF_Recurse(chunks, "This data should be read in.");
}
TEST_F(AdbClientSocketTest, ReadUntilEOF_EmptyChunks) {
  std::string chunks[] = {"", "", "", "", ""};
  TestReadUntilEOF_Recurse(chunks, "");
}
TEST_F(AdbClientSocketTest, ReadUntilEOF_Empty) {
  TestReadUntilEOF_Recurse({}, "");
}
TEST_F(AdbClientSocketTest, ReadUntilEOF_EmptyEndingChunk) {
  std::string chunks[] = {"yeah", ""};
  TestReadUntilEOF_Recurse(chunks, "yeah");
}

TEST_F(AdbClientSocketTest, ReadStatusOutput_Okay) {
  TestReadStatus("OKAY", 4, "OKAY", 0);
}
TEST_F(AdbClientSocketTest, ReadStatusOutput_OkayNulls) {
  TestReadStatus("OKAY\0\0\0\0", 8, "OKAY", 0);
}
TEST_F(AdbClientSocketTest, ReadStatusOutput_Fail) {
  TestReadStatus("FAIL", 4, "FAIL", 1);
}
TEST_F(AdbClientSocketTest, ReadStatusOutput_FailEmpty) {
  TestReadStatus("", 0, "FAIL", 1);
}
