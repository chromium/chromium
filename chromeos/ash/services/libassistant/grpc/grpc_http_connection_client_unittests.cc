// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/grpc_http_connection_client.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/grpc_transport/streaming/streaming_writer.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/http_connection_interface.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

using ::assistant::api::StreamHttpConnectionRequest;
using ::assistant::api::StreamHttpConnectionResponse;
using assistant_client::HttpConnection;

MATCHER_P(SerializedProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class MockHttpConnection : public HttpConnection {
 public:
  explicit MockHttpConnection(HttpConnection::Delegate* delegate)
      : delegate_(delegate) {}
  MockHttpConnection(const MockHttpConnection&) = delete;
  MockHttpConnection& operator=(const MockHttpConnection&) = delete;
  ~MockHttpConnection() override = default;

  // assistant_client::HttpConnection implementation:
  MOCK_METHOD(void,
              SetRequest,
              (const std::string& url, HttpConnection::Method),
              (override));
  MOCK_METHOD(void,
              AddHeader,
              (const std::string& name, const std::string& value),
              (override));
  MOCK_METHOD(void,
              SetUploadContent,
              (const std::string& content, const std::string& content_type),
              (override));
  MOCK_METHOD(void,
              SetChunkedUploadContentType,
              (const std::string& content_type),
              (override));
  MOCK_METHOD(void, EnableHeaderResponse, (), (override));
  MOCK_METHOD(void, EnablePartialResults, (), (override));
  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void,
              UploadData,
              (const std::string& data, bool is_last_chunk),
              (override));

  void SendOnHeaderResponse(const std::string& raw_headers) {
    delegate_->OnHeaderResponse(raw_headers);
  }

  void SendOnPartialResponse(const std::string& partial_response) {
    delegate_->OnPartialResponse(partial_response);
  }

  void SendOnCompleteResponse(int http_status,
                              const std::string& raw_headers,
                              const std::string& response) {
    delegate_->OnCompleteResponse(http_status, raw_headers, response);
  }

  void SendOnNetworkError(int error_code, const std::string& message) {
    delegate_->OnNetworkError(error_code, message);
  }

  void SendOnConnectionDestroyed() { delegate_->OnConnectionDestroyed(); }

 private:
  raw_ptr<HttpConnection::Delegate> delegate_;
};

class TestHttpConnectionFactory
    : public assistant_client::HttpConnectionFactory {
 public:
  TestHttpConnectionFactory() = default;
  TestHttpConnectionFactory(const TestHttpConnectionFactory&) = delete;
  TestHttpConnectionFactory& operator=(const TestHttpConnectionFactory&) =
      delete;
  ~TestHttpConnectionFactory() override = default;

  // assistant_client::HttpConnectionFactory implementation:
  HttpConnection* Create(HttpConnection::Delegate* delegate) override {
    http_connection_ = std::make_unique<MockHttpConnection>(delegate);
    return http_connection_.get();
  }

  MockHttpConnection* http_connection() { return http_connection_.get(); }

 private:
  std::unique_ptr<MockHttpConnection> http_connection_;
};

class MockStreamingWriter : public chromeos::libassistant::StreamingWriter<
                                StreamHttpConnectionRequest> {
 public:
  MockStreamingWriter() = default;
  ~MockStreamingWriter() override = default;

  // StreamingWriter implementation:
  MOCK_METHOD(void, Write, (StreamHttpConnectionRequest msg), (override));
  MOCK_METHOD(void, WritesDone, (), (override));
};

}  // namespace

class TestGrpcHttpConnectionService {
 public:
  explicit TestGrpcHttpConnectionService(GrpcHttpConnectionClient* client)
      : client_(client) {
    CreateWriter();
  }

  TestGrpcHttpConnectionService(const TestGrpcHttpConnectionService&) = delete;
  TestGrpcHttpConnectionService& operator=(
      const TestGrpcHttpConnectionService&) = delete;
  ~TestGrpcHttpConnectionService() = default;

  void SendCreateCommand() {
    StreamHttpConnectionResponse response;
    response.set_id(1);
    response.set_command(StreamHttpConnectionResponse::CREATE);
    WriteResponse(std::move(response));
  }

  void SendStartCommand(
      const std::string& url,
      StreamHttpConnectionResponse::Method method,
      const std::vector<std::pair<std::string, std::string>>& headers,
      const std::string& upload_content,
      const std::string& upload_content_type,
      bool enable_header_response,
      bool handle_partial_response) {
    StreamHttpConnectionResponse response;
    response.set_id(1);
    response.set_command(StreamHttpConnectionResponse::START);
    auto* parameters = response.mutable_parameters();
    parameters->set_url(url);
    parameters->set_method(method);
    for (const auto& header : headers) {
      auto* new_header = parameters->add_headers();
      new_header->set_name(header.first);
      new_header->set_value(header.second);
    }
    parameters->set_upload_content(upload_content);
    parameters->set_upload_content_type(upload_content_type);
    parameters->set_enable_header_response(enable_header_response);
    parameters->set_enable_partial_response(handle_partial_response);
    WriteResponse(std::move(response));
  }

