// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <locale>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/server/http_handler.h"
#include "mojo/core/embedder/embedder.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/log/net_log_source.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace {

// Maximum message size between app and ChromeDriver. Data larger than 150 MB
// or so can cause crashes in Chrome (https://crbug.com/890854), so there is no
// need to support messages that are too large.
const int kBufferSize = 256 * 1024 * 1024;  // 256 MB

typedef base::Callback<
    void(const net::HttpServerRequestInfo&, const HttpResponseSenderFunc&)>
    HttpRequestHandlerFunc;

int ListenOnIPv4(net::ServerSocket* socket, uint16_t port, bool allow_remote) {
  std::string binding_ip = net::IPAddress::IPv4Localhost().ToString();
  if (allow_remote)
    binding_ip = net::IPAddress::IPv4AllZeros().ToString();
  return socket->ListenWithAddressAndPort(binding_ip, port, 5);
}

int ListenOnIPv6(net::ServerSocket* socket, uint16_t port, bool allow_remote) {
  std::string binding_ip = net::IPAddress::IPv6Localhost().ToString();
  if (allow_remote)
    binding_ip = net::IPAddress::IPv6AllZeros().ToString();
  return socket->ListenWithAddressAndPort(binding_ip, port, 5);
}

bool RequestIsSafeToServe(const net::HttpServerRequestInfo& info) {
  // To guard against browser-originating cross-site requests, when host header
  // and/or origin header are present, serve only those coming from localhost.
  std::string host_header = info.headers["host"];
  if (!host_header.empty()) {
    GURL url = GURL("http://" + host_header);
    if (!net::IsLocalhost(url)) {
      LOG(ERROR) << "Rejecting request with host: " << host_header;
      return false;
    }
  }
  std::string origin_header = info.headers["origin"];
  if (!origin_header.empty()) {
    GURL url = GURL(origin_header);
    if (!net::IsLocalhost(url)) {
      LOG(ERROR) << "Rejecting request with origin: " << origin_header;
      return false;
    }
  }
  return true;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Ensure that there is a writable shared memory directory. We use
// network::SimpleURLLoader to connect to Chrome, and it calls
// base::subtle::PlatformSharedMemoryRegion::Create to get a shared memory
// region. network::SimpleURLLoader would fail if the shared memory directory is
// not accessible. We work around this issue by adding --disable-dev-shm-usage
// to command line, to use an alternative directory for shared memory.
// See https://crbug.com/chromedriver/2782.
void EnsureSharedMemory(base::CommandLine* cmd_line) {
  if (!cmd_line->HasSwitch("disable-dev-shm-usage")) {
    base::FilePath directory;
    if (GetShmemTempDir(false, &directory) &&
        access(directory.value().c_str(), W_OK | X_OK) < 0) {
      VLOG(0) << directory
              << " not writable, adding --disable-dev-shm-usage switch";
      cmd_line->AppendSwitch("disable-dev-shm-usage");
    }
  }
}
#endif

class HttpServer : public net::HttpServer::Delegate {
 public:
  explicit HttpServer(const std::string& url_base,
                      const HttpRequestHandlerFunc& handle_request_func)
      : url_base_(url_base),
        handle_request_func_(handle_request_func),
        allow_remote_(false) {}

  ~HttpServer() override {}

  int Start(uint16_t port, bool allow_remote, bool use_ipv4) {
    allow_remote_ = allow_remote;
    std::unique_ptr<net::ServerSocket> server_socket(
        new net::TCPServerSocket(NULL, net::NetLogSource()));
    int status = use_ipv4
                     ? ListenOnIPv4(server_socket.get(), port, allow_remote)
                     : ListenOnIPv6(server_socket.get(), port, allow_remote);
    if (status != net::OK) {
      VLOG(0) << "listen on " << (use_ipv4 ? "IPv4" : "IPv6")
              << " failed with error " << net::ErrorToShortString(status);
      return status;
    }
    server_.reset(new net::HttpServer(std::move(server_socket), this));
    net::IPEndPoint address;
    return server_->GetLocalAddress(&address);
  }

  // Overridden from net::HttpServer::Delegate:
  void OnConnect(int connection_id) override {
    server_->SetSendBufferSize(connection_id, kBufferSize);
    server_->SetReceiveBufferSize(connection_id, kBufferSize);
  }

  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override {
    if (!allow_remote_ && !RequestIsSafeToServe(info)) {
      server_->Send500(
          connection_id,
          "Host header or origin header is specified and is not localhost.",
          TRAFFIC_ANNOTATION_FOR_TESTS);
      return;
    }
    handle_request_func_.Run(
        info,
        base::Bind(&HttpServer::OnResponse,
                   weak_factory_.GetWeakPtr(),
                   connection_id,
                   !info.HasHeaderValue("connection", "close")));
  }

  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override {
    std::string path = info.path;
    std::string session_id;

    if (!base::StartsWith(path, url_base_, base::CompareCase::SENSITIVE)) {
      net::HttpServerResponseInfo response(net::HTTP_BAD_REQUEST);
      response.SetBody("invalid websocket request url path", "text/plain");
      server_->SendResponse(connection_id, response,
                            TRAFFIC_ANNOTATION_FOR_TESTS);
      return;
    }
    path.erase(0, url_base_.length());

    std::vector<std::string> path_parts = base::SplitString(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::vector<std::string> command_path_parts = base::SplitString(
        kCreateWebSocketPath, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    if (path_parts.size() != command_path_parts.size()) {
      net::HttpServerResponseInfo response(net::HTTP_BAD_REQUEST);
      response.SetBody("invalid websocket request url path", "text/plain");
      server_->SendResponse(connection_id, response,
                            TRAFFIC_ANNOTATION_FOR_TESTS);
      return;
    }

    for (size_t i = 0; i < path_parts.size(); ++i) {
      if (command_path_parts[i][0] == ':') {
        std::string name = command_path_parts[i];
        name.erase(0, 1);
        CHECK(name.length());
        if (name == "sessionId")
          session_id = path_parts[i];
      } else if (command_path_parts[i] != path_parts[i]) {
        net::HttpServerResponseInfo response(net::HTTP_BAD_REQUEST);
        response.SetBody("invalid websocket request url path", "text/plain");
        server_->SendResponse(connection_id, response,
                              TRAFFIC_ANNOTATION_FOR_TESTS);
        return;
      }
    }

    server_->AcceptWebSocket(connection_id, info, TRAFFIC_ANNOTATION_FOR_TESTS);
    connection_to_session_map[connection_id] = session_id;
  }

  void OnWebSocketMessage(int connection_id, std::string data) override {
    base::Optional<base::Value> parsed_data = base::JSONReader::Read(data);
    std::string path = url_base_ + kSendCommandFromWebSocket;
    base::ReplaceFirstSubstringAfterOffset(
        &path, 0, ":sessionId", connection_to_session_map[connection_id]);

    net::HttpServerRequestInfo request;
    request.method = "post";
    request.path = path;
    request.data = data;
    OnHttpRequest(connection_id, request);
  }

  void OnClose(int connection_id) override {}

 private:
  void OnResponse(int connection_id,
                  bool keep_alive,
                  std::unique_ptr<net::HttpServerResponseInfo> response) {
    if (!keep_alive)
      response->AddHeader("Connection", "close");
    server_->SendResponse(connection_id, *response,
                          TRAFFIC_ANNOTATION_FOR_TESTS);
    // Don't need to call server_->Close(), since SendResponse() will handle
    // this for us.
  }

  const std::string url_base_;
  HttpRequestHandlerFunc handle_request_func_;
  std::unique_ptr<net::HttpServer> server_;
  std::map<int, std::string> connection_to_session_map;
  bool allow_remote_;
  base::WeakPtrFactory<HttpServer> weak_factory_{this};  // Should be last.
};

void SendResponseOnCmdThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const HttpResponseSenderFunc& send_response_on_io_func,
    std::unique_ptr<net::HttpServerResponseInfo> response) {
  io_task_runner->PostTask(
      FROM_HERE, base::BindOnce(send_response_on_io_func, std::move(response)));
}

void HandleRequestOnCmdThread(
    HttpHandler* handler,
    const std::vector<net::IPAddress>& whitelisted_ips,
    const net::HttpServerRequestInfo& request,
    const HttpResponseSenderFunc& send_response_func) {
  if (!whitelisted_ips.empty()) {
    const net::IPAddress& peer_address = request.peer.address();
    if (!base::Contains(whitelisted_ips, peer_address)) {
      LOG(WARNING) << "unauthorized access from " << request.peer.ToString();
      std::unique_ptr<net::HttpServerResponseInfo> response(
          new net::HttpServerResponseInfo(net::HTTP_UNAUTHORIZED));
      response->SetBody("Unauthorized access", "text/plain");
      send_response_func.Run(std::move(response));
      return;
    }
  }

  handler->Handle(request, send_response_func);
}

void HandleRequestOnIOThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& cmd_task_runner,
    const HttpRequestHandlerFunc& handle_request_on_cmd_func,
    const net::HttpServerRequestInfo& request,
    const HttpResponseSenderFunc& send_response_func) {
  cmd_task_runner->PostTask(
      FROM_HERE, base::BindOnce(handle_request_on_cmd_func, request,
                                base::Bind(&SendResponseOnCmdThread,
                                           base::ThreadTaskRunnerHandle::Get(),
                                           send_response_func)));
}

base::LazyInstance<base::ThreadLocalPointer<HttpServer>>::DestructorAtExit
    lazy_tls_server_ipv4 = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::ThreadLocalPointer<HttpServer>>::DestructorAtExit
    lazy_tls_server_ipv6 = LAZY_INSTANCE_INITIALIZER;

void StopServerOnIOThread() {
  // Note, |server| may be NULL.
  HttpServer* server = lazy_tls_server_ipv4.Pointer()->Get();
  lazy_tls_server_ipv4.Pointer()->Set(NULL);
  delete server;

  server = lazy_tls_server_ipv6.Pointer()->Get();
  lazy_tls_server_ipv6.Pointer()->Set(NULL);
  delete server;
}

void StartServerOnIOThread(uint16_t port,
                           bool allow_remote,
                           const std::string& url_base,
                           const HttpRequestHandlerFunc& handle_request_func) {
  std::unique_ptr<HttpServer> temp_server;

// On Linux and Windows, we listen to IPv6 first, and then optionally listen
// to IPv4 (depending on |need_ipv4| below). The reason is listening to an
// IPv6 port may automatically listen to the same IPv4 port as well, and would
// return an error if the IPv4 port is already in use.
//
// On Mac, however, we listen to IPv4 first before listening to IPv6. If we
// were to listen to IPv6 first, it would succeed whether the corresponding
// IPv4 port is in use or not, and we wouldn't know if we ended up listening
// to both IPv4 and IPv6 ports, or only IPv6 port. Listening to IPv4 first
// ensures that we successfully listen to both IPv4 and IPv6.

#if defined(OS_MACOSX)
  temp_server.reset(new HttpServer(url_base, handle_request_func));
  int ipv4_status = temp_server->Start(port, allow_remote, true);
  if (ipv4_status == net::OK) {
    lazy_tls_server_ipv4.Pointer()->Set(temp_server.release());
  } else if (ipv4_status == net::ERR_ADDRESS_IN_USE) {
    // ERR_ADDRESS_IN_USE causes an immediate exit, since it indicates the port
    // is being used by another process. Other errors are assumed to indicate
    // that IPv4 isn't available for some reason, e.g., on an IPv6-only host.
    // Thus the error doesn't cause an exit immediately. The HttpServer::Start
    // method has already printed a message indicating what has happened. Later,
    // near the end of this function, we exit if both IPv4 and IPv6 failed.
    printf("IPv4 port not available. Exiting...\n");
    exit(1);
  }
#endif

  temp_server.reset(new HttpServer(url_base, handle_request_func));
  int ipv6_status = temp_server->Start(port, allow_remote, false);
  if (ipv6_status == net::OK) {
    lazy_tls_server_ipv6.Pointer()->Set(temp_server.release());
  } else if (ipv6_status == net::ERR_ADDRESS_IN_USE) {
    printf("IPv6 port not available. Exiting...\n");
    exit(1);
  }

#if !defined(OS_MACOSX)
  // In some cases, binding to an IPv6 port also binds to the same IPv4 port.
  // The following code determines if it is necessary to bind to IPv4 port.
  enum class NeedIPv4 { NOT_NEEDED, UNKNOWN, NEEDED } need_ipv4;
  // Dual-protocol bind deosn't work while binding to localhost (!allow_remote).
  if (!allow_remote || ipv6_status != net::OK) {
    need_ipv4 = NeedIPv4::NEEDED;
  } else {
// Currently, the network layer provides no way for us to control dual-protocol
// bind option, or to query the current setting of that option, so we do our
// best to determine the current setting. See https://crbug.com/858892.
#if defined(OS_LINUX)
    // On Linux, dual-protocol bind is controlled by a system file.
    // ChromeOS builds also have OS_LINUX defined, so the code below applies.
    std::string bindv6only;
    base::FilePath bindv6only_filename("/proc/sys/net/ipv6/bindv6only");
    if (!base::ReadFileToString(bindv6only_filename, &bindv6only)) {
      LOG(WARNING) << "Unable to read " << bindv6only_filename << ".";
      need_ipv4 = NeedIPv4::UNKNOWN;
    } else if (bindv6only == "1\n") {
      need_ipv4 = NeedIPv4::NEEDED;
    } else if (bindv6only == "0\n") {
      need_ipv4 = NeedIPv4::NOT_NEEDED;
    } else {
      LOG(WARNING) << "Unexpected " << bindv6only_filename << " contents.";
      need_ipv4 = NeedIPv4::UNKNOWN;
    }
#elif defined(OS_WIN)
    // On Windows, the net component always enables dual-protocol bind. See
    // https://chromium.googlesource.com/chromium/src/+/69.0.3464.0/net/socket/socket_descriptor.cc#28.
    need_ipv4 = NeedIPv4::NOT_NEEDED;
#else
    LOG(WARNING) << "Running on a platform not officially supported by "
                 << kChromeDriverProductFullName << ".";
    need_ipv4 = NeedIPv4::UNKNOWN;
#endif
  }
  int ipv4_status;
  if (need_ipv4 == NeedIPv4::NOT_NEEDED) {
    ipv4_status = ipv6_status;
  } else {
    temp_server.reset(new HttpServer(url_base, handle_request_func));
    ipv4_status = temp_server->Start(port, allow_remote, true);
    if (ipv4_status == net::OK) {
      lazy_tls_server_ipv4.Pointer()->Set(temp_server.release());
    } else if (ipv4_status == net::ERR_ADDRESS_IN_USE) {
      if (need_ipv4 == NeedIPv4::NEEDED) {
        printf("IPv4 port not available. Exiting...\n");
        exit(1);
      } else {
        printf("Unable to determine if bind to IPv4 port was successful.\n");
      }
    }
  }
#endif  // !defined(OS_MACOSX)

  if (ipv4_status != net::OK && ipv6_status != net::OK) {
    printf("Unable to start server with either IPv4 or IPv6. Exiting...\n");
    exit(1);
  }
}

void RunServer(uint16_t port,
               bool allow_remote,
               const std::vector<net::IPAddress>& whitelisted_ips,
               const std::string& url_base,
               int adb_port) {
  base::Thread io_thread(
      base::StringPrintf("%s IO", kChromeDriverProductShortName));
  CHECK(io_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0)));

  base::SingleThreadTaskExecutor main_task_executor;
  base::RunLoop cmd_run_loop;
  HttpHandler handler(cmd_run_loop.QuitClosure(), io_thread.task_runner(),
                      url_base, adb_port);
  HttpRequestHandlerFunc handle_request_func =
      base::Bind(&HandleRequestOnCmdThread, &handler, whitelisted_ips);

  io_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &StartServerOnIOThread, port, allow_remote, url_base,
          base::Bind(&HandleRequestOnIOThread, main_task_executor.task_runner(),
                     handle_request_func)));
  // Run the command loop. This loop is quit after the response for a shutdown
  // request is posted to the IO loop. After the command loop quits, a task
  // is posted to the IO loop to stop the server. Lastly, the IO thread is
  // destroyed, which waits until all pending tasks have been completed.
  // This assumes the response is sent synchronously as part of the IO task.
  cmd_run_loop.Run();
  io_thread.task_runner()->PostTask(FROM_HERE,
                                    base::BindOnce(&StopServerOnIOThread));
}

}  // namespace

