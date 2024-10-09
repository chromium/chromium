// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_socket.h"

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "components/media_router/common/providers/cast/channel/cast_auth_util.h"
#include "components/media_router/common/providers/cast/channel/cast_framer.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "components/media_router/common/providers/cast/channel/cast_transport.h"
#include "components/media_router/common/providers/cast/channel/logger.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/rsa_private_key.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_server_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/pem.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

const int64_t kDistantTimeoutMillis = 100000;  // 100 seconds (never hit).

using ::testing::_;
using ::testing::A;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;

using ::openscreen::cast::proto::CastMessage;

namespace cast_channel {
namespace {
const char kAuthNamespace[] = "urn:x-cast:com.google.cast.tp.deviceauth";

// Returns an auth challenge message inline.
CastMessage CreateAuthChallenge() {
  CastMessage output;
  CreateAuthChallengeMessage(&output, AuthContext::Create());
  return output;
}

// Returns an auth challenge response message inline.
CastMessage CreateAuthReply() {
  CastMessage output;
  output.set_protocol_version(CastMessage::CASTV2_1_0);
  output.set_source_id("sender-0");
  output.set_destination_id("receiver-0");
  output.set_payload_type(CastMessage::BINARY);
  output.set_payload_binary("abcd");
  output.set_namespace_(kAuthNamespace);
  return output;
}

CastMessage CreateTestMessage() {
  CastMessage test_message;
  test_message.set_protocol_version(CastMessage::CASTV2_1_0);
  test_message.set_namespace_("ns");
  test_message.set_source_id("source");
  test_message.set_destination_id("dest");
  test_message.set_payload_type(CastMessage::STRING);
  test_message.set_payload_utf8("payload");
  return test_message;
}

base::FilePath GetTestCertsDirectory() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL("components"));
  path = path.Append(FILE_PATH_LITERAL("test"));
  path = path.Append(FILE_PATH_LITERAL("data"));
  path = path.Append(FILE_PATH_LITERAL("media_router"));
  path = path.Append(FILE_PATH_LITERAL("common"));
  path = path.Append(FILE_PATH_LITERAL("providers"));
  path = path.Append(FILE_PATH_LITERAL("cast"));
  path = path.Append(FILE_PATH_LITERAL("channel"));
  return path;
}

class MockTCPSocket : public net::MockTCPClientSocket {
 public:
  MockTCPSocket(bool do_nothing, net::SocketDataProvider* socket_provider)
      : net::MockTCPClientSocket(net::AddressList(), nullptr, socket_provider) {
    do_nothing_ = do_nothing;
    set_enable_read_if_ready(true);
  }

  MockTCPSocket(const MockTCPSocket&) = delete;
  MockTCPSocket& operator=(const MockTCPSocket&) = delete;

  int Connect(net::CompletionOnceCallback callback) override {
    if (do_nothing_) {
      // Stall the I/O event loop.
      return net::ERR_IO_PENDING;
    }
    return net::MockTCPClientSocket::Connect(std::move(callback));
  }

 private:
  bool do_nothing_;
};

class CompleteHandler {
 public:
  CompleteHandler() = default;

  CompleteHandler(const CompleteHandler&) = delete;
  CompleteHandler& operator=(const CompleteHandler&) = delete;

  MOCK_METHOD1(OnCloseComplete, void(int result));
  MOCK_METHOD1(OnConnectComplete, void(CastSocket* socket));
  MOCK_METHOD1(OnWriteComplete, void(int result));
  MOCK_METHOD1(OnReadComplete, void(int result));
};

class TestCastSocketBase : public CastSocketImpl {
 public:
  TestCastSocketBase(network::mojom::NetworkContext* network_context,
                     const CastSocketOpenParams& open_params,
                     Logger* logger)
      : CastSocketImpl(base::BindRepeating(
                           [](network::mojom::NetworkContext* network_context) {
                             return network_context;
                           },
                           network_context),
                       open_params,
                       logger,
                       AuthContext::Create()),
        verify_challenge_result_(true),
        verify_challenge_disallow_(false),
        mock_timer_(new base::MockOneShotTimer()) {
    SetPeerCertForTesting(
        net::ImportCertFromFile(GetTestCertsDirectory(), "self_signed.pem"));
  }

  TestCastSocketBase(const TestCastSocketBase&) = delete;
  TestCastSocketBase& operator=(const TestCastSocketBase&) = delete;

  ~TestCastSocketBase() override = default;

  void SetVerifyChallengeResult(bool value) {
    verify_challenge_result_ = value;
  }

  void TriggerTimeout() { mock_timer_->Fire(); }

  bool TestVerifyChannelPolicyNone() {
    AuthResult authResult;
    return VerifyChannelPolicy(authResult);
  }

  void DisallowVerifyChallengeResult() { verify_challenge_disallow_ = true; }

