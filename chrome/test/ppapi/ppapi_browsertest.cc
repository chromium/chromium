// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/chrome_browser_main_extra_parts_nacl_deprecation.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "chrome/test/ppapi/ppapi_test.h"
#include "chrome/test/ppapi/ppapi_test_select_file_dialog_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/javascript_test_observer.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/common/constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/ssl/ssl_info.h"
#include "ppapi/shared_impl/test_utils.h"
#include "rlz/buildflags/buildflags.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/test/test_dns_util.h"
#include "services/network/test/test_network_context.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/printing/browser_printing_context_factory_for_test.h"
#include "printing/backend/test_print_backend.h"
#endif

using content::RenderViewHost;

// This macro finesses macro expansion to do what we want.
#define STRIP_PREFIXES(test_name) ppapi::StripTestPrefixes(#test_name)
// Turn the given token into a string. This allows us to use precompiler stuff
// to turn names into DISABLED_Foo, but still pass a string to RunTest.
#define STRINGIFY(test_name) #test_name
#define LIST_TEST(test_name) STRINGIFY(test_name) ","

// Use these macros to run the tests for a specific interface.
// Most interfaces should be tested with both macros.
#define TEST_PPAPI_OUT_OF_PROCESS(test_name) \
    IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTest(STRIP_PREFIXES(test_name)); \
    }

// Similar macros that test over HTTP.
#define TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(test_name) \
    IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }

// Similar macros that test with an SSL server.
#define TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(test_name) \
    IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
      RunTestWithSSLServer(STRIP_PREFIXES(test_name)); \
    }

// Disable all NaCl tests for --disable-nacl flag
#if !BUILDFLAG(ENABLE_NACL)
#define MAYBE_PPAPI_NACL(test_name) DISABLED_##test_name
#define MAYBE_PPAPI_PNACL(test_name) DISABLED_##test_name

#define TEST_PPAPI_NACL_NATIVE(test_name)
#define TEST_PPAPI_NACL(test_name)
#define TEST_PPAPI_NACL_DISALLOWED_SOCKETS(test_name)
#define TEST_PPAPI_NACL_WITH_SSL_SERVER(test_name)
#define TEST_PPAPI_NACL_SUBTESTS(test_name, run_statement)

#else

#define MAYBE_PPAPI_NACL(test_name) test_name
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || defined(ADDRESS_SANITIZER)
// http://crbug.com/633067, http://crbug.com/727989, http://crbug.com/1076806
#define MAYBE_PPAPI_PNACL(test_name) DISABLED_##test_name
#else
#define MAYBE_PPAPI_PNACL(test_name) test_name
#endif

// NaCl based PPAPI tests (direct-to-native NaCl only, no PNaCl)
#define TEST_PPAPI_NACL_NATIVE(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }

// NaCl based PPAPI tests
#define TEST_PPAPI_NACL(test_name)                                           \
  TEST_PPAPI_NACL_NATIVE(test_name)                                          \
  IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(test_name)) { \
    RunTestViaHTTP(STRIP_PREFIXES(test_name));                               \
  }

// NaCl based PPAPI tests
#define TEST_PPAPI_NACL_SUBTESTS(test_name, run_statement)                   \
  IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, test_name) { run_statement; }  \
  IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(test_name)) { \
    run_statement;                                                           \
  }

// NaCl based PPAPI tests with disallowed socket API
#define TEST_PPAPI_NACL_DISALLOWED_SOCKETS(test_name) \
    IN_PROC_BROWSER_TEST_F(PPAPINaClTestDisallowedSockets, test_name) { \
      RunTestViaHTTP(STRIP_PREFIXES(test_name)); \
    }

// NaCl based PPAPI tests with SSL server
#define TEST_PPAPI_NACL_WITH_SSL_SERVER(test_name)                           \
  IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, test_name) {                   \
    RunTestWithSSLServer(STRIP_PREFIXES(test_name));                         \
  }                                                                          \
  IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(test_name)) { \
    RunTestWithSSLServer(STRIP_PREFIXES(test_name));                         \
  }

#endif  // !BUILDFLAG(ENABLE_NACL)

//
// Interface tests.
//

TEST_PPAPI_NACL(Console)
TEST_PPAPI_NACL(Core)

// Non-NaCl TraceEvent tests are in content/test/ppapi/ppapi_browsertest.cc.
TEST_PPAPI_NACL(TraceEvent)

TEST_PPAPI_NACL(InputEvent)

// Graphics2D_Dev isn't supported in NaCl, only test the other interfaces
// TODO(jhorwich) Enable when Graphics2D_Dev interfaces are proxied in NaCl.
TEST_PPAPI_NACL(Graphics2D_InvalidResource)
TEST_PPAPI_NACL(Graphics2D_InvalidSize)
TEST_PPAPI_NACL(Graphics2D_Humongous)
TEST_PPAPI_NACL(Graphics2D_InitToZero)
TEST_PPAPI_NACL(Graphics2D_Describe)
TEST_PPAPI_NACL(Graphics2D_Paint)
TEST_PPAPI_NACL(Graphics2D_Scroll)
TEST_PPAPI_NACL(Graphics2D_Replace)
TEST_PPAPI_NACL(Graphics2D_Flush)
// TODO(crbug.com/40502125): Flaky on Ubuntu.
// TEST_PPAPI_NACL(Graphics2D_FlushOffscreenUpdate)
TEST_PPAPI_NACL(Graphics2D_BindNull)

TEST_PPAPI_OUT_OF_PROCESS(Graphics3D)
TEST_PPAPI_NACL(Graphics3D)

TEST_PPAPI_NACL(ImageData)

// TCPSocket and TCPSocketPrivate tests.
#define PPAPI_SOCKET_TEST(_test)                                         \
  IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, _test) {                 \
    RunTestViaHTTP(LIST_TEST(_test));                                    \
  }                                                                      \
  IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(_test)) { \
    RunTestViaHTTP(LIST_TEST(_test));                                    \
  }                                                                      \
  IN_PROC_BROWSER_TEST_F(PPAPINaClGLibcTest, MAYBE_GLIBC(_test)) {       \
    RunTestViaHTTP(LIST_TEST(_test));                                    \
  }                                                                      \
  IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(_test)) { \
    RunTestViaHTTP(LIST_TEST(_test));                                    \
  }

// Split tests into multiple tests, making it easier to isolate which tests are
// failing, and reducing chance of timeout.
PPAPI_SOCKET_TEST(TCPSocket_Connect)
PPAPI_SOCKET_TEST(TCPSocket_ReadWrite)
PPAPI_SOCKET_TEST(TCPSocket_SetOption)
PPAPI_SOCKET_TEST(TCPSocket_Backlog)
PPAPI_SOCKET_TEST(TCPSocket_Listen)
PPAPI_SOCKET_TEST(TCPSocket_Interface_1_0)
PPAPI_SOCKET_TEST(TCPSocket_UnexpectedCalls)

TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(TCPServerSocketPrivate_Listen)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(TCPServerSocketPrivate_Backlog)
TEST_PPAPI_NACL(TCPServerSocketPrivate_Listen)
TEST_PPAPI_NACL(TCPServerSocketPrivate_Backlog)

TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivate_Basic)
TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivate_ReadWrite)
TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivate_ReadWriteSSL)
TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivate_ConnectAddress)
TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivate_SetOption)
TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivate_LargeRead)

TEST_PPAPI_NACL_WITH_SSL_SERVER(TCPSocketPrivate_Basic)
TEST_PPAPI_NACL_WITH_SSL_SERVER(TCPSocketPrivate_ReadWrite)
TEST_PPAPI_NACL_WITH_SSL_SERVER(TCPSocketPrivate_ReadWriteSSL)
TEST_PPAPI_NACL_WITH_SSL_SERVER(TCPSocketPrivate_ConnectAddress)
TEST_PPAPI_NACL_WITH_SSL_SERVER(TCPSocketPrivate_SetOption)
TEST_PPAPI_NACL_WITH_SSL_SERVER(TCPSocketPrivate_LargeRead)

TEST_PPAPI_OUT_OF_PROCESS_WITH_SSL_SERVER(TCPSocketPrivateTrusted)

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, TCPSocketPrivateCrash_Resolve) {
  if (content::IsInProcessNetworkService())
    return;

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  IgnoreNetworkServiceCrashes();
  network_service_test->CrashOnResolveHost("crash.com");

  RunTestViaHTTP(STRIP_PREFIXES(TCPSocketPrivateCrash_Resolve));
}

namespace {

// Different types of TCPSocket failures to simulate. *Error means to keep the
// pipe alive while invoking the callback with an error code, and *PipeError
// means to close the pipe that the method was invoked on (not the pipe passed
// to the callback, if there was one) without invoking the callback.
//
// Note that closing a pipe after a successful operation isn't too interesting,
// as it looks just like closing the pipe on the next operation, since the
// message filter classes don't generally watch for pipe closure.
enum class TCPFailureType {
  // Makes creation calls for all socket types (server, connected, and bound)
  // close all Mojo pipes without doing anything.
  kClosePipeOnCreate,

  kBindClosePipe,
  kBindError,
  kBindHangs,

  // These apply to both CreateTCPServerSocket and TCPBoundSocket::Listen().
  kCreateTCPServerSocketClosePipe,
  kCreateTCPServerSocketError,
  kCreateTCPServerSocketHangs,

  kAcceptDropPipe,
  kAcceptError,
  kAcceptHangs,

  kConnectClosePipe,
  kConnectError,
  kConnectHangs,
  kWriteClosePipe,
  kWriteError,
  kReadClosePipe,
  kReadError,

  // These apply to all TCPConnectedSocket configuration methods.
  kSetOptionsClosePipe,
  kSetOptionsError,

  kUpgradeToTLSClosePipe,
  kUpgradeToTLSError,
  kUpgradeToTLSHangs,
  kSSLWriteClosePipe,
  kSSLWriteError,
  kSSLReadClosePipe,
  kSSLReadError,
};

net::IPEndPoint LocalAddress() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), 1234);
}