int main(int argc, char *argv[]) {
  base::CommandLine::Init(argc, argv);

  base::AtExitManager at_exit;
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

#if defined(OS_LINUX)
  // Select the locale from the environment by passing an empty string instead
  // of the default "C" locale. This is particularly needed for the keycode
  // conversion code to work.
  setlocale(LC_ALL, "");
#endif

  // Parse command line flags.
  uint16_t port = 9515;
  int adb_port = 5037;
  bool allow_remote = false;
  std::vector<net::IPAddress> whitelisted_ips;
  std::string url_base;
  if (cmd_line->HasSwitch("h") || cmd_line->HasSwitch("help")) {
    std::string options;
    const char* const kOptionAndDescriptions[] = {
        "port=PORT",
            "port to listen on",
        "adb-port=PORT",
            "adb server port",
        "log-path=FILE",
            "write server log to file instead of stderr, "
            "increases log level to INFO",
        "log-level=LEVEL",
            "set log level: ALL, DEBUG, INFO, WARNING, SEVERE, OFF",
        "verbose",
            "log verbosely (equivalent to --log-level=ALL)",
        "silent",
            "log nothing (equivalent to --log-level=OFF)",
        "append-log",
            "append log file instead of rewriting",
        "replayable",
            "(experimental) log verbosely and don't truncate long "
            "strings so that the log can be replayed.",
        "version",
            "print the version number and exit",
        "url-base",
            "base URL path prefix for commands, e.g. wd/url",
        "readable-timestamp",
            "add readable timestamps to log",
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
        "disable-dev-shm-usage",
            "do not use /dev/shm "
            "(add this switch if seeing errors related to shared memory)",
#endif
    };
    for (size_t i = 0; i < base::size(kOptionAndDescriptions) - 1; i += 2) {
      options += base::StringPrintf(
          "  --%-30s%s\n",
          kOptionAndDescriptions[i], kOptionAndDescriptions[i + 1]);
    }

    // Add helper info for whitelisted-ips since the product name may be
    // different.
    options += base::StringPrintf(
        "  --%-30scomma-separated whitelist of remote IP addresses which are "
        "allowed to connect to %s\n",
        "whitelisted-ips", kChromeDriverProductShortName);

    printf("Usage: %s [OPTIONS]\n\nOptions\n%s", argv[0], options.c_str());
    return 0;
  }
  bool early_exit = false;
  if (cmd_line->HasSwitch("v") || cmd_line->HasSwitch("version")) {
    printf("%s %s\n", kChromeDriverProductFullName, kChromeDriverVersion);
    early_exit = true;
  }
  if (early_exit)
    return 0;
  if (cmd_line->HasSwitch("port")) {
    int cmd_line_port;
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("port"),
                           &cmd_line_port) ||
        cmd_line_port < 0 || cmd_line_port > 65535) {
      printf("Invalid port. Exiting...\n");
      return 1;
    }
    port = static_cast<uint16_t>(cmd_line_port);
  }
  if (cmd_line->HasSwitch("adb-port")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("adb-port"),
                           &adb_port)) {
      printf("Invalid adb-port. Exiting...\n");
      return 1;
    }
  }
  if (cmd_line->HasSwitch("url-base"))
    url_base = cmd_line->GetSwitchValueASCII("url-base");
  if (url_base.empty() || url_base.front() != '/')
    url_base = "/" + url_base;
  if (url_base.back() != '/')
    url_base = url_base + "/";
  if (cmd_line->HasSwitch("whitelisted-ips")) {
    allow_remote = true;
    std::string whitelist = cmd_line->GetSwitchValueASCII("whitelisted-ips");
    std::vector<std::string> whitelist_ip_strs = base::SplitString(
        whitelist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (!whitelist_ip_strs.empty()) {
      // Convert IP address strings into net::IPAddress objects.
      for (const auto& ip_str : whitelist_ip_strs) {
        base::StringPiece ip_str_piece(ip_str);
        if (ip_str_piece.size() >= 2 && ip_str_piece.front() == '[' &&
            ip_str_piece.back() == ']') {
          ip_str_piece.remove_prefix(1);
          ip_str_piece.remove_suffix(1);
        }
        net::IPAddress ip;
        if (!ip.AssignFromIPLiteral(ip_str_piece)) {
          printf("Invalid IP address %s. Exiting...\n", ip_str.c_str());
          return 1;
        }
        whitelisted_ips.push_back(ip);
        if (ip.IsIPv4()) {
          whitelisted_ips.push_back(net::ConvertIPv4ToIPv4MappedIPv6(ip));
        } else if (ip.IsIPv4MappedIPv6()) {
          whitelisted_ips.push_back(net::ConvertIPv4MappedIPv6ToIPv4(ip));
        }
      }
      whitelisted_ips.push_back(net::IPAddress::IPv4Localhost());
      whitelisted_ips.push_back(net::IPAddress::IPv6Localhost());
      whitelisted_ips.push_back(
          net::ConvertIPv4ToIPv4MappedIPv6(net::IPAddress::IPv4Localhost()));
    }
  }
  if (!cmd_line->HasSwitch("silent") &&
      cmd_line->GetSwitchValueASCII("log-level") != "OFF") {
    printf("Starting %s %s on port %u\n", kChromeDriverProductShortName,
           kChromeDriverVersion, port);
    if (!allow_remote) {
      printf("Only local connections are allowed.\n");
    } else if (!whitelisted_ips.empty()) {
      printf("Remote connections are allowed by a whitelist (%s).\n",
             cmd_line->GetSwitchValueASCII("whitelisted-ips").c_str());
    } else {
      printf("All remote connections are allowed. Use a whitelist instead!\n");
    }
    printf("%s\n", GetPortProtectionMessage());
    fflush(stdout);
  }

  if (!InitLogging(port)) {
    printf("Unable to initialize logging. Exiting...\n");
    return 1;
  }

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  EnsureSharedMemory(cmd_line);
#endif

  mojo::core::Init();

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      kChromeDriverProductShortName);

  RunServer(port, allow_remote, whitelisted_ips, url_base, adb_port);

  // clean up
  base::ThreadPoolInstance::Get()->Shutdown();
  return 0;
}