 protected:
  bool VerifyChallengeReply() override {
    EXPECT_FALSE(verify_challenge_disallow_);
    return verify_challenge_result_;
  }

  base::OneShotTimer* GetTimer() override { return mock_timer_.get(); }

  // Simulated result of verifying challenge reply.
  bool verify_challenge_result_;
  bool verify_challenge_disallow_;
  std::unique_ptr<base::MockOneShotTimer> mock_timer_;
};

class MockTestCastSocket : public TestCastSocketBase {
 public:
  static std::unique_ptr<MockTestCastSocket> CreateSecure(
      network::mojom::NetworkContext* network_context,
      const CastSocketOpenParams& open_params,
      Logger* logger) {
    return std::make_unique<MockTestCastSocket>(network_context, open_params,
                                                logger);
  }

  using TestCastSocketBase::TestCastSocketBase;

  MockTestCastSocket(network::mojom::NetworkContext* network_context,
                     const CastSocketOpenParams& open_params,
                     Logger* logger)
      : TestCastSocketBase(network_context, open_params, logger) {}

  MockTestCastSocket(const MockTestCastSocket&) = delete;
  MockTestCastSocket& operator=(const MockTestCastSocket&) = delete;

  ~MockTestCastSocket() override = default;

  void SetupMockTransport() {
    mock_transport_ = new MockCastTransport;
    SetTransportForTesting(base::WrapUnique(mock_transport_.get()));
  }

  bool TestVerifyChannelPolicyAudioOnly() {
    AuthResult authResult;
    authResult.channel_policies |= AuthResult::POLICY_AUDIO_ONLY;
    return VerifyChannelPolicy(authResult);
  }

  MockCastTransport* GetMockTransport() {
    CHECK(mock_transport_);
    return mock_transport_;
  }

 private:
  raw_ptr<MockCastTransport, AcrossTasksDanglingUntriaged> mock_transport_ =
      nullptr;
};

// TODO(crbug.com/41439190):  Remove this class.
class TestSocketFactory : public net::ClientSocketFactory {
 public:
  explicit TestSocketFactory(net::IPEndPoint ip) : ip_(ip) {}

  TestSocketFactory(const TestSocketFactory&) = delete;
  TestSocketFactory& operator=(const TestSocketFactory&) = delete;

  ~TestSocketFactory() override = default;

  // Socket connection helpers.
  void SetupTcpConnect(net::IoMode mode, int result) {
    tcp_connect_data_ = std::make_unique<net::MockConnect>(mode, result, ip_);
  }
  void SetupSslConnect(net::IoMode mode, int result) {
    ssl_connect_data_ = std::make_unique<net::MockConnect>(mode, result, ip_);
  }

  // Socket I/O helpers.
  void AddWriteResult(const net::MockWrite& write) { writes_.push_back(write); }
  void AddWriteResult(net::IoMode mode, int result) {
    AddWriteResult(net::MockWrite(mode, result));
  }
  void AddWriteResultForData(net::IoMode mode, const std::string& msg) {
    AddWriteResult(mode, msg.size());
  }
  void AddReadResult(const net::MockRead& read) { reads_.push_back(read); }
  void AddReadResult(net::IoMode mode, int result) {
    AddReadResult(net::MockRead(mode, result));
  }
  void AddReadResultForData(net::IoMode mode, const std::string& data) {
    AddReadResult(net::MockRead(mode, data.c_str(), data.size()));
  }

  // Helpers for modifying other connection-related behaviors.
  void SetupTcpConnectUnresponsive() { tcp_unresponsive_ = true; }

  void SetTcpSocket(
      std::unique_ptr<net::TransportClientSocket> tcp_client_socket) {
    tcp_client_socket_ = std::move(tcp_client_socket);
  }

  void SetTLSSocketCreatedClosure(base::OnceClosure closure) {
    tls_socket_created_ = std::move(closure);
  }

  void Pause() {
    if (socket_data_provider_)
      socket_data_provider_->Pause();
    else
      socket_data_provider_paused_ = true;
  }

  void Resume() { socket_data_provider_->Resume(); }