net::IPEndPoint RemoteAddress() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), 12345);
}

// Use the same class for TCPConnectedSocket and, if it's upgraded,
// TLSClientSocket, since the TLSClientSocket interface doesn't do anything.
class MockTCPConnectedSocket : public network::mojom::TCPConnectedSocket,
                               public network::mojom::TLSClientSocket {
 public:
  MockTCPConnectedSocket(
      TCPFailureType tcp_failure_type,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      network::mojom::NetworkContext::CreateTCPConnectedSocketCallback callback)
      : tcp_failure_type_(tcp_failure_type),
        observer_(std::move(observer)),
        receiver_(this, std::move(receiver)) {
    if (tcp_failure_type_ == TCPFailureType::kConnectError) {
      std::move(callback).Run(
          net::ERR_FAILED, std::nullopt /* local_addr */,
          std::nullopt /* peer_addr */,
          mojo::ScopedDataPipeConsumerHandle() /* receive_stream */,
          mojo::ScopedDataPipeProducerHandle() /* send_stream */);
      return;
    }

    if (tcp_failure_type_ == TCPFailureType::kConnectHangs) {
      create_connected_socket_callback_ = std::move(callback);
      return;
    }

    mojo::ScopedDataPipeProducerHandle send_producer_handle;
    EXPECT_EQ(
        mojo::CreateDataPipe(nullptr, send_producer_handle, send_pipe_handle_),
        MOJO_RESULT_OK);

    mojo::ScopedDataPipeConsumerHandle receive_consumer_handle;
    EXPECT_EQ(mojo::CreateDataPipe(nullptr, receive_pipe_handle_,
                                   receive_consumer_handle),
              MOJO_RESULT_OK);

    std::move(callback).Run(net::OK, LocalAddress(), RemoteAddress(),
                            std::move(receive_consumer_handle),
                            std::move(send_producer_handle));
    ClosePipeIfNeeded();
  }

  MockTCPConnectedSocket(
      TCPFailureType tcp_failure_type,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      network::mojom::TCPServerSocket::AcceptCallback callback)
      : tcp_failure_type_(tcp_failure_type),
        observer_(std::move(observer)),
        receiver_(this) {
    if (tcp_failure_type_ == TCPFailureType::kAcceptError) {
      std::move(callback).Run(
          net::ERR_FAILED, std::nullopt /* remote_addr */,
          mojo::NullRemote() /* connected_socket */,
          mojo::ScopedDataPipeConsumerHandle() /* receive_stream */,
          mojo::ScopedDataPipeProducerHandle() /* send_stream */);
      return;
    }

    if (tcp_failure_type_ == TCPFailureType::kAcceptHangs) {
      accept_callback_ = std::move(callback);
      return;
    }

    mojo::ScopedDataPipeProducerHandle send_producer_handle;
    EXPECT_EQ(
        mojo::CreateDataPipe(nullptr, send_producer_handle, send_pipe_handle_),
        MOJO_RESULT_OK);

    mojo::ScopedDataPipeConsumerHandle receive_consumer_handle;
    EXPECT_EQ(mojo::CreateDataPipe(nullptr, receive_pipe_handle_,
                                   receive_consumer_handle),
              MOJO_RESULT_OK);

    std::move(callback).Run(
        net::OK, RemoteAddress(), receiver_.BindNewPipeAndPassRemote(),
        std::move(receive_consumer_handle), std::move(send_producer_handle));
    ClosePipeIfNeeded();
  }

  MockTCPConnectedSocket(const MockTCPConnectedSocket&) = delete;
  MockTCPConnectedSocket& operator=(const MockTCPConnectedSocket&) = delete;

  ~MockTCPConnectedSocket() override {}

  // mojom::TCPConnectedSocket implementation:

  void UpgradeToTLS(
      const net::HostPortPair& host_port_pair,
      network::mojom::TLSClientSocketOptionsPtr socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TLSClientSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      network::mojom::TCPConnectedSocket::UpgradeToTLSCallback callback)
      override {
    observer_.reset();

    // Succeed or fail, keep these pipes open (Their state shouldn't matter when
    // checking for failures).
    observer_.Bind(std::move(observer));
    tls_client_socket_receiver_.Bind(std::move(receiver));

    if (tcp_failure_type_ == TCPFailureType::kUpgradeToTLSClosePipe) {
      receiver_.reset();
      return;
    }
    if (tcp_failure_type_ == TCPFailureType::kUpgradeToTLSError) {
      std::move(callback).Run(
          net::ERR_FAILED, mojo::ScopedDataPipeConsumerHandle(),
          mojo::ScopedDataPipeProducerHandle(), std::nullopt /* ssl_info */);
      return;
    }

    if (tcp_failure_type_ == TCPFailureType::kUpgradeToTLSHangs) {
      upgrade_to_tls_callback_ = std::move(callback);
      return;
    }

    // Invoke callback immediately, without waiting for pipes to close - tests
    // that use a real NetworkContext already make sure that the class correctly
    // closes the sockets when upgrading.

    mojo::ScopedDataPipeProducerHandle send_producer_handle;
    EXPECT_EQ(
        mojo::CreateDataPipe(nullptr, send_producer_handle, send_pipe_handle_),
        MOJO_RESULT_OK);

    mojo::ScopedDataPipeConsumerHandle receive_consumer_handle;
    EXPECT_EQ(mojo::CreateDataPipe(nullptr, receive_pipe_handle_,
                                   receive_consumer_handle),
              MOJO_RESULT_OK);

    std::move(callback).Run(net::OK, std::move(receive_consumer_handle),
                            std::move(send_producer_handle), net::SSLInfo());

    if (tcp_failure_type_ == TCPFailureType::kSSLWriteClosePipe) {
      observer_.reset();
      send_pipe_handle_.reset();
    } else if (tcp_failure_type_ == TCPFailureType::kSSLWriteError) {
      observer_->OnWriteError(net::ERR_FAILED);
      send_pipe_handle_.reset();
    } else if (tcp_failure_type_ == TCPFailureType::kSSLReadClosePipe) {
      observer_.reset();
      receive_pipe_handle_.reset();
    } else if (tcp_failure_type_ == TCPFailureType::kSSLReadError) {
      observer_->OnReadError(net::ERR_FAILED);
      receive_pipe_handle_.reset();
    }
  }

  void SetSendBufferSize(int send_buffer_size,
                         SetSendBufferSizeCallback callback) override {
    if (tcp_failure_type_ == TCPFailureType::kSetOptionsClosePipe) {
      receiver_.reset();
      return;
    }
    DCHECK_EQ(tcp_failure_type_, TCPFailureType::kSetOptionsError);
    std::move(callback).Run(net::ERR_FAILED);
  }

  void SetReceiveBufferSize(int send_buffer_size,
                            SetSendBufferSizeCallback callback) override {
    if (tcp_failure_type_ == TCPFailureType::kSetOptionsClosePipe) {
      receiver_.reset();
      return;
    }
    DCHECK_EQ(tcp_failure_type_, TCPFailureType::kSetOptionsError);
    std::move(callback).Run(net::ERR_FAILED);
  }

  void SetNoDelay(bool no_delay, SetNoDelayCallback callback) override {
    if (tcp_failure_type_ == TCPFailureType::kSetOptionsClosePipe) {
      receiver_.reset();
      return;
    }
    DCHECK_EQ(tcp_failure_type_, TCPFailureType::kSetOptionsError);
    std::move(callback).Run(false);
  }

  void SetKeepAlive(bool enable,
                    int32_t delay_secs,
                    SetKeepAliveCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  void ClosePipeIfNeeded() {
    if (tcp_failure_type_ == TCPFailureType::kWriteClosePipe) {
      observer_.reset();
      send_pipe_handle_.reset();
    } else if (tcp_failure_type_ == TCPFailureType::kWriteError) {
      observer_->OnWriteError(net::ERR_FAILED);
      send_pipe_handle_.reset();
    } else if (tcp_failure_type_ == TCPFailureType::kReadClosePipe) {
      observer_.reset();
      receive_pipe_handle_.reset();
    } else if (tcp_failure_type_ == TCPFailureType::kReadError) {
      observer_->OnReadError(net::ERR_FAILED);
      receive_pipe_handle_.reset();
    }
  }

  const TCPFailureType tcp_failure_type_;

  mojo::Remote<network::mojom::SocketObserver> observer_;

  // Callbacks held onto when simulating a hang.
  network::mojom::NetworkContext::CreateTCPConnectedSocketCallback
      create_connected_socket_callback_;
  network::mojom::TCPServerSocket::AcceptCallback accept_callback_;
  network::mojom::TCPConnectedSocket::UpgradeToTLSCallback
      upgrade_to_tls_callback_;

  mojo::ScopedDataPipeProducerHandle receive_pipe_handle_;
  mojo::ScopedDataPipeConsumerHandle send_pipe_handle_;

  mojo::Receiver<network::mojom::TCPConnectedSocket> receiver_;
  mojo::Receiver<network::mojom::TLSClientSocket> tls_client_socket_receiver_{
      this};
};

class MockTCPServerSocket : public network::mojom::TCPServerSocket {
 public:
  // CreateTCPServerSocket constructor.
  MockTCPServerSocket(
      TCPFailureType tcp_failure_type,
      mojo::PendingReceiver<network::mojom::TCPServerSocket> receiver,
      network::mojom::NetworkContext::CreateTCPServerSocketCallback callback)
      : tcp_failure_type_(tcp_failure_type),
        receiver_(this, std::move(receiver)) {
    if (tcp_failure_type_ == TCPFailureType::kCreateTCPServerSocketError) {
      std::move(callback).Run(net::ERR_FAILED, std::nullopt /* local_addr */);
      return;
    }
    if (tcp_failure_type_ == TCPFailureType::kCreateTCPServerSocketHangs) {
      create_server_socket_callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(net::OK, LocalAddress());
  }

  // TCPBoundSocket::Listen constructor.
  MockTCPServerSocket(
      TCPFailureType tcp_failure_type,
      mojo::PendingReceiver<network::mojom::TCPServerSocket> receiver,
      network::mojom::TCPBoundSocket::ListenCallback callback)
      : tcp_failure_type_(tcp_failure_type),
        receiver_(this, std::move(receiver)) {
    if (tcp_failure_type_ == TCPFailureType::kCreateTCPServerSocketError) {
      std::move(callback).Run(net::ERR_FAILED);
      return;
    }
    if (tcp_failure_type_ == TCPFailureType::kCreateTCPServerSocketHangs) {
      listen_callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(net::OK);
  }

  MockTCPServerSocket(const MockTCPServerSocket&) = delete;
  MockTCPServerSocket& operator=(const MockTCPServerSocket&) = delete;

  ~MockTCPServerSocket() override {}

  // TCPServerSocket implementation:
  void Accept(mojo::PendingRemote<network::mojom::SocketObserver> observer,
              AcceptCallback callback) override {
    // This falls through just to keep the observer alive.
    if (tcp_failure_type_ == TCPFailureType::kAcceptDropPipe)
      receiver_.reset();
    connected_socket_ = std::make_unique<MockTCPConnectedSocket>(
        tcp_failure_type_, std::move(observer), std::move(callback));
  }

 private:
  const TCPFailureType tcp_failure_type_;

  std::unique_ptr<MockTCPConnectedSocket> connected_socket_;

  // Callbacks held onto when simulating a hang.
  network::mojom::NetworkContext::CreateTCPServerSocketCallback
      create_server_socket_callback_;
  network::mojom::TCPBoundSocket::ListenCallback listen_callback_;

  mojo::Receiver<network::mojom::TCPServerSocket> receiver_;
};

class MockTCPBoundSocket : public network::mojom::TCPBoundSocket {
 public:
  MockTCPBoundSocket(
      TCPFailureType tcp_failure_type,
      mojo::PendingReceiver<network::mojom::TCPBoundSocket> receiver,
      network::mojom::NetworkContext::CreateTCPBoundSocketCallback callback)
      : tcp_failure_type_(tcp_failure_type),
        receiver_(this, std::move(receiver)) {
    if (tcp_failure_type_ == TCPFailureType::kBindError) {
      std::move(callback).Run(net::ERR_FAILED, std::nullopt /* local_addr */);
      return;
    }
    if (tcp_failure_type_ == TCPFailureType::kBindHangs) {
      callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(net::OK, LocalAddress());
  }

  MockTCPBoundSocket(const MockTCPBoundSocket&) = delete;
  MockTCPBoundSocket& operator=(const MockTCPBoundSocket&) = delete;

  ~MockTCPBoundSocket() override {}

  // mojom::TCPBoundSocket implementation:
  void Listen(uint32_t backlog,
              mojo::PendingReceiver<network::mojom::TCPServerSocket> receiver,
              ListenCallback callback) override {
    // If closing the pipe, create ServerSocket anyways, to keep |receiver|
    // alive. The callback invocation will have no effect, since it uses the
    // TCPBoundSocket's pipe, which was just closed.
    if (tcp_failure_type_ == TCPFailureType::kCreateTCPServerSocketClosePipe)
      receiver_.reset();
    server_socket_ = std::make_unique<MockTCPServerSocket>(
        tcp_failure_type_, std::move(receiver), std::move(callback));
  }

  void Connect(
      const net::AddressList& remote_addr,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      ConnectCallback callback) override {
    if (tcp_failure_type_ == TCPFailureType::kConnectClosePipe)
      receiver_.reset();
    connected_socket_ = std::make_unique<MockTCPConnectedSocket>(
        tcp_failure_type_, std::move(receiver), std::move(observer),
        std::move(callback));
  }

 private:
  const TCPFailureType tcp_failure_type_;

  // Needs to be destroyed after |receiver_|, as it may be holding onto a
  // callback bound to the Receiver.
  std::unique_ptr<MockTCPServerSocket> server_socket_;
  std::unique_ptr<MockTCPConnectedSocket> connected_socket_;

  // Callback held onto when simulating a hang.
  network::mojom::NetworkContext::CreateTCPBoundSocketCallback callback_;

  mojo::Receiver<network::mojom::TCPBoundSocket> receiver_;
};

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(
      TCPFailureType tcp_failure_type,
      Browser* browser,
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver)
      : tcp_failure_type_(tcp_failure_type),
        browser_(browser),
        receiver_(this, std::move(receiver)) {}

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

  ~MockNetworkContext() override {}

  // network::mojom::NetworkContext implementation:

  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      network::mojom::TCPServerSocketOptionsPtr options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPServerSocket> receiver,
      CreateTCPServerSocketCallback callback) override {
    // If closing the pipe, create ServerSocket anyways, to keep |receiver|
    // alive. The callback invocation will have no effect, since it uses the
    // TCPBoundSocket's pipe, which was just closed.
    if (tcp_failure_type_ == TCPFailureType::kCreateTCPServerSocketClosePipe)
      receiver_.reset();
    server_sockets_.emplace_back(std::make_unique<MockTCPServerSocket>(
        tcp_failure_type_, std::move(receiver), std::move(callback)));
  }

  void CreateTCPConnectedSocket(
      const std::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override {
    if (tcp_failure_type_ == TCPFailureType::kConnectClosePipe)
      receiver_.reset();
    connected_sockets_.emplace_back(std::make_unique<MockTCPConnectedSocket>(
        tcp_failure_type_, std::move(socket), std::move(observer),
        std::move(callback)));
  }

  void CreateTCPBoundSocket(
      const net::IPEndPoint& local_addr,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPBoundSocket> receiver,
      CreateTCPBoundSocketCallback callback) override {
    if (tcp_failure_type_ == TCPFailureType::kBindClosePipe)
      receiver_.reset();
    // These tests only create at most one object of a given type at a time.
    bound_sockets_.emplace_back(std::make_unique<MockTCPBoundSocket>(
        tcp_failure_type_, std::move(receiver), std::move(callback)));
  }

  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client) override {
    EXPECT_EQ(browser_->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetPrimaryMainFrame()
                  ->GetIsolationInfoForSubresources()
                  .network_anonymization_key(),
              network_anonymization_key);
    mojo::Remote<network::mojom::ResolveHostClient> response_client(
        std::move(pending_response_client));
    response_client->OnComplete(
        net::OK, net::ResolveErrorInfo(net::OK),
        net::AddressList(LocalAddress()),
        /*endpoint_results_with_metadata=*/std::nullopt);
  }

 private:
  TCPFailureType tcp_failure_type_;
  Browser* const browser_;

  std::vector<std::unique_ptr<MockTCPServerSocket>> server_sockets_;
  std::vector<std::unique_ptr<MockTCPBoundSocket>> bound_sockets_;
  std::vector<std::unique_ptr<MockTCPConnectedSocket>> connected_sockets_;

  mojo::Receiver<network::mojom::NetworkContext> receiver_;
};

// Runs a TCP test using a MockNetworkContext, through a Mojo pipe. Using a Mojo
// pipe makes sure that everything happens asynchronously through a pipe.
#define RUN_TCP_FAILURE_TEST(test_name, failure_type)                         \
  do {                                                                        \
    mojo::Remote<network::mojom::NetworkContext> network_context_proxy;       \
    MockNetworkContext network_context(                                       \
        failure_type, browser(),                                              \
        network_context_proxy.BindNewPipeAndPassReceiver());                  \
    ppapi::SetPepperTCPNetworkContextForTesting(network_context_proxy.get()); \
    RunTestViaHTTP(LIST_TEST(test_name));                                     \
    ppapi::SetPepperTCPNetworkContextForTesting(nullptr);                     \
  } while (false)

}  // namespace

// Macro for tests that use |WrappedUDPSocket| to simulate errors. |test_name|
// and |_test| are separate values because there are often multiple ways to get
// the same error pattern (Dropped mojo pipe and failed call, generally).
#define TCP_SOCKET_FAILURE_TEST(test_name, _test, failure_type)              \
  IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) {                 \
    RUN_TCP_FAILURE_TEST(_test, failure_type);                               \
  }                                                                          \
  IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(test_name)) { \
    RUN_TCP_FAILURE_TEST(_test, failure_type);                               \
  }                                                                          \
  IN_PROC_BROWSER_TEST_F(PPAPINaClGLibcTest, MAYBE_GLIBC(test_name)) {       \
    RUN_TCP_FAILURE_TEST(_test, failure_type);                               \
  }                                                                          \
  IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(test_name)) { \
    RUN_TCP_FAILURE_TEST(_test, failure_type);                               \
  }