  void SendPauseCommand() {
    StreamHttpConnectionResponse response;
    response.set_id(1);
    response.set_command(StreamHttpConnectionResponse::PAUSE);
    WriteResponse(std::move(response));
  }

  void SendResumeCommand() {
    StreamHttpConnectionResponse response;
    response.set_id(1);
    response.set_command(StreamHttpConnectionResponse::RESUME);
    WriteResponse(std::move(response));
  }

  void SendCloseCommand() {
    StreamHttpConnectionResponse response;
    response.set_id(1);
    response.set_command(StreamHttpConnectionResponse::CLOSE);
    WriteResponse(std::move(response));
  }

  void SendUploadDataCommand(const std::string& data, bool is_last_chunk) {
    StreamHttpConnectionResponse response;
    response.set_id(1);
    response.set_command(StreamHttpConnectionResponse::UPLOAD_DATA);
    auto* chunked_data = response.mutable_chunked_data();
    chunked_data->set_data(data);
    chunked_data->set_is_last_chunk(is_last_chunk);
    WriteResponse(std::move(response));
  }

  void SetWriteAvailable() {
    client_->OnRpcWriteAvailable(nullptr, writer_.get());
  }
  MockStreamingWriter& writer() { return *writer_; }
  void CreateWriter() { writer_ = std::make_unique<MockStreamingWriter>(); }
  void ResetWriter() { writer_.reset(); }

 private:
  void WriteResponse(StreamHttpConnectionResponse response) {
    client_->OnRpcReadAvailable(nullptr, response);
  }

  raw_ptr<GrpcHttpConnectionClient> client_;
  std::unique_ptr<MockStreamingWriter> writer_;
};

class GrpcHttpConnectionClientTest : public testing::Test {
 public:
  GrpcHttpConnectionClientTest() = default;
  GrpcHttpConnectionClientTest(const GrpcHttpConnectionClientTest&) = delete;
  GrpcHttpConnectionClientTest& operator=(const GrpcHttpConnectionClientTest&) =
      delete;
  ~GrpcHttpConnectionClientTest() override = default;

  void SetUp() override {
    client_ = std::make_unique<GrpcHttpConnectionClient>(
        &http_connection_factory_,
        /*server_address=*/"unix:///tmp/test.socket");
    service_ = std::make_unique<TestGrpcHttpConnectionService>(client_.get());
    client_->Start();
  }

 protected:
  MockHttpConnection* http_connection() {
    return http_connection_factory_.http_connection();
  }

  base::test::SingleThreadTaskEnvironment environment_;
  std::unique_ptr<TestGrpcHttpConnectionService> service_;
  TestHttpConnectionFactory http_connection_factory_;
  std::unique_ptr<GrpcHttpConnectionClient> client_;
};

TEST_F(GrpcHttpConnectionClientTest, CreateHttpConnection) {
  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);
  EXPECT_CALL(*connection, Close());
}

TEST_F(GrpcHttpConnectionClientTest, StartHttpConnection) {
  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);

  const std::string url = "url";
  const auto method = StreamHttpConnectionResponse::POST;
  std::vector<std::pair<std::string, std::string>> headers;
  headers.push_back({"name", "value"});
  const std::string upload_content = "upload_content";
  const std::string upload_content_type = "upload_content_type";
  const bool enable_header_response = true;
  const bool handle_partial_response = true;

  EXPECT_CALL(*connection, SetRequest(url, HttpConnection::POST));
  EXPECT_CALL(*connection, AddHeader("name", "value"));
  EXPECT_CALL(*connection,
              SetUploadContent(upload_content, upload_content_type));
  EXPECT_CALL(*connection, EnableHeaderResponse());
  EXPECT_CALL(*connection, EnablePartialResults());
  EXPECT_CALL(*connection, Start());
  EXPECT_CALL(*connection, Close());
  service_->SendStartCommand(url, method, headers, upload_content,
                             upload_content_type, enable_header_response,
                             handle_partial_response);
}

TEST_F(GrpcHttpConnectionClientTest, PauseHttpConnection) {
  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);

  EXPECT_CALL(*connection, Pause());
  EXPECT_CALL(*connection, Close());
  service_->SendPauseCommand();
}

TEST_F(GrpcHttpConnectionClientTest, ResumeHttpConnection) {
  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);

  EXPECT_CALL(*connection, Resume());
  EXPECT_CALL(*connection, Close());
  service_->SendResumeCommand();
}

TEST_F(GrpcHttpConnectionClientTest, CloseHttpConnection) {
  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);

  EXPECT_CALL(*connection, Close()).Times(1);
  service_->SendCloseCommand();
}