 private:
  std::unique_ptr<net::DatagramClientSocket> CreateDatagramClientSocket(
      net::DatagramSocket::BindType,
      net::NetLog*,
      const net::NetLogSource&) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  std::unique_ptr<net::TransportClientSocket> CreateTransportClientSocket(
      const net::AddressList&,
      std::unique_ptr<net::SocketPerformanceWatcher>,
      net::NetworkQualityEstimator*,
      net::NetLog*,
      const net::NetLogSource&) override {
    if (tcp_client_socket_)
      return std::move(tcp_client_socket_);

    if (tcp_unresponsive_) {
      socket_data_provider_ = std::make_unique<net::StaticSocketDataProvider>();
      return std::unique_ptr<net::TransportClientSocket>(
          new MockTCPSocket(true, socket_data_provider_.get()));
    } else {
      socket_data_provider_ =
          std::make_unique<net::StaticSocketDataProvider>(reads_, writes_);
      socket_data_provider_->set_connect_data(*tcp_connect_data_);
      if (socket_data_provider_paused_)
        socket_data_provider_->Pause();
      return std::unique_ptr<net::TransportClientSocket>(
          new MockTCPSocket(false, socket_data_provider_.get()));
    }
  }
  std::unique_ptr<net::SSLClientSocket> CreateSSLClientSocket(
      net::SSLClientContext* context,
      std::unique_ptr<net::StreamSocket> nested_socket,
      const net::HostPortPair& host_and_port,
      const net::SSLConfig& ssl_config) override {
    if (!ssl_connect_data_) {
      // Test isn't overriding SSL socket creation.
      return net::ClientSocketFactory::GetDefaultFactory()
          ->CreateSSLClientSocket(context, std::move(nested_socket),
                                  host_and_port, ssl_config);
    }
    ssl_socket_data_provider_ = std::make_unique<net::SSLSocketDataProvider>(
        ssl_connect_data_->mode, ssl_connect_data_->result);

    if (tls_socket_created_)
      std::move(tls_socket_created_).Run();

    return std::make_unique<net::MockSSLClientSocket>(
        std::move(nested_socket), net::HostPortPair(), net::SSLConfig(),
        ssl_socket_data_provider_.get());
  }

  net::IPEndPoint ip_;
  // Simulated connect data
  std::unique_ptr<net::MockConnect> tcp_connect_data_;
  std::unique_ptr<net::MockConnect> ssl_connect_data_;
  // Simulated read / write data
  std::vector<net::MockWrite> writes_;
  std::vector<net::MockRead> reads_;
  std::unique_ptr<net::StaticSocketDataProvider> socket_data_provider_;
  std::unique_ptr<net::SSLSocketDataProvider> ssl_socket_data_provider_;
  bool socket_data_provider_paused_ = false;
  // If true, makes TCP connection process stall. For timeout testing.
  bool tcp_unresponsive_ = false;
  std::unique_ptr<net::TransportClientSocket> tcp_client_socket_;
  base::OnceClosure tls_socket_created_;
};

class CastSocketTestBase : public testing::Test {
 protected:
  CastSocketTestBase()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        logger_(new Logger()),
        observer_(new MockCastSocketObserver()),
        socket_open_params_(CreateIPEndPointForTest(),
                            base::Milliseconds(kDistantTimeoutMillis)),
        client_socket_factory_(socket_open_params_.ip_endpoint) {}

  CastSocketTestBase(const CastSocketTestBase&) = delete;
  CastSocketTestBase& operator=(const CastSocketTestBase&) = delete;

  ~CastSocketTestBase() override = default;

  void SetUp() override {
    EXPECT_CALL(*observer_, OnMessage(_, _)).Times(0);

    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(
        &client_socket_factory_);
    url_request_context_ = context_builder->Build();
    network_context_ = std::make_unique<network::NetworkContext>(
        nullptr, network_context_remote_.BindNewPipeAndPassReceiver(),
        url_request_context_.get(),
        /*cors_exempt_header_list=*/std::vector<std::string>());
  }

  // Runs all pending tasks in the message loop.
  void RunPendingTasks() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  TestSocketFactory* client_socket_factory() { return &client_socket_factory_; }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  raw_ptr<Logger, AcrossTasksDanglingUntriaged> logger_;
  CompleteHandler handler_;
  std::unique_ptr<MockCastSocketObserver> observer_;
  CastSocketOpenParams socket_open_params_;
  TestSocketFactory client_socket_factory_;
};

class MockCastSocketTest : public CastSocketTestBase {
 public:
  MockCastSocketTest(const MockCastSocketTest&) = delete;
  MockCastSocketTest& operator=(const MockCastSocketTest&) = delete;

 protected:
  MockCastSocketTest() = default;

  void TearDown() override {
    if (socket_) {
      EXPECT_CALL(handler_, OnCloseComplete(net::OK));
      socket_->Close(base::BindOnce(&CompleteHandler::OnCloseComplete,
                                    base::Unretained(&handler_)));
    }
  }

  void CreateCastSocketSecure() {
    socket_ = MockTestCastSocket::CreateSecure(network_context_.get(),
                                               socket_open_params_, logger_);
  }

  void HandleAuthHandshake() {
    socket_->SetupMockTransport();
    CastMessage challenge_proto = CreateAuthChallenge();
    EXPECT_CALL(*socket_->GetMockTransport(),
                SendMessage_(EqualsProto(challenge_proto), _))
        .WillOnce(PostCompletionCallbackTask<1>(net::OK));
    EXPECT_CALL(*socket_->GetMockTransport(), Start());
    EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
    socket_->AddObserver(observer_.get());
    socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                    base::Unretained(&handler_)));
    RunPendingTasks();
    socket_->GetMockTransport()->current_delegate()->OnMessage(
        CreateAuthReply());
    RunPendingTasks();
  }

  std::unique_ptr<MockTestCastSocket> socket_;
};