TCP_SOCKET_FAILURE_TEST(TCPSocket_ConnectClosePipe,
                        TCPSocket_ConnectFails,
                        TCPFailureType::kConnectClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_ConnectError,
                        TCPSocket_ConnectFails,
                        TCPFailureType::kConnectError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_ConnectHangs,
                        TCPSocket_ConnectHangs,
                        TCPFailureType::kConnectHangs)
TCP_SOCKET_FAILURE_TEST(TCPSocket_WriteClosePipe,
                        TCPSocket_WriteFails,
                        TCPFailureType::kWriteClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_WriteError,
                        TCPSocket_WriteFails,
                        TCPFailureType::kWriteError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_ReadClosePipe,
                        TCPSocket_ReadFails,
                        TCPFailureType::kReadClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_ReadError,
                        TCPSocket_ReadFails,
                        TCPFailureType::kReadError)

TCP_SOCKET_FAILURE_TEST(TCPSocket_SetSendBufferSizeClosePipe,
                        TCPSocket_SetSendBufferSizeFails,
                        TCPFailureType::kSetOptionsClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_SetSendBufferSizeError,
                        TCPSocket_SetSendBufferSizeFails,
                        TCPFailureType::kSetOptionsError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_SetReceiveBufferSizeClosePipe,
                        TCPSocket_SetReceiveBufferSizeFails,
                        TCPFailureType::kSetOptionsClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_SetReceiveBufferSizeError,
                        TCPSocket_SetReceiveBufferSizeFails,
                        TCPFailureType::kSetOptionsError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_SetNoDelayClosePipe,
                        TCPSocket_SetNoDelayFails,
                        TCPFailureType::kSetOptionsClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_SetNoDelayError,
                        TCPSocket_SetNoDelayFails,
                        TCPFailureType::kSetOptionsError)