TEST_F(GrpcHttpConnectionClientTest, UploadData) {
  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);

  const std::string data = "data";
  const bool is_last_chunk = true;
  EXPECT_CALL(*connection, UploadData(data, is_last_chunk));
  EXPECT_CALL(*connection, Close());
  service_->SendUploadDataCommand(data, is_last_chunk);
}

TEST_F(GrpcHttpConnectionClientTest, RegisterOnTheFirstWriteAvailable) {
  StreamHttpConnectionRequest request;
  request.set_command(StreamHttpConnectionRequest::REGISTER);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  // Will trigger registering client.
  service_->SetWriteAvailable();
}

TEST_F(GrpcHttpConnectionClientTest, ReceiveOnHeaderResponse) {
  StreamHttpConnectionRequest request;
  request.set_command(StreamHttpConnectionRequest::REGISTER);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  // Will trigger registering client.
  service_->SetWriteAvailable();

  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);
  EXPECT_CALL(*connection, Close());

  const std::string raw_headers = "raw_headers";
  request.Clear();
  request.set_id(1);
  request.set_command(StreamHttpConnectionRequest::HANDLE_HEADER_RESPONSE);
  request.set_raw_headers(raw_headers);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  service_->SetWriteAvailable();
  connection->SendOnHeaderResponse(raw_headers);
  base::RunLoop().RunUntilIdle();
}

TEST_F(GrpcHttpConnectionClientTest, ReceiveOnPartialResponse) {
  StreamHttpConnectionRequest request;
  request.set_command(StreamHttpConnectionRequest::REGISTER);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  // Will trigger registering client.
  service_->SetWriteAvailable();

  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);
  EXPECT_CALL(*connection, Close());

  const std::string partial_response = "partial_response";
  request.Clear();
  request.set_id(1);
  request.set_command(StreamHttpConnectionRequest::HANDLE_PARTIAL_RESPONSE);
  request.set_partial_response(partial_response);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  service_->SetWriteAvailable();
  connection->SendOnPartialResponse(partial_response);
  base::RunLoop().RunUntilIdle();
}

TEST_F(GrpcHttpConnectionClientTest, ReceiveOnCompleteResponse) {
  StreamHttpConnectionRequest request;
  request.set_command(StreamHttpConnectionRequest::REGISTER);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  // Will trigger registering client.
  service_->SetWriteAvailable();

  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);
  EXPECT_CALL(*connection, Close());

  int http_status = 200;
  const std::string raw_headers = "raw_headers";
  const std::string response = "response";
  request.Clear();
  request.set_id(1);
  request.set_command(StreamHttpConnectionRequest::HANDLE_COMPLETE_RESPONSE);
  auto* res = request.mutable_complete_response();
  res->set_response_code(http_status);
  res->set_raw_headers(raw_headers);
  res->set_response(response);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  service_->SetWriteAvailable();
  connection->SendOnCompleteResponse(http_status, raw_headers, response);
  base::RunLoop().RunUntilIdle();
}

TEST_F(GrpcHttpConnectionClientTest, ReceiveOnNetworkError) {
  StreamHttpConnectionRequest request;
  request.set_command(StreamHttpConnectionRequest::REGISTER);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  // Will trigger registering client.
  service_->SetWriteAvailable();

  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);
  EXPECT_CALL(*connection, Close());

  int error_code = 501;
  const std::string message = "message";
  request.Clear();
  request.set_id(1);
  request.set_command(StreamHttpConnectionRequest::HANDLE_NETWORK_ERROR);
  auto* error = request.mutable_error();
  error->set_error_code(error_code);
  error->set_error_message(message);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  service_->SetWriteAvailable();
  connection->SendOnNetworkError(error_code, message);
  base::RunLoop().RunUntilIdle();
}

TEST_F(GrpcHttpConnectionClientTest, NotCrashWhenWriterGone) {
  StreamHttpConnectionRequest request;
  request.set_command(StreamHttpConnectionRequest::REGISTER);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));
  // Will trigger registering client.
  service_->SetWriteAvailable();
  base::RunLoop().RunUntilIdle();

  service_->SendCreateCommand();
  auto* connection = http_connection();
  ASSERT_TRUE(connection);
  EXPECT_CALL(*connection, Close());

  const std::string raw_headers = "raw_headers";
  request.Clear();
  request.set_id(1);
  request.set_command(StreamHttpConnectionRequest::HANDLE_HEADER_RESPONSE);
  request.set_raw_headers(raw_headers);
  connection->SendOnHeaderResponse(raw_headers);
  EXPECT_CALL(service_->writer(), Write(SerializedProtoEquals(request)));

  // Simulate the case that the writer becomes nullptr. Should not crash.
  service_->SetWriteAvailable();
  service_->ResetWriter();
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash::libassistant