class SslCastSocketTest : public CastSocketTestBase {
 public:
  SslCastSocketTest(const SslCastSocketTest&) = delete;
  SslCastSocketTest& operator=(const SslCastSocketTest&) = delete;

 protected:
  SslCastSocketTest() = default;

  void TearDown() override {
    if (socket_) {
      EXPECT_CALL(handler_, OnCloseComplete(net::OK));
      socket_->Close(base::BindOnce(&CompleteHandler::OnCloseComplete,
                                    base::Unretained(&handler_)));
    }
  }

  void CreateSockets() {
    socket_ = std::make_unique<TestCastSocketBase>(
        network_context_.get(), socket_open_params_, logger_);

    server_cert_ =
        net::ImportCertFromFile(GetTestCertsDirectory(), "self_signed.pem");
    ASSERT_TRUE(server_cert_);
    server_private_key_ = ReadTestKeyFromPEM("self_signed.pem");
    ASSERT_TRUE(server_private_key_);
    server_context_ = CreateSSLServerContext(
        server_cert_.get(), *server_private_key_, server_ssl_config_);

    tcp_server_socket_ =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    ASSERT_EQ(net::OK,
              tcp_server_socket_->ListenWithAddressAndPort("127.0.0.1", 0, 1));
    net::IPEndPoint server_address;
    ASSERT_EQ(net::OK, tcp_server_socket_->GetLocalAddress(&server_address));
    tcp_client_socket_ = std::make_unique<net::TCPClientSocket>(
        net::AddressList(server_address), nullptr, nullptr, nullptr,
        net::NetLogSource());

    std::unique_ptr<net::StreamSocket> accepted_socket;
    accept_result_ = tcp_server_socket_->Accept(
        &accepted_socket, base::BindOnce(&SslCastSocketTest::TcpAcceptCallback,
                                         base::Unretained(this)));
    connect_result_ = tcp_client_socket_->Connect(base::BindOnce(
        &SslCastSocketTest::TcpConnectCallback, base::Unretained(this)));
    while (accept_result_ == net::ERR_IO_PENDING ||
           connect_result_ == net::ERR_IO_PENDING) {
      RunPendingTasks();
    }
    ASSERT_EQ(net::OK, accept_result_);
    ASSERT_EQ(net::OK, connect_result_);
    ASSERT_TRUE(accepted_socket);
    ASSERT_TRUE(tcp_client_socket_->IsConnected());

    server_socket_ =
        server_context_->CreateSSLServerSocket(std::move(accepted_socket));
    ASSERT_TRUE(server_socket_);

    client_socket_factory()->SetTcpSocket(std::move(tcp_client_socket_));
  }

  void ConnectSockets() {
    socket_->AddObserver(observer_.get());
    socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                    base::Unretained(&handler_)));

    net::TestCompletionCallback handshake_callback;
    int server_ret = handshake_callback.GetResult(
        server_socket_->Handshake(handshake_callback.callback()));

    ASSERT_EQ(net::OK, server_ret);
  }

  void TcpAcceptCallback(int result) { accept_result_ = result; }

  void TcpConnectCallback(int result) { connect_result_ = result; }

  std::unique_ptr<crypto::RSAPrivateKey> ReadTestKeyFromPEM(
      std::string_view name) {
    base::FilePath key_path = GetTestCertsDirectory().AppendASCII(name);
    std::string pem_data;
    if (!base::ReadFileToString(key_path, &pem_data)) {
      return nullptr;
    }

    const std::vector<std::string> headers({"PRIVATE KEY"});
    bssl::PEMTokenizer pem_tokenizer(pem_data, headers);
    if (!pem_tokenizer.GetNext()) {
      return nullptr;
    }
    std::vector<uint8_t> key_vector(pem_tokenizer.data().begin(),
                                    pem_tokenizer.data().end());
    std::unique_ptr<crypto::RSAPrivateKey> key(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vector));
    return key;
  }

  int ReadExactLength(net::IOBuffer* buffer,
                      int buffer_length,
                      net::Socket* socket) {
    scoped_refptr<net::DrainableIOBuffer> draining_buffer =
        base::MakeRefCounted<net::DrainableIOBuffer>(buffer, buffer_length);
    while (draining_buffer->BytesRemaining() > 0) {
      net::TestCompletionCallback read_callback;
      int read_result = read_callback.GetResult(server_socket_->Read(
          draining_buffer.get(), draining_buffer->BytesRemaining(),
          read_callback.callback()));
      EXPECT_GT(read_result, 0);
      draining_buffer->DidConsume(read_result);
    }
    return buffer_length;
  }

  int WriteExactLength(net::IOBuffer* buffer,
                       int buffer_length,
                       net::Socket* socket) {
    scoped_refptr<net::DrainableIOBuffer> draining_buffer =
        base::MakeRefCounted<net::DrainableIOBuffer>(buffer, buffer_length);
    while (draining_buffer->BytesRemaining() > 0) {
      net::TestCompletionCallback write_callback;
      int write_result = write_callback.GetResult(server_socket_->Write(
          draining_buffer.get(), draining_buffer->BytesRemaining(),
          write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
      EXPECT_GT(write_result, 0);
      draining_buffer->DidConsume(write_result);
    }
    return buffer_length;
  }

  // Result values used for TCP socket setup.  These should contain values from
  // net::Error.
  int accept_result_;
  int connect_result_;

  // Underlying TCP sockets for |socket_| to communicate with |server_socket_|
  // when testing with the real SSL implementation.
  std::unique_ptr<net::TransportClientSocket> tcp_client_socket_;
  std::unique_ptr<net::TCPServerSocket> tcp_server_socket_;

  std::unique_ptr<TestCastSocketBase> socket_;

  // `server_socket_` is used for the *RealSSL tests in order to test the
  // CastSocket over a real SSL socket.  The other members below are used to
  // initialize `server_socket_`.
  std::unique_ptr<net::SSLServerContext> server_context_;
  std::unique_ptr<crypto::RSAPrivateKey> server_private_key_;
  scoped_refptr<net::X509Certificate> server_cert_;
  net::SSLServerConfig server_ssl_config_;

  // `server_socket_` must be declared below objects that are passed to it as
  // raw pointers, to avoid dangling pointer warnings.
  std::unique_ptr<net::SSLServerSocket> server_socket_;
};

}  // namespace