// Can't use TCPSocket_BindFailsConnectSucceeds for this one, because
// BindClosePipe has to close the NetworkContext pipe.
TCP_SOCKET_FAILURE_TEST(TCPSocket_BindClosePipe,
                        TCPSocket_BindFails,
                        TCPFailureType::kBindClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_BindError,
                        TCPSocket_BindFailsConnectSucceeds,
                        TCPFailureType::kBindError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_BindHangs,
                        TCPSocket_BindHangs,
                        TCPFailureType::kBindHangs)
// https://crbug.com/997840. Flaky.
TCP_SOCKET_FAILURE_TEST(DISABLED_TCPSocket_ListenClosePipe,
                        TCPSocket_ListenFails,
                        TCPFailureType::kCreateTCPServerSocketClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_ListenError,
                        TCPSocket_ListenFails,
                        TCPFailureType::kCreateTCPServerSocketError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_ListenHangs,
                        TCPSocket_ListenHangs,
                        TCPFailureType::kCreateTCPServerSocketHangs)
TCP_SOCKET_FAILURE_TEST(TCPSocket_AcceptClosePipe,
                        TCPSocket_AcceptFails,
                        TCPFailureType::kAcceptDropPipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_AcceptError,
                        TCPSocket_AcceptFails,
                        TCPFailureType::kAcceptError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_AcceptHangs,
                        TCPSocket_AcceptHangs,
                        TCPFailureType::kAcceptHangs)
TCP_SOCKET_FAILURE_TEST(TCPSocket_AcceptedSocketWriteClosePipe,
                        TCPSocket_AcceptedSocketWriteFails,
                        TCPFailureType::kWriteClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_AcceptedSocketWriteError,
                        TCPSocket_AcceptedSocketWriteFails,
                        TCPFailureType::kWriteError)
// https://crbug.com/997840. Flaky.
TCP_SOCKET_FAILURE_TEST(DISABLED_TCPSocket_AcceptedSocketReadClosePipe,
                        TCPSocket_AcceptedSocketReadFails,
                        TCPFailureType::kReadClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_AcceptedSocketReadError,
                        TCPSocket_AcceptedSocketReadFails,
                        TCPFailureType::kReadError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_BindConnectClosePipe,
                        TCPSocket_BindConnectFails,
                        TCPFailureType::kConnectClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocket_BindConnectError,
                        TCPSocket_BindConnectFails,
                        TCPFailureType::kConnectError)
TCP_SOCKET_FAILURE_TEST(TCPSocket_BindConnectHangs,
                        TCPSocket_BindConnectHangs,
                        TCPFailureType::kConnectHangs)

TCP_SOCKET_FAILURE_TEST(TCPSocketPrivate_SSLHandshakeClosePipe,
                        TCPSocketPrivate_SSLHandshakeFails,
                        TCPFailureType::kUpgradeToTLSClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocketPrivate_SSLHandshakeError,
                        TCPSocketPrivate_SSLHandshakeFails,
                        TCPFailureType::kUpgradeToTLSError)
TCP_SOCKET_FAILURE_TEST(TCPSocketPrivate_SSLHandshakeHangs,
                        TCPSocketPrivate_SSLHandshakeHangs,
                        TCPFailureType::kUpgradeToTLSHangs)
TCP_SOCKET_FAILURE_TEST(TCPSocketPrivate_SSLWriteClosePipe,
                        TCPSocketPrivate_SSLWriteFails,
                        TCPFailureType::kSSLWriteClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocketPrivate_SSLWriteError,
                        TCPSocketPrivate_SSLWriteFails,
                        TCPFailureType::kSSLWriteError)
TCP_SOCKET_FAILURE_TEST(TCPSocketPrivate_SSLReadClosePipe,
                        TCPSocketPrivate_SSLReadFails,
                        TCPFailureType::kSSLReadClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPSocketPrivate_SSLReadError,
                        TCPSocketPrivate_SSLReadFails,
                        TCPFailureType::kSSLReadError)

TCP_SOCKET_FAILURE_TEST(TCPServerSocketPrivate_ListenClosePipe,
                        TCPServerSocketPrivate_ListenFails,
                        TCPFailureType::kCreateTCPServerSocketClosePipe)
TCP_SOCKET_FAILURE_TEST(TCPServerSocketPrivate_ListenError,
                        TCPServerSocketPrivate_ListenFails,
                        TCPFailureType::kCreateTCPServerSocketError)
TCP_SOCKET_FAILURE_TEST(TCPServerSocketPrivate_ListenHangs,
                        TCPServerSocketPrivate_ListenHangs,
                        TCPFailureType::kCreateTCPServerSocketHangs)
TCP_SOCKET_FAILURE_TEST(TCPServerSocketPrivate_AcceptClosePipe,
                        TCPServerSocketPrivate_AcceptFails,
                        TCPFailureType::kAcceptDropPipe)
TCP_SOCKET_FAILURE_TEST(TCPServerSocketPrivate_AcceptError,
                        TCPServerSocketPrivate_AcceptFails,
                        TCPFailureType::kAcceptError)
TCP_SOCKET_FAILURE_TEST(TCPServerSocketPrivate_AcceptHangs,
                        TCPServerSocketPrivate_AcceptHangs,
                        TCPFailureType::kAcceptHangs)

// UDPSocket tests.

// Split tests into multiple tests, making it easier to isolate which tests are
// failing.
PPAPI_SOCKET_TEST(UDPSocket_ReadWrite)
PPAPI_SOCKET_TEST(UDPSocket_SetOption)
PPAPI_SOCKET_TEST(UDPSocket_SetOption_1_0)
PPAPI_SOCKET_TEST(UDPSocket_SetOption_1_1)

// Fails on MacOS 11, crbug.com/1211138.
#if !BUILDFLAG(IS_MAC)
PPAPI_SOCKET_TEST(UDPSocket_Broadcast)
#endif

PPAPI_SOCKET_TEST(UDPSocket_ParallelSend)
PPAPI_SOCKET_TEST(UDPSocket_Multicast)

// UDPSocketPrivate tests.
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(UDPSocketPrivate_Connect)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(UDPSocketPrivate_ConnectFailure)

// Fails on MacOS 11, crbug.com/1211138.
#if !BUILDFLAG(IS_MAC)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(UDPSocketPrivate_Broadcast)
#endif

TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(UDPSocketPrivate_SetSocketFeatureErrors)
TEST_PPAPI_NACL(UDPSocketPrivate_Connect)
TEST_PPAPI_NACL(UDPSocketPrivate_ConnectFailure)

// Fails on MacOS 11, crbug.com/1211138.
#if !BUILDFLAG(IS_MAC)
TEST_PPAPI_NACL(UDPSocketPrivate_Broadcast)
#endif

TEST_PPAPI_NACL(UDPSocketPrivate_SetSocketFeatureErrors)

namespace {

// UDPSocket subclass that wraps a real network::mojom::UDPSocket, and can
// simulate certain failures. Owns itself, and destroys itself when one of
// its Mojo pipes is closed.
class WrappedUDPSocket : public network::mojom::UDPSocket {
 public:
  // Type of failure to simulate. "DropPipe" failures correspond to dropping a
  // Mojo pipe (Which typically happens if the network service crashes, or the
  // parent NetworkContext is torn down). "Error" failures correspond to
  // returning net::ERR_FAILED.
  enum class FailureType {
    kBindError,
    kBindDropPipe,
    kBroadcastError,
    kBroadcastDropPipe,
    kSendToDropPipe,
    kSendToError,
    kDropListenerPipeOnConstruction,
    kDropListenerPipeOnReceiveMore,
    kReadError,
  };
  WrappedUDPSocket(
      FailureType failure_type,
      network::mojom::NetworkContext* network_context,
      mojo::PendingReceiver<network::mojom::UDPSocket> socket_receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> socket_listener)
      : failure_type_(failure_type),
        receiver_(this, std::move(socket_receiver)) {
    if (failure_type == FailureType::kDropListenerPipeOnConstruction)
      socket_listener.reset();
    else
      socket_listener_.Bind(std::move(socket_listener));
    network_context->CreateUDPSocket(
        wrapped_socket_.BindNewPipeAndPassReceiver(), mojo::NullRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&WrappedUDPSocket::Close, base::Unretained(this)));
    wrapped_socket_.set_disconnect_handler(
        base::BindOnce(&WrappedUDPSocket::Close, base::Unretained(this)));
  }

  WrappedUDPSocket(const WrappedUDPSocket&) = delete;
  WrappedUDPSocket& operator=(const WrappedUDPSocket&) = delete;