// Tests that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection succeeds (async)
// - Cert is extracted successfully
// - Challenge request is sent (async)
// - Challenge response is received (async)
// - Credentials are verified successfuly
TEST_F(MockCastSocketTest, TestConnectFullSecureFlowAsync) {
  CreateCastSocketSecure();
  client_socket_factory()->SetupTcpConnect(net::ASYNC, net::OK);
  client_socket_factory()->SetupSslConnect(net::ASYNC, net::OK);

  HandleAuthHandshake();

  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());
}

// Tests that the following connection flow works:
// - TCP connection succeeds (sync)
// - SSL connection succeeds (sync)
// - Cert is extracted successfully
// - Challenge request is sent (sync)
// - Challenge response is received (sync)
// - Credentials are verified successfuly
TEST_F(MockCastSocketTest, TestConnectFullSecureFlowSync) {
  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  client_socket_factory()->SetupSslConnect(net::SYNCHRONOUS, net::OK);

  CreateCastSocketSecure();
  HandleAuthHandshake();

  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());
}

// Test that an AuthMessage with a mangled namespace triggers cancelation
// of the connection event loop.
TEST_F(MockCastSocketTest, TestConnectAuthMessageCorrupted) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();

  client_socket_factory()->SetupTcpConnect(net::ASYNC, net::OK);
  client_socket_factory()->SetupSslConnect(net::ASYNC, net::OK);

  CastMessage challenge_proto = CreateAuthChallenge();
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage_(EqualsProto(challenge_proto), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  EXPECT_CALL(*socket_->GetMockTransport(), Start());
  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();
  CastMessage mangled_auth_reply = CreateAuthReply();
  mangled_auth_reply.set_namespace_("BOGUS_NAMESPACE");

  socket_->GetMockTransport()->current_delegate()->OnMessage(
      mangled_auth_reply);
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::TRANSPORT_ERROR, socket_->error_state());

  // Verifies that the CastSocket's resources were torn down during channel
  // close. (see http://crbug.com/504078)
  EXPECT_EQ(nullptr, socket_->transport());
}

// Test connection error - TCP connect fails (async)
TEST_F(MockCastSocketTest, TestConnectTcpConnectErrorAsync) {
  CreateCastSocketSecure();

  client_socket_factory()->SetupTcpConnect(net::ASYNC, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::CONNECT_ERROR, socket_->error_state());
}

// Test connection error - TCP connect fails (sync)
TEST_F(MockCastSocketTest, TestConnectTcpConnectErrorSync) {
  CreateCastSocketSecure();

  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::CONNECT_ERROR, socket_->error_state());
}

// Test connection error - timeout
TEST_F(MockCastSocketTest, TestConnectTcpTimeoutError) {
  CreateCastSocketSecure();
  client_socket_factory()->SetupTcpConnectUnresponsive();
  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  EXPECT_CALL(*observer_, OnError(_, ChannelError::CONNECT_TIMEOUT));
  socket_->AddObserver(observer_.get());
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CONNECTING, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());
  socket_->TriggerTimeout();
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::CONNECT_TIMEOUT, socket_->error_state());
}

// Test connection error - TCP socket returns timeout
TEST_F(MockCastSocketTest, TestConnectTcpSocketTimeoutError) {
  CreateCastSocketSecure();
  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS,
                                           net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  EXPECT_CALL(*observer_, OnError(_, ChannelError::CONNECT_TIMEOUT));
  socket_->AddObserver(observer_.get());
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::CONNECT_TIMEOUT, socket_->error_state());
  EXPECT_EQ(net::ERR_CONNECTION_TIMED_OUT,
            logger_->GetLastError(socket_->id()).net_return_value);
}

// Test connection error - SSL connect fails (async)
TEST_F(MockCastSocketTest, TestConnectSslConnectErrorAsync) {
  CreateCastSocketSecure();

  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  client_socket_factory()->SetupSslConnect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::AUTHENTICATION_ERROR, socket_->error_state());
}

// Test connection error - SSL connect fails (sync)
TEST_F(MockCastSocketTest, TestConnectSslConnectErrorSync) {
  CreateCastSocketSecure();

  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  client_socket_factory()->SetupSslConnect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::AUTHENTICATION_ERROR, socket_->error_state());
  EXPECT_EQ(net::ERR_FAILED,
            logger_->GetLastError(socket_->id()).net_return_value);
}

// Test connection error - SSL connect times out (sync)
TEST_F(MockCastSocketTest, TestConnectSslConnectTimeoutSync) {
  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  client_socket_factory()->SetupSslConnect(net::SYNCHRONOUS,
                                           net::ERR_CONNECTION_TIMED_OUT);

  CreateCastSocketSecure();

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::CONNECT_TIMEOUT, socket_->error_state());
  EXPECT_EQ(net::ERR_CONNECTION_TIMED_OUT,
            logger_->GetLastError(socket_->id()).net_return_value);
}

// Test connection error - SSL connect times out (async)
TEST_F(MockCastSocketTest, TestConnectSslConnectTimeoutAsync) {
  CreateCastSocketSecure();

  client_socket_factory()->SetupTcpConnect(net::ASYNC, net::OK);
  client_socket_factory()->SetupSslConnect(net::ASYNC,
                                           net::ERR_CONNECTION_TIMED_OUT);

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::CONNECT_TIMEOUT, socket_->error_state());
}

// Test connection error - challenge send fails
TEST_F(MockCastSocketTest, TestConnectChallengeSendError) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();

  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  client_socket_factory()->SetupSslConnect(net::SYNCHRONOUS, net::OK);
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage_(EqualsProto(CreateAuthChallenge()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::ERR_CONNECTION_RESET));

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::CAST_SOCKET_ERROR, socket_->error_state());
}

// Test connection error - connection is destroyed after the challenge is
// sent, with the async result still lurking in the task queue.
TEST_F(MockCastSocketTest, TestConnectDestroyedAfterChallengeSent) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();
  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  client_socket_factory()->SetupSslConnect(net::SYNCHRONOUS, net::OK);
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage_(EqualsProto(CreateAuthChallenge()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::ERR_CONNECTION_RESET));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();
  socket_.reset();
  RunPendingTasks();
}

// Test connection error - challenge reply receive fails
TEST_F(MockCastSocketTest, TestConnectChallengeReplyReceiveError) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();

  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  client_socket_factory()->SetupSslConnect(net::SYNCHRONOUS, net::OK);
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage_(EqualsProto(CreateAuthChallenge()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  client_socket_factory()->AddReadResult(net::SYNCHRONOUS, net::ERR_FAILED);
  EXPECT_CALL(*observer_, OnError(_, ChannelError::CAST_SOCKET_ERROR));
  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  EXPECT_CALL(*socket_->GetMockTransport(), Start());
  socket_->AddObserver(observer_.get());
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();
  socket_->GetMockTransport()->current_delegate()->OnError(
      ChannelError::CAST_SOCKET_ERROR);
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::CAST_SOCKET_ERROR, socket_->error_state());
}