  // network::mojom::UDPSocket implementation.
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr options,
               ConnectCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  void Bind(const net::IPEndPoint& local_addr,
            network::mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override {
    if (failure_type_ == FailureType::kBindError) {
      std::move(callback).Run(net::ERR_FAILED, std::nullopt);
      return;
    }
    if (failure_type_ == FailureType::kBindDropPipe) {
      Close();
      return;
    }
    wrapped_socket_->Bind(local_addr, std::move(options), std::move(callback));
  }
  void SetBroadcast(bool broadcast, SetBroadcastCallback callback) override {
    if (failure_type_ == FailureType::kBroadcastError) {
      std::move(callback).Run(net::ERR_FAILED);
      return;
    }
    if (failure_type_ == FailureType::kBroadcastDropPipe) {
      Close();
      return;
    }
    wrapped_socket_->SetBroadcast(broadcast, std::move(callback));
  }
  void SetSendBufferSize(int32_t send_buffer_size,
                         SetSendBufferSizeCallback callback) override {
    wrapped_socket_->SetSendBufferSize(send_buffer_size, std::move(callback));
  }
  void SetReceiveBufferSize(int32_t receive_buffer_size,
                            SetReceiveBufferSizeCallback callback) override {
    wrapped_socket_->SetReceiveBufferSize(receive_buffer_size,
                                          std::move(callback));
  }
  void JoinGroup(const net::IPAddress& group_address,
                 JoinGroupCallback callback) override {
    wrapped_socket_->JoinGroup(group_address, std::move(callback));
  }
  void LeaveGroup(const net::IPAddress& group_address,
                  LeaveGroupCallback callback) override {
    wrapped_socket_->LeaveGroup(group_address, std::move(callback));
  }
  void ReceiveMore(uint32_t num_additional_datagrams) override {
    if (failure_type_ == FailureType::kDropListenerPipeOnReceiveMore) {
      socket_listener_.reset();
      return;
    }
    if (failure_type_ == FailureType::kReadError) {
      for (uint32_t i = 0; i < num_additional_datagrams; ++i) {
        socket_listener_->OnReceived(net::ERR_FAILED, std::nullopt,
                                     std::nullopt);
      }
      return;
    }
    // None of the tests using this fixture expect to read anything
    // successfully, so just ignore this call if it isn't supposed to result in
    // an error of some sort.
  }
  void ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                 uint32_t buffer_size) override {
    NOTREACHED_IN_MIGRATION();
  }
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override {
    if (failure_type_ == FailureType::kSendToError) {
      std::move(callback).Run(net::ERR_FAILED);
      return;
    }
    if (failure_type_ == FailureType::kSendToDropPipe) {
      Close();
      return;
    }
    wrapped_socket_->SendTo(dest_addr, data, traffic_annotation,
                            std::move(callback));
  }
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  void Close() override {
    // Deleting |this| before closing the bindings can cause Mojo to DCHECK if
    // there's a pending callback.
    receiver_.reset();
    socket_listener_.reset();
    delete this;
  }

 private:
  const FailureType failure_type_;
  mojo::Receiver<network::mojom::UDPSocket> receiver_;
  mojo::Remote<network::mojom::UDPSocket> wrapped_socket_;

  // Only populated on certain read FailureTypes.
  mojo::Remote<network::mojom::UDPSocketListener> socket_listener_;
};

void TestCreateUDPSocketCallback(
    WrappedUDPSocket::FailureType failure_type,
    network::mojom::NetworkContext* network_context,
    mojo::PendingReceiver<network::mojom::UDPSocket> socket_receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> socket_listener) {
  // This will delete itself when one of its Mojo pipes is closed.
  new WrappedUDPSocket(failure_type, network_context,
                       std::move(socket_receiver), std::move(socket_listener));
}

#define RUN_UDP_FAILURE_TEST(test_name, failure_type)                    \
  do {                                                                   \
    auto callback =                                                      \
        base::BindRepeating(&TestCreateUDPSocketCallback, failure_type); \
    ppapi::SetPepperUDPSocketCallackForTesting(&callback);               \
    RunTestViaHTTP(LIST_TEST(test_name));                                \
    ppapi::SetPepperUDPSocketCallackForTesting(nullptr);                 \
  } while (false)

}  // namespace

// Macro for tests that use |WrappedUDPSocket| to simulate errors. |test_name|
// and |_test| are separate values because there are often multiple ways to get
// the same error pattern (Dropped mojo pipe and failed call, generally).
#define UDPSOCKET_FAILURE_TEST(test_name, _test, failure_type)               \
  IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) {                 \
    RUN_UDP_FAILURE_TEST(_test, failure_type);                               \
  }                                                                          \
  IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(test_name)) { \
    RUN_UDP_FAILURE_TEST(_test, failure_type);                               \
  }                                                                          \
  IN_PROC_BROWSER_TEST_F(PPAPINaClGLibcTest, MAYBE_GLIBC(test_name)) {       \
    RUN_UDP_FAILURE_TEST(_test, failure_type);                               \
  }                                                                          \
  IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(test_name)) { \
    RUN_UDP_FAILURE_TEST(_test, failure_type);                               \
  }

UDPSOCKET_FAILURE_TEST(UDPSocket_BindError,
                       UDPSocket_BindFails,
                       WrappedUDPSocket::FailureType::kBindError)
UDPSOCKET_FAILURE_TEST(UDPSocket_BindDropPipe,
                       UDPSocket_BindFails,
                       WrappedUDPSocket::FailureType::kBindDropPipe)
UDPSOCKET_FAILURE_TEST(UDPSocket_SetBroadcastError,
                       UDPSocket_SetBroadcastFails,
                       WrappedUDPSocket::FailureType::kBroadcastError)
UDPSOCKET_FAILURE_TEST(UDPSocket_SetBroadcastDropPipe,
                       UDPSocket_SetBroadcastFails,
                       WrappedUDPSocket::FailureType::kBroadcastDropPipe)
UDPSOCKET_FAILURE_TEST(UDPSocket_SendToBeforeDropPipeFails,
                       UDPSocket_SendToFails,
                       WrappedUDPSocket::FailureType::kSendToDropPipe)
UDPSOCKET_FAILURE_TEST(UDPSocket_DropPipeAfterBindSendToFails,
                       UDPSocket_SendToFails,
                       WrappedUDPSocket::FailureType::kSendToError)
// https://crbug.com/997840. Flaky.
UDPSOCKET_FAILURE_TEST(DISABLED_UDPSocket_ReadError,
                       UDPSocket_ReadFails,
                       WrappedUDPSocket::FailureType::kReadError)
// https://crbug.com/997840. Flaky.
UDPSOCKET_FAILURE_TEST(
    DISABLED_UDPSocket_DropListenerPipeOnConstruction,
    UDPSocket_ReadFails,
    WrappedUDPSocket::FailureType::kDropListenerPipeOnConstruction)
// Flaky on all platforms. http://crbug.com/997785.
UDPSOCKET_FAILURE_TEST(
    DISABLED_UDPSocket_DropListenerPipeOnReceiveMore,
    UDPSocket_ReadFails,
    WrappedUDPSocket::FailureType::kDropListenerPipeOnReceiveMore)

// Disallowed socket tests.
TEST_PPAPI_NACL_DISALLOWED_SOCKETS(HostResolverPrivateDisallowed)
TEST_PPAPI_NACL_DISALLOWED_SOCKETS(TCPServerSocketPrivateDisallowed)
TEST_PPAPI_NACL_DISALLOWED_SOCKETS(TCPSocketPrivateDisallowed)
TEST_PPAPI_NACL_DISALLOWED_SOCKETS(UDPSocketPrivateDisallowed)

// Checks that a hostname used by the HostResolver tests ("host_resolver.test")
// is present in the DNS cache with the NetworkAnonymizationKey associated with
// the foreground WebContents - this is needed so as not to leak what hostnames
// were looked up across tabs with different first party origins.
void CheckTestHostNameUsedWithCorrectNetworkIsolationKey(Browser* browser) {
  network::mojom::NetworkContext* network_context =
      browser->profile()->GetDefaultStoragePartition()->GetNetworkContext();
  const net::HostPortPair kHostPortPair(
      net::HostPortPair("host_resolver.test", 80));

  network::mojom::ResolveHostParametersPtr params =
      network::mojom::ResolveHostParameters::New();
  // Cache only lookup.
  params->source = net::HostResolverSource::LOCAL_ONLY;
  // Match the parameters used by the test.
  params->include_canonical_name = true;
  net::NetworkAnonymizationKey network_anonymization_key =
      browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame()
          ->GetIsolationInfoForSubresources()
          .network_anonymization_key();
  network::DnsLookupResult result1 =
      network::BlockingDnsLookup(network_context, kHostPortPair,
                                 std::move(params), network_anonymization_key);
  EXPECT_EQ(net::OK, result1.error);
  ASSERT_TRUE(result1.resolved_addresses.has_value());
  ASSERT_EQ(1u, result1.resolved_addresses->size());
  EXPECT_EQ(browser->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL()
                .host(),
            result1.resolved_addresses.value()[0].ToStringWithoutPort());

  // Check that the entry isn't present in the cache with the empty
  // NetworkAnonymizationKey().
  params = network::mojom::ResolveHostParameters::New();
  // Cache only lookup.
  params->source = net::HostResolverSource::LOCAL_ONLY;
  // Match the parameters used by the test.
  params->include_canonical_name = true;
  network::DnsLookupResult result2 = network::BlockingDnsLookup(
      network_context, kHostPortPair, std::move(params),
      net::NetworkAnonymizationKey());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, result2.error);
}

// HostResolver and HostResolverPrivate tests. The PPAPI code used by these
// tests is in ppapi/tests/test_host_resolver.cc.
#define RUN_HOST_RESOLVER_SUBTESTS                                             \
  RunTestViaHTTP(LIST_TEST(HostResolver_Empty) LIST_TEST(HostResolver_Resolve) \
                     LIST_TEST(HostResolver_ResolveIPv4));                     \
  CheckTestHostNameUsedWithCorrectNetworkIsolationKey(browser())

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, HostResolverCrash_Basic) {
  if (content::IsInProcessNetworkService())
    return;

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  IgnoreNetworkServiceCrashes();
  network_service_test->CrashOnResolveHost("crash.com");

  RunTestViaHTTP(STRIP_PREFIXES(HostResolverCrash_Basic));
}

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, HostResolver) {
  RUN_HOST_RESOLVER_SUBTESTS;
}

IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(HostResolver)) {
  RUN_HOST_RESOLVER_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(HostResolver)) {
  RUN_HOST_RESOLVER_SUBTESTS;
}

TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(HostResolverPrivate_Resolve)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(HostResolverPrivate_ResolveIPv4)
TEST_PPAPI_NACL(HostResolverPrivate_Resolve)
TEST_PPAPI_NACL(HostResolverPrivate_ResolveIPv4)

// URLLoader tests. These are split into multiple test fixtures because if we
// run them all together, they tend to time out.
#define RUN_URLLOADER_SUBTESTS_0 \
  RunTestViaHTTP( \
      LIST_TEST(URLLoader_BasicGET) \
      LIST_TEST(URLLoader_BasicPOST) \
      LIST_TEST(URLLoader_BasicFilePOST) \
      LIST_TEST(URLLoader_BasicFileRangePOST) \
      LIST_TEST(URLLoader_CompoundBodyPOST) \
  )

#define RUN_URLLOADER_SUBTESTS_1 \
  RunTestViaHTTP( \
      LIST_TEST(URLLoader_EmptyDataPOST) \
      LIST_TEST(URLLoader_BinaryDataPOST) \
      LIST_TEST(URLLoader_CustomRequestHeader) \
      LIST_TEST(URLLoader_FailsBogusContentLength) \
      LIST_TEST(URLLoader_StreamToFile) \
  )

// TODO(bbudge) Fix Javascript URLs for trusted loaders.
// http://crbug.com/103062
#define RUN_URLLOADER_SUBTESTS_2 \
  RunTestViaHTTP( \
      LIST_TEST(URLLoader_UntrustedSameOriginRestriction) \
      LIST_TEST(URLLoader_UntrustedCrossOriginRequest) \
      LIST_TEST(URLLoader_UntrustedCorbEligibleRequest) \
      LIST_TEST(URLLoader_UntrustedJavascriptURLRestriction) \
      LIST_TEST(DISABLED_URLLoader_TrustedJavascriptURLRestriction) \
  )

#define RUN_URLLOADER_NACL_SUBTESTS_2 \
  RunTestViaHTTP( \
      LIST_TEST(URLLoader_UntrustedSameOriginRestriction) \
      LIST_TEST(URLLoader_UntrustedCrossOriginRequest) \
      LIST_TEST(URLLoader_UntrustedCorbEligibleRequest) \
      LIST_TEST(URLLoader_UntrustedJavascriptURLRestriction) \
      LIST_TEST(DISABLED_URLLoader_TrustedJavascriptURLRestriction) \
  )

#define RUN_URLLOADER_SUBTESTS_3 \
  RunTestViaHTTP( \
      LIST_TEST(URLLoader_UntrustedHttpRequests) \
      LIST_TEST(URLLoader_FollowURLRedirect) \
      LIST_TEST(URLLoader_AuditURLRedirect) \
      LIST_TEST(URLLoader_RestrictURLRedirectCommon) \
      LIST_TEST(URLLoader_RestrictURLRedirectEnabled) \
      LIST_TEST(URLLoader_AbortCalls) \
      LIST_TEST(URLLoader_UntendedLoad) \
      LIST_TEST(URLLoader_PrefetchBufferThreshold) \
  )

// Note: we do not support Trusted APIs in NaCl, so these will be skipped.
// XRequestedWithHeader isn't trusted per-se, but the header isn't provided
// for NaCl and thus must be skipped.
#define RUN_URLLOADER_TRUSTED_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(URLLoader_TrustedSameOriginRestriction) \
      LIST_TEST(URLLoader_TrustedCrossOriginRequest) \
      LIST_TEST(URLLoader_TrustedCorbEligibleRequest) \
      LIST_TEST(URLLoader_TrustedHttpRequests) \
      LIST_TEST(URLLoader_XRequestedWithHeader) \
  )

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, URLLoader0) {
  RUN_URLLOADER_SUBTESTS_0;
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, URLLoader1) {
  RUN_URLLOADER_SUBTESTS_1;
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, URLLoader2) {
  RUN_URLLOADER_SUBTESTS_2;
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, URLLoader3) {
  RUN_URLLOADER_SUBTESTS_3;
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, URLLoaderTrusted) {
  RUN_URLLOADER_TRUSTED_SUBTESTS;
}

class OutOfProcessWithoutPepperCrossOriginRestrictionPPAPITest
    : public OutOfProcessPPAPITest {
 public:
  OutOfProcessWithoutPepperCrossOriginRestrictionPPAPITest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kPepperCrossOriginRedirectRestriction);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OutOfProcessWithoutPepperCrossOriginRestrictionPPAPITest,
                       URLLoaderRestrictURLRedirectDisabled) {
  // This test verifies if the restriction in the pepper_url_loader_host.cc
  // can be managed via base::FeatureList, and does not need to run with various
  // NaCl sandbox modes.
  RunTestViaHTTP(LIST_TEST(URLLoader_RestrictURLRedirectCommon)
                 LIST_TEST(URLLoader_RestrictURLRedirectDisabled));
}

IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(URLLoader0)) {
  RUN_URLLOADER_SUBTESTS_0;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(URLLoader1)) {
  RUN_URLLOADER_SUBTESTS_1;
}

IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(URLLoader2)) {
  RUN_URLLOADER_SUBTESTS_2;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(URLLoader3)) {
  RUN_URLLOADER_SUBTESTS_3;
}

IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(URLLoader0)) {
  RUN_URLLOADER_SUBTESTS_0;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(URLLoader1)) {
  RUN_URLLOADER_SUBTESTS_1;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(URLLoader2)) {
  RUN_URLLOADER_SUBTESTS_2;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(URLLoader3)) {
  RUN_URLLOADER_SUBTESTS_3;
}

// URLRequestInfo tests.
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLRequest_CreateAndIsURLRequestInfo)

TEST_PPAPI_NACL(URLRequest_CreateAndIsURLRequestInfo)

TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLRequest_SetProperty)
TEST_PPAPI_NACL(URLRequest_SetProperty)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLRequest_AppendDataToBody)
TEST_PPAPI_NACL(URLRequest_AppendDataToBody)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(DISABLED_URLRequest_AppendFileToBody)
TEST_PPAPI_NACL(DISABLED_URLRequest_AppendFileToBody)
TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(URLRequest_Stress)
TEST_PPAPI_NACL(URLRequest_Stress)

TEST_PPAPI_OUT_OF_PROCESS(PaintAggregator)
TEST_PPAPI_NACL(PaintAggregator)

// TODO(danakj): http://crbug.com/115286
TEST_PPAPI_NACL(DISABLED_Scrollbar)

TEST_PPAPI_NACL(Var)

TEST_PPAPI_NACL(VarResource)

// This test is only for x86-32 NaCl.
#if defined(ARCH_CPU_X86)
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, NaClIRTStackAlignment) {
  RunTestViaHTTP(STRIP_PREFIXES(NaClIRTStackAlignment));
}
#endif

// PostMessage tests.
#define RUN_POSTMESSAGE_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(PostMessage_SendInInit) \
      LIST_TEST(PostMessage_SendingData) \
      LIST_TEST(PostMessage_SendingString) \
      LIST_TEST(PostMessage_SendingArrayBuffer) \
      LIST_TEST(PostMessage_SendingArray) \
      LIST_TEST(PostMessage_SendingDictionary) \
      LIST_TEST(PostMessage_SendingResource) \
      LIST_TEST(PostMessage_SendingComplexVar) \
      LIST_TEST(PostMessage_MessageEvent) \
      LIST_TEST(PostMessage_NoHandler) \
      LIST_TEST(PostMessage_ExtraParam) \
      LIST_TEST(PostMessage_NonMainThread) \
  )

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, PostMessage) {
  RUN_POSTMESSAGE_SUBTESTS;
}

IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(PostMessage)) {
  RUN_POSTMESSAGE_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(PostMessage)) {
  RUN_POSTMESSAGE_SUBTESTS;
}

TEST_PPAPI_NACL(Memory)

// FileIO tests.
#define RUN_FILEIO_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(FileIO_Open) \
      LIST_TEST(FileIO_OpenDirectory) \
      LIST_TEST(FileIO_AbortCalls) \
      LIST_TEST(FileIO_ParallelReads) \
      LIST_TEST(FileIO_ParallelWrites) \
      LIST_TEST(FileIO_NotAllowMixedReadWrite) \
      LIST_TEST(FileIO_ReadWriteSetLength) \
      LIST_TEST(FileIO_ReadToArrayWriteSetLength) \
      LIST_TEST(FileIO_TouchQuery) \
  )

#define RUN_FILEIO_PRIVATE_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(FileIO_RequestOSFileHandle) \
      LIST_TEST(FileIO_RequestOSFileHandleWithOpenExclusive) \
      LIST_TEST(FileIO_Mmap) \
  )

IN_PROC_BROWSER_TEST_F(PPAPIPrivateTest, FileIO_Private) {
  RUN_FILEIO_PRIVATE_SUBTESTS;
}

// See: crbug.com/421284
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, DISABLED_FileIO) {
  RUN_FILEIO_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPIPrivateTest, FileIO_Private) {
  RUN_FILEIO_PRIVATE_SUBTESTS;
}

// http://crbug.com/313401
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, DISABLED_FileIO) {
  RUN_FILEIO_SUBTESTS;
}
// http://crbug.com/313401
IN_PROC_BROWSER_TEST_F(PPAPIPrivateNaClNewlibTest,
                       DISABLED_NaCl_Newlib_FileIO_Private) {
  RUN_FILEIO_PRIVATE_SUBTESTS;
}

// http://crbug.com/313205
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, DISABLED_FileIO) {
  RUN_FILEIO_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPIPrivateNaClPNaClTest,
                       DISABLED_PNaCl_FileIO_Private) {
  RUN_FILEIO_PRIVATE_SUBTESTS;
}

#define SETUP_FOR_FILEREF_TESTS                                             \
  const char kContents[] = "Hello from browser";                            \
  base::ScopedAllowBlockingForTesting allow_blocking;                       \
  base::ScopedTempDir temp_dir;                                             \
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());                              \
  base::FilePath existing_filename = temp_dir.GetPath().AppendASCII("foo"); \
  ASSERT_TRUE(base::WriteFile(existing_filename, kContents));               \
  PPAPITestSelectFileDialogFactory::SelectedFileInfoList file_info_list;    \
  file_info_list.emplace_back(ui::SelectedFileInfo(existing_filename));     \
  PPAPITestSelectFileDialogFactory test_dialog_factory(                     \
      PPAPITestSelectFileDialogFactory::RESPOND_WITH_FILE_LIST,             \
      file_info_list);