TEST_F(MockCastSocketTest, TestConnectChallengeVerificationFails) {
  CreateCastSocketSecure();
  socket_->SetupMockTransport();
  client_socket_factory()->SetupTcpConnect(net::ASYNC, net::OK);
  client_socket_factory()->SetupSslConnect(net::ASYNC, net::OK);
  socket_->SetVerifyChallengeResult(false);

  EXPECT_CALL(*observer_, OnError(_, ChannelError::AUTHENTICATION_ERROR));
  CastMessage challenge_proto = CreateAuthChallenge();
  EXPECT_CALL(*socket_->GetMockTransport(),
              SendMessage_(EqualsProto(challenge_proto), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  EXPECT_CALL(*socket_->GetMockTransport(), Start());
  socket_->AddObserver(observer_.get());
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();
  socket_->GetMockTransport()->current_delegate()->OnMessage(CreateAuthReply());
  RunPendingTasks();

  EXPECT_EQ(ReadyState::CLOSED, socket_->ready_state());
  EXPECT_EQ(ChannelError::AUTHENTICATION_ERROR, socket_->error_state());
}

// Sends message data through an actual non-mocked CastTransport object,
// testing the two components in integration.
TEST_F(MockCastSocketTest, TestConnectEndToEndWithRealTransportAsync) {
  CreateCastSocketSecure();
  client_socket_factory()->SetupTcpConnect(net::ASYNC, net::OK);
  client_socket_factory()->SetupSslConnect(net::ASYNC, net::OK);

  // Set low-level auth challenge expectations.
  CastMessage challenge = CreateAuthChallenge();
  std::string challenge_str;
  EXPECT_TRUE(MessageFramer::Serialize(challenge, &challenge_str));
  client_socket_factory()->AddWriteResultForData(net::ASYNC, challenge_str);

  // Set low-level auth reply expectations.
  CastMessage reply = CreateAuthReply();
  std::string reply_str;
  EXPECT_TRUE(MessageFramer::Serialize(reply, &reply_str));
  client_socket_factory()->AddReadResultForData(net::ASYNC, reply_str);
  client_socket_factory()->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);
  // Make sure the data is ready by the TLS socket and not the TCP socket.
  client_socket_factory()->Pause();
  client_socket_factory()->SetTLSSocketCreatedClosure(
      base::BindLambdaForTesting([&] { client_socket_factory()->Resume(); }));

  CastMessage test_message = CreateTestMessage();
  std::string test_message_str;
  EXPECT_TRUE(MessageFramer::Serialize(test_message, &test_message_str));
  client_socket_factory()->AddWriteResultForData(net::ASYNC, test_message_str);

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();
  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());

  // Send the test message through a real transport object.
  EXPECT_CALL(handler_, OnWriteComplete(net::OK));
  socket_->transport()->SendMessage(
      test_message, base::BindOnce(&CompleteHandler::OnWriteComplete,
                                   base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());
}

// Same as TestConnectEndToEndWithRealTransportAsync, except synchronous.
TEST_F(MockCastSocketTest, TestConnectEndToEndWithRealTransportSync) {
  CreateCastSocketSecure();
  client_socket_factory()->SetupTcpConnect(net::SYNCHRONOUS, net::OK);
  client_socket_factory()->SetupSslConnect(net::SYNCHRONOUS, net::OK);

  // Set low-level auth challenge expectations.
  CastMessage challenge = CreateAuthChallenge();
  std::string challenge_str;
  EXPECT_TRUE(MessageFramer::Serialize(challenge, &challenge_str));
  client_socket_factory()->AddWriteResultForData(net::SYNCHRONOUS,
                                                 challenge_str);

  // Set low-level auth reply expectations.
  CastMessage reply = CreateAuthReply();
  std::string reply_str;
  EXPECT_TRUE(MessageFramer::Serialize(reply, &reply_str));
  client_socket_factory()->AddReadResultForData(net::SYNCHRONOUS, reply_str);
  client_socket_factory()->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);
  // Make sure the data is ready by the TLS socket and not the TCP socket.
  client_socket_factory()->Pause();
  client_socket_factory()->SetTLSSocketCreatedClosure(
      base::BindLambdaForTesting([&] { client_socket_factory()->Resume(); }));

  CastMessage test_message = CreateTestMessage();
  std::string test_message_str;
  EXPECT_TRUE(MessageFramer::Serialize(test_message, &test_message_str));
  client_socket_factory()->AddWriteResultForData(net::SYNCHRONOUS,
                                                 test_message_str);

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();
  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());

  // Send the test message through a real transport object.
  EXPECT_CALL(handler_, OnWriteComplete(net::OK));
  socket_->transport()->SendMessage(
      test_message, base::BindOnce(&CompleteHandler::OnWriteComplete,
                                   base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());
}

TEST_F(MockCastSocketTest, TestObservers) {
  CreateCastSocketSecure();

  // Test adding observers.
  MockCastSocketObserver observer1;
  MockCastSocketObserver observer2;
  socket_->AddObserver(&observer1);
  socket_->AddObserver(&observer1);
  socket_->AddObserver(&observer2);
  socket_->AddObserver(&observer2);

  // Test notifying observers.
  EXPECT_CALL(observer1, OnError(_, cast_channel::ChannelError::CONNECT_ERROR));
  EXPECT_CALL(observer2, OnError(_, cast_channel::ChannelError::CONNECT_ERROR));
  CastSocketImpl::CastSocketMessageDelegate delegate(socket_.get());
  delegate.OnError(cast_channel::ChannelError::CONNECT_ERROR);

  // Finally, remove the observers to avoid the CheckedObserver CHECK.
  socket_->RemoveObserver(&observer1);
  socket_->RemoveObserver(&observer2);
}

TEST_F(MockCastSocketTest, TestOpenChannelConnectingSocket) {
  CreateCastSocketSecure();
  client_socket_factory()->SetupTcpConnectUnresponsive();
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get())).Times(2);
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  socket_->TriggerTimeout();
  RunPendingTasks();
}