// FileRef tests.
#define RUN_FILEREF_SUBTESTS_1 \
  SETUP_FOR_FILEREF_TESTS \
  RunTestViaHTTP( \
      LIST_TEST(FileRef_Create) \
      LIST_TEST(FileRef_GetFileSystemType) \
      LIST_TEST(FileRef_GetName) \
      LIST_TEST(FileRef_GetPath) \
      LIST_TEST(FileRef_GetParent) \
      LIST_TEST(FileRef_MakeDirectory) \
  )

#define RUN_FILEREF_SUBTESTS_2 \
  SETUP_FOR_FILEREF_TESTS \
  RunTestViaHTTP( \
      LIST_TEST(FileRef_QueryAndTouchFile) \
      LIST_TEST(FileRef_DeleteFileAndDirectory) \
      LIST_TEST(FileRef_RenameFileAndDirectory) \
      LIST_TEST(FileRef_Query) \
      LIST_TEST(FileRef_FileNameEscaping) \
  )

// Note, the FileRef tests are split into two, because all of them together
// sometimes take too long on windows: crbug.com/336999
// FileRef_ReadDirectoryEntries is flaky, so left out. See crbug.com/241646.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, FileRef1) {
  RUN_FILEREF_SUBTESTS_1;
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, FileRef2) {
  RUN_FILEREF_SUBTESTS_2;
}

IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(FileRef1)) {
  RUN_FILEREF_SUBTESTS_1;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(FileRef2)) {
  RUN_FILEREF_SUBTESTS_2;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(FileRef1)) {
  RUN_FILEREF_SUBTESTS_1;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(FileRef2)) {
  RUN_FILEREF_SUBTESTS_2;
}

TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(FileSystem)

TEST_PPAPI_NACL(FileSystem)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// http://crbug.com/146008
#define MAYBE_Fullscreen DISABLED_Fullscreen
#else
#define MAYBE_Fullscreen Fullscreen
#endif

TEST_PPAPI_OUT_OF_PROCESS_VIA_HTTP(MAYBE_Fullscreen)
TEST_PPAPI_NACL(MAYBE_Fullscreen)

TEST_PPAPI_OUT_OF_PROCESS(X509CertificatePrivate)

TEST_PPAPI_OUT_OF_PROCESS(UMA)
TEST_PPAPI_NACL(UMA)

// NetAddress tests.
#define RUN_NETADDRESS_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(NetAddress_IPv4Address) \
      LIST_TEST(NetAddress_IPv6Address) \
      LIST_TEST(NetAddress_DescribeAsString) \
  )

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, NetAddress) {
  RUN_NETADDRESS_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(NetAddress)) {
  RUN_NETADDRESS_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(NetAddress)) {
  RUN_NETADDRESS_SUBTESTS;
}

// NetAddressPrivate tests.
#define RUN_NETADDRESS_PRIVATE_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(NetAddressPrivate_AreEqual) \
      LIST_TEST(NetAddressPrivate_AreHostsEqual) \
      LIST_TEST(NetAddressPrivate_Describe) \
      LIST_TEST(NetAddressPrivate_ReplacePort) \
      LIST_TEST(NetAddressPrivate_GetAnyAddress) \
      LIST_TEST(NetAddressPrivate_DescribeIPv6) \
      LIST_TEST(NetAddressPrivate_GetFamily) \
      LIST_TEST(NetAddressPrivate_GetPort) \
      LIST_TEST(NetAddressPrivate_GetAddress) \
      LIST_TEST(NetAddressPrivate_GetScopeID) \
  )

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, NetAddressPrivate) {
  RUN_NETADDRESS_PRIVATE_SUBTESTS;
}

#define RUN_NETADDRESS_PRIVATE_UNTRUSTED_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(NetAddressPrivateUntrusted_AreEqual) \
      LIST_TEST(NetAddressPrivateUntrusted_AreHostsEqual) \
      LIST_TEST(NetAddressPrivateUntrusted_Describe) \
      LIST_TEST(NetAddressPrivateUntrusted_ReplacePort) \
      LIST_TEST(NetAddressPrivateUntrusted_GetAnyAddress) \
      LIST_TEST(NetAddressPrivateUntrusted_GetFamily) \
      LIST_TEST(NetAddressPrivateUntrusted_GetPort) \
      LIST_TEST(NetAddressPrivateUntrusted_GetAddress) \
  )

IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest,
                       MAYBE_PPAPI_NACL(NetAddressPrivate)) {
  RUN_NETADDRESS_PRIVATE_UNTRUSTED_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest,
                       MAYBE_PPAPI_PNACL(NetAddressPrivate)) {
  RUN_NETADDRESS_PRIVATE_UNTRUSTED_SUBTESTS;
}

// NetworkMonitor tests.
#define RUN_NETWORK_MONITOR_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(NetworkMonitor_Basic) \
      LIST_TEST(NetworkMonitor_2Monitors) \
      LIST_TEST(NetworkMonitor_DeleteInCallback) \
  )

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, NetworkMonitor) {
  RUN_NETWORK_MONITOR_SUBTESTS;
}
// https://crbug.com/997840. Universally flaky.
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, DISABLED_NetworkMonitor) {
  RUN_NETWORK_MONITOR_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(NetworkMonitor)) {
  RUN_NETWORK_MONITOR_SUBTESTS;
}

// In-process WebSocket tests. Note, the WebSocket tests are split into two,
// because all of them together sometimes take too long on windows:
// crbug.com/336999
#define RUN_WEBSOCKET_SUBTESTS_1 \
  RunTestWithWebSocketServer( \
      LIST_TEST(WebSocket_IsWebSocket) \
      LIST_TEST(WebSocket_UninitializedPropertiesAccess) \
      LIST_TEST(WebSocket_InvalidConnect) \
      LIST_TEST(WebSocket_Protocols) \
      LIST_TEST(WebSocket_GetURL) \
      LIST_TEST(WebSocket_ValidConnect) \
      LIST_TEST(WebSocket_InvalidClose) \
      LIST_TEST(WebSocket_ValidClose) \
      LIST_TEST(WebSocket_GetProtocol) \
      LIST_TEST(WebSocket_TextSendReceive) \
      LIST_TEST(WebSocket_BinarySendReceive) \
      LIST_TEST(WebSocket_StressedSendReceive) \
      LIST_TEST(WebSocket_BufferedAmount) \
  )

#define RUN_WEBSOCKET_SUBTESTS_2 \
  RunTestWithWebSocketServer( \
      LIST_TEST(WebSocket_AbortCallsWithCallback) \
      LIST_TEST(WebSocket_AbortSendMessageCall) \
      LIST_TEST(WebSocket_AbortCloseCall) \
      LIST_TEST(WebSocket_AbortReceiveMessageCall) \
      LIST_TEST(WebSocket_ClosedFromServerWhileSending) \
      LIST_TEST(WebSocket_CcInterfaces) \
      LIST_TEST(WebSocket_UtilityInvalidConnect) \
      LIST_TEST(WebSocket_UtilityProtocols) \
      LIST_TEST(WebSocket_UtilityGetURL) \
      LIST_TEST(WebSocket_UtilityValidConnect) \
      LIST_TEST(WebSocket_UtilityInvalidClose) \
      LIST_TEST(WebSocket_UtilityValidClose) \
      LIST_TEST(WebSocket_UtilityGetProtocol) \
      LIST_TEST(WebSocket_UtilityTextSendReceive) \
      LIST_TEST(WebSocket_UtilityBinarySendReceive) \
      LIST_TEST(WebSocket_UtilityBufferedAmount) \
  )

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, WebSocket1) {
  RUN_WEBSOCKET_SUBTESTS_1;
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, WebSocket2) {
  RUN_WEBSOCKET_SUBTESTS_2;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(WebSocket1)) {
  RUN_WEBSOCKET_SUBTESTS_1;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(WebSocket2)) {
  RUN_WEBSOCKET_SUBTESTS_2;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(WebSocket1)) {
  RUN_WEBSOCKET_SUBTESTS_1;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(WebSocket2)) {
  RUN_WEBSOCKET_SUBTESTS_2;
}

// AudioConfig tests
#define RUN_AUDIO_CONFIG_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(AudioConfig_RecommendSampleRate) \
      LIST_TEST(AudioConfig_ValidConfigs) \
      LIST_TEST(AudioConfig_InvalidConfigs) \
  )

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, AudioConfig) {
  RUN_AUDIO_CONFIG_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(AudioConfig)) {
  RUN_AUDIO_CONFIG_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClGLibcTest, MAYBE_GLIBC(AudioConfig)) {
  RUN_AUDIO_CONFIG_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(AudioConfig)) {
  RUN_AUDIO_CONFIG_SUBTESTS;
}

// PPB_Audio tests.
#define RUN_AUDIO_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(Audio_Creation) \
      LIST_TEST(Audio_DestroyNoStop) \
      LIST_TEST(Audio_Failures) \
      LIST_TEST(Audio_AudioCallback1) \
      LIST_TEST(Audio_AudioCallback2) \
      LIST_TEST(Audio_AudioCallback3) \
      LIST_TEST(Audio_AudioCallback4) \
  )

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// http://crbug.com/396464
#define MAYBE_Audio DISABLED_Audio
#else
// Tests are flaky: http://crbug.com/629680
#define MAYBE_Audio DISABLED_Audio
#endif
// PPB_Audio is not supported in-process.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, MAYBE_Audio) {
  RUN_AUDIO_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(MAYBE_Audio)) {
  RUN_AUDIO_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClGLibcTest, MAYBE_GLIBC(MAYBE_Audio)) {
  RUN_AUDIO_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(MAYBE_Audio)) {
  RUN_AUDIO_SUBTESTS;
}

#define RUN_AUDIO_THREAD_CREATOR_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(Audio_AudioThreadCreatorIsRequired) \
      LIST_TEST(Audio_AudioThreadCreatorIsCalled) \
  )

// Tests are flaky: http://crbug.com/629680
#define MAYBE_AudioThreadCreator DISABLED_AudioThreadCreator

IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest,
                       MAYBE_PPAPI_NACL(MAYBE_AudioThreadCreator)) {
  RUN_AUDIO_THREAD_CREATOR_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClGLibcTest,
                       MAYBE_GLIBC(MAYBE_AudioThreadCreator)) {
  RUN_AUDIO_THREAD_CREATOR_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest,
                       MAYBE_PPAPI_PNACL(MAYBE_AudioThreadCreator)) {
  RUN_AUDIO_THREAD_CREATOR_SUBTESTS;
}

TEST_PPAPI_OUT_OF_PROCESS(View_CreatedVisible)
#if BUILDFLAG(IS_MAC)
// http://crbug.com/474399
#define MAYBE_View_CreatedVisible DISABLED_View_CreatedVisible
#else
#define MAYBE_View_CreatedVisible View_CreatedVisible
#endif
TEST_PPAPI_NACL(MAYBE_View_CreatedVisible)

// This test ensures that plugins created in a background tab have their
// initial visibility set to false. We don't bother testing in-process for this
// custom test since the out of process code also exercises in-process.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, View_CreateInvisible) {
  // Make a second tab in the foreground.
  GURL url = GetTestFileUrl("View_CreatedInvisible");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);
}

// This test messes with tab visibility so is custom.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, DISABLED_View_PageHideShow) {
  // The plugin will be loaded in the foreground tab and will send us a message.
  PPAPITestMessageHandler handler;
  content::JavascriptTestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      &handler);

  GURL url = GetTestFileUrl("View_PageHideShow");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(observer.Run()) << handler.error_message();
  EXPECT_STREQ("TestPageHideShow:Created", handler.message().c_str());
  observer.Reset();

  // Make a new tab to cause the original one to hide, this should trigger the
  // next phase of the test.
  NavigateParams params(browser(), GURL(url::kAboutBlankURL),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);

  // Wait until the test acks that it got hidden.
  ASSERT_TRUE(observer.Run()) << handler.error_message();
  EXPECT_STREQ("TestPageHideShow:Hidden", handler.message().c_str());
  observer.Reset();

  // Switch back to the test tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  ASSERT_TRUE(observer.Run()) << handler.error_message();
  EXPECT_STREQ("PASS", handler.message().c_str());
}

// Tests that if a plugin accepts touch events, the browser knows to send touch
// events to the renderer.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, InputEvent_AcceptTouchEvent1) {
  RunTouchEventTest("InputEvent_AcceptTouchEvent_1");
}

// The browser sends touch events to the renderer if the plugin registers for
// touch events and then unregisters.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, InputEvent_AcceptTouchEvent2) {
  RunTouchEventTest("InputEvent_AcceptTouchEvent_2");
}

// Tests that if a plugin accepts touch events, the browser knows to send touch
// events to the renderer. In this case, the plugin requests that input events
// corresponding to touch events are delivered for filtering.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, InputEvent_AcceptTouchEvent3) {
  RunTouchEventTest("InputEvent_AcceptTouchEvent_3");
}
// The plugin sends multiple RequestInputEvent() with the second
// requesting touch events to be delivered.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, InputEvent_AcceptTouchEvent4) {
  RunTouchEventTest("InputEvent_AcceptTouchEvent_4");
}
// View tests.
#define RUN_VIEW_SUBTESTS \
  RunTestViaHTTP( \
      LIST_TEST(View_SizeChange) \
      LIST_TEST(View_ClipChange) \
      LIST_TEST(View_ScrollOffsetChange) \
  )

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, View) {
  RUN_VIEW_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClNewlibTest, MAYBE_PPAPI_NACL(View)) {
  RUN_VIEW_SUBTESTS;
}
IN_PROC_BROWSER_TEST_F(PPAPINaClPNaClTest, MAYBE_PPAPI_PNACL(View)) {
  RUN_VIEW_SUBTESTS;
}

// The compositor test timeouts sometimes, so we have to split it to two
// subtests.
#define RUN_COMPOSITOR_SUBTESTS_0 \
  RunTestViaHTTP( \
      LIST_TEST(Compositor_BindUnbind) \
      LIST_TEST(Compositor_Release) \
      LIST_TEST(Compositor_ReleaseUnbound) \
      LIST_TEST(Compositor_ReleaseWithoutCommit) \
      LIST_TEST(Compositor_ReleaseWithoutCommitUnbound) \
  )

#define RUN_COMPOSITOR_SUBTESTS_1 \
  RunTestViaHTTP( \
      LIST_TEST(Compositor_CommitTwoTimesWithoutChange) \
      LIST_TEST(Compositor_CommitTwoTimesWithoutChangeUnbound) \
      LIST_TEST(Compositor_General) \
      LIST_TEST(Compositor_GeneralUnbound) \
  )

// flaky on Linux: http://crbug.com/396482
#define MAYBE_Compositor0 DISABLED_Compositor0
#define MAYBE_Compositor1 DISABLED_Compositor1

TEST_PPAPI_NACL_SUBTESTS(MAYBE_Compositor0, RUN_COMPOSITOR_SUBTESTS_0)
TEST_PPAPI_NACL_SUBTESTS(MAYBE_Compositor1, RUN_COMPOSITOR_SUBTESTS_1)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Flaky on ChromeOS, Linux, Windows, and Mac (crbug.com/438729)
#define MAYBE_MediaStreamAudioTrack DISABLED_MediaStreamAudioTrack
#else
#define MAYBE_MediaStreamAudioTrack MediaStreamAudioTrack
#endif
TEST_PPAPI_NACL(MAYBE_MediaStreamAudioTrack)

TEST_PPAPI_NACL(MediaStreamVideoTrack)

TEST_PPAPI_NACL(MouseCursor)

TEST_PPAPI_NACL(NetworkProxy)

// TODO(crbug.com/41248785), TODO(crbug.com/41248786) Flaky on CrOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_VideoDecoder DISABLED_VideoDecoder
#else
#define MAYBE_VideoDecoder VideoDecoder
#endif
TEST_PPAPI_NACL(MAYBE_VideoDecoder)

// https://crbug.com/997840.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_VideoEncoder DISABLED_VideoEncoder
#else
#define MAYBE_VideoEncoder VideoEncoder
#endif
TEST_PPAPI_NACL(MAYBE_VideoEncoder)

// Printing doesn't work in content_browsertests.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, Printing) {
#if BUILDFLAG(IS_CHROMEOS)
  printing::BrowserPrintingContextFactoryForTest test_printing_context_factory;
  auto test_backend = base::MakeRefCounted<printing::TestPrintBackend>();
  printing::PrintingContext::SetPrintingContextFactoryForTest(
      &test_printing_context_factory);
  printing::PrintBackend::SetPrintBackendForTesting(test_backend.get());
#endif

  RunTest("Printing");
}

// https://crbug.com/1038957.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MessageHandler DISABLED_MessageHandler
#else
#define MAYBE_MessageHandler MessageHandler
#endif
TEST_PPAPI_NACL(MAYBE_MessageHandler)

TEST_PPAPI_NACL(MessageLoop_Basics)
TEST_PPAPI_NACL(MessageLoop_Post)

#if BUILDFLAG(ENABLE_NACL)
class PackagedAppTest : public extensions::ExtensionBrowserTest {
 public:
  explicit PackagedAppTest(const std::string& toolchain)
      : toolchain_(toolchain) {
    feature_list_.InitAndEnableFeature(kNaclAllow);
  }

  void LaunchTestingApp(const std::string& extension_dirname) {
    base::FilePath data_dir;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathService::Get(chrome::DIR_GEN_TEST_DATA, &data_dir));
    }
    base::FilePath app_dir = data_dir.AppendASCII("ppapi")
                                     .AppendASCII("tests")
                                     .AppendASCII("extensions")
                                     .AppendASCII(extension_dirname)
                                     .AppendASCII(toolchain_);

    const extensions::Extension* extension = LoadExtension(app_dir);
    ASSERT_TRUE(extension);

    apps::AppLaunchParams params(
        extension->id(), apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
    params.command_line = *base::CommandLine::ForCurrentProcess();
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(std::move(params));
  }

  void RunTests(const std::string& extension_dirname) {
    ExtensionTestMessageListener listener("PASS");
    LaunchTestingApp(extension_dirname);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

 protected:
  std::string toolchain_;
  base::test::ScopedFeatureList feature_list_;
};

class NewlibPackagedAppTest : public PackagedAppTest {
 public:
  NewlibPackagedAppTest() : PackagedAppTest("newlib") { }
};

// Load a packaged app, and wait for it to successfully post a "hello" message
// back.
#if !defined(NDEBUG)
// flaky on debug builds: crbug.com/709447
IN_PROC_BROWSER_TEST_F(NewlibPackagedAppTest, DISABLED_SuccessfulLoad) {
#else
IN_PROC_BROWSER_TEST_F(NewlibPackagedAppTest,
                       MAYBE_PPAPI_NACL(SuccessfulLoad)) {
#endif
  RunTests("packaged_app");
}

IN_PROC_BROWSER_TEST_F(NewlibPackagedAppTest,
                       MAYBE_PPAPI_NACL(MulticastPermissions)) {
  RunTests("multicast_permissions");
}

IN_PROC_BROWSER_TEST_F(NewlibPackagedAppTest,
                       MAYBE_PPAPI_NACL(NoSocketPermissions)) {
  RunTests("no_socket_permissions");
}

IN_PROC_BROWSER_TEST_F(NewlibPackagedAppTest,
                       MAYBE_PPAPI_NACL(SocketPermissions)) {
  RunTests("socket_permissions");
}

#endif