TEST_F(MockCastSocketTest, TestOpenChannelConnectedSocket) {
  CreateCastSocketSecure();
  client_socket_factory()->SetupTcpConnect(net::ASYNC, net::OK);
  client_socket_factory()->SetupSslConnect(net::ASYNC, net::OK);
  HandleAuthHandshake();

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
}

TEST_F(MockCastSocketTest, TestOpenChannelClosedSocket) {
  CreateCastSocketSecure();
  client_socket_factory()->SetupTcpConnect(net::ASYNC, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  socket_->Connect(base::BindOnce(&CompleteHandler::OnConnectComplete,
                                  base::Unretained(&handler_)));
}

// https://crbug.com/874491, flaky on Win and Mac
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_TestConnectEndToEndWithRealSSL \
  DISABLED_TestConnectEndToEndWithRealSSL
#else
#define MAYBE_TestConnectEndToEndWithRealSSL TestConnectEndToEndWithRealSSL
#endif
// Tests connecting through an actual non-mocked CastTransport object and
// non-mocked SSLClientSocket, testing the components in integration.
TEST_F(SslCastSocketTest, MAYBE_TestConnectEndToEndWithRealSSL) {
  CreateSockets();
  ConnectSockets();

  // Set low-level auth challenge expectations.
  CastMessage challenge = CreateAuthChallenge();
  std::string challenge_str;
  EXPECT_TRUE(MessageFramer::Serialize(challenge, &challenge_str));

  int challenge_buffer_length = challenge_str.size();
  auto challenge_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(challenge_buffer_length);
  int read = ReadExactLength(challenge_buffer.get(), challenge_buffer_length,
                             server_socket_.get());

  EXPECT_EQ(challenge_buffer_length, read);
  EXPECT_EQ(challenge_str,
            std::string(challenge_buffer->data(), challenge_buffer_length));

  // Set low-level auth reply expectations.
  CastMessage reply = CreateAuthReply();
  std::string reply_str;
  EXPECT_TRUE(MessageFramer::Serialize(reply, &reply_str));

  scoped_refptr<net::StringIOBuffer> reply_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(reply_str);
  int written = WriteExactLength(reply_buffer.get(), reply_buffer->size(),
                                 server_socket_.get());

  EXPECT_EQ(reply_buffer->size(), written);
  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());
}

// Sends message data through an actual non-mocked CastTransport object and
// non-mocked SSLClientSocket, testing the components in integration.
TEST_F(SslCastSocketTest, DISABLED_TestMessageEndToEndWithRealSSL) {
  CreateSockets();
  ConnectSockets();

  // Set low-level auth challenge expectations.
  CastMessage challenge = CreateAuthChallenge();
  std::string challenge_str;
  EXPECT_TRUE(MessageFramer::Serialize(challenge, &challenge_str));

  int challenge_buffer_length = challenge_str.size();
  auto challenge_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(challenge_buffer_length);

  int read = ReadExactLength(challenge_buffer.get(), challenge_buffer_length,
                             server_socket_.get());

  EXPECT_EQ(challenge_buffer_length, read);
  EXPECT_EQ(challenge_str,
            std::string(challenge_buffer->data(), challenge_buffer_length));

  // Set low-level auth reply expectations.
  CastMessage reply = CreateAuthReply();
  std::string reply_str;
  EXPECT_TRUE(MessageFramer::Serialize(reply, &reply_str));

  scoped_refptr<net::StringIOBuffer> reply_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(reply_str);
  int written = WriteExactLength(reply_buffer.get(), reply_buffer->size(),
                                 server_socket_.get());

  EXPECT_EQ(reply_buffer->size(), written);
  EXPECT_CALL(handler_, OnConnectComplete(socket_.get()));
  RunPendingTasks();

  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());

  // Send a test message through the ssl socket.
  CastMessage test_message = CreateTestMessage();
  std::string test_message_str;
  EXPECT_TRUE(MessageFramer::Serialize(test_message, &test_message_str));

  int test_message_length = test_message_str.size();
  auto test_message_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(test_message_length);

  EXPECT_CALL(handler_, OnWriteComplete(net::OK));
  socket_->transport()->SendMessage(
      test_message, base::BindOnce(&CompleteHandler::OnWriteComplete,
                                   base::Unretained(&handler_)));
  RunPendingTasks();

  read = ReadExactLength(test_message_buffer.get(), test_message_length,
                         server_socket_.get());

  EXPECT_EQ(test_message_length, read);
  EXPECT_EQ(test_message_str,
            std::string(test_message_buffer->data(), test_message_length));

  EXPECT_EQ(ReadyState::OPEN, socket_->ready_state());
  EXPECT_EQ(ChannelError::NONE, socket_->error_state());
}

}  // namespace cast_channel
