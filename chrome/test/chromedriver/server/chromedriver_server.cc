// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <locale>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/keycode_text_conversion.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/server/http_handler.h"
#include "chrome/test/chromedriver/server/http_server.h"
#include "mojo/core/embedder/embedder.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"

namespace {

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

void SendResponseOnCmdThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const HttpResponseSenderFunc& send_response_on_io_func,
    std::unique_ptr<net::HttpServerResponseInfo> response) {
  io_task_runner->PostTask(
      FROM_HERE, base::BindOnce(send_response_on_io_func, std::move(response)));
}

void HandleRequestOnCmdThread(
    HttpHandler* handler,
    const std::vector<net::IPAddress>& allowed_ips,
    const net::HttpServerRequestInfo& request,
    const HttpResponseSenderFunc& send_response_func) {
  if (!allowed_ips.empty()) {
    const net::IPAddress& peer_address = request.peer.address();
    if (!base::Contains(allowed_ips, peer_address)) {
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
      FROM_HERE,
      base::BindOnce(
          handle_request_on_cmd_func, request,
          base::BindRepeating(&SendResponseOnCmdThread,
                              base::SingleThreadTaskRunner::GetCurrentDefault(),
                              send_response_func)));
}

constinit thread_local HttpServer* server_ipv4 = nullptr;
constinit thread_local HttpServer* server_ipv6 = nullptr;

void StopServerOnIOThread() {
  delete server_ipv4;
  server_ipv4 = nullptr;

  delete server_ipv6;
  server_ipv4 = nullptr;
}

void StartServerOnIOThread(
    uint16_t port,
    bool allow_remote,
    const std::string& url_base,
    const std::vector<net::IPAddress>& allowed_ips,
    const std::vector<std::string>& allowed_origins,
    const HttpRequestHandlerFunc& handle_request_func,
    base::WeakPtr<HttpHandler> handler,
    const scoped_refptr<base::SingleThreadTaskRunner>& cmd_task_runner) {
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

#if BUILDFLAG(IS_MAC)
  temp_server = std::make_unique<HttpServer>(
      url_base, allowed_ips, allowed_origins, handle_request_func, handler,
      cmd_task_runner);
  int ipv4_status = temp_server->Start(port, allow_remote, true);
  if (ipv4_status == net::OK) {
    port = temp_server->LocalAddress().port();
    server_ipv4 = temp_server.release();
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

  temp_server = std::make_unique<HttpServer>(
      url_base, allowed_ips, allowed_origins, handle_request_func, handler,
      cmd_task_runner);
  int ipv6_status = temp_server->Start(port, allow_remote, false);
  if (ipv6_status == net::OK) {
    port = temp_server->LocalAddress().port();
    server_ipv6 = temp_server.release();
  } else if (ipv6_status == net::ERR_ADDRESS_IN_USE) {
    printf("IPv6 port not available. Exiting...\n");
    exit(1);
  }

#if !BUILDFLAG(IS_MAC)
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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
#elif BUILDFLAG(IS_WIN)
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
    temp_server = std::make_unique<HttpServer>(
        url_base, allowed_ips, allowed_origins, handle_request_func, handler,
        cmd_task_runner);
    ipv4_status = temp_server->Start(port, allow_remote, true);
    if (ipv4_status == net::OK) {
      server_ipv4 = temp_server.release();
    } else if (ipv4_status == net::ERR_ADDRESS_IN_USE) {
      if (need_ipv4 == NeedIPv4::NEEDED) {
        printf("IPv4 port not available. Exiting...\n");
        exit(1);
      } else {
        printf("Unable to determine if bind to IPv4 port was successful.\n");
      }
    }
  }
#endif  // !BUILDFLAG(IS_MAC)

  if (ipv4_status != net::OK && ipv6_status != net::OK) {
    printf("Unable to start server with either IPv4 or IPv6. Exiting...\n");
    exit(1);
  }

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch("silent") &&
      cmd_line->GetSwitchValueASCII("log-level") != "OFF") {
    printf("%s was started successfully on port %u.\n",
           kChromeDriverProductShortName, port);
  }
  if (cmd_line->HasSwitch("log-path")) {
    VLOG(0) << kChromeDriverProductShortName
            << " was started successfully on port " << port;
  }
  fflush(stdout);
}

void RunServer(uint16_t port,
               bool allow_remote,
               const std::vector<net::IPAddress>& allowed_ips,
               const std::vector<std::string>& allowed_origins,
               const std::string& url_base,
               int adb_port) {
  base::Thread io_thread(
      base::StringPrintf("%s IO", kChromeDriverProductShortName));
  CHECK(io_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0)));

  base::SingleThreadTaskExecutor main_task_executor;
  base::RunLoop cmd_run_loop;
  HttpHandler handler(cmd_run_loop.QuitClosure(), io_thread.task_runner(),
                      main_task_executor.task_runner(), url_base, adb_port);
  HttpRequestHandlerFunc handle_request_func =
      base::BindRepeating(&HandleRequestOnCmdThread, &handler, allowed_ips);

  io_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&StartServerOnIOThread, port, allow_remote, url_base,
                     allowed_ips, allowed_origins,
                     base::BindRepeating(&HandleRequestOnIOThread,
                                         main_task_executor.task_runner(),
                                         handle_request_func),
                     handler.WeakPtr(), main_task_executor.task_runner()));
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Select the locale from the environment by passing an empty string instead
  // of the default "C" locale. This is particularly needed for the keycode
  // conversion code to work.
  setlocale(LC_ALL, "");
#endif

  // Parse command line flags.
  uint16_t port = 0;
  int adb_port = 5037;
  bool allow_remote = false;
  std::vector<net::IPAddress> allowed_ips;
  std::vector<std::string> allowed_origins;
  std::string allowlist_ips;
  std::string allowlist_origins;
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
        "enable-chrome-logs",
        "show logs from the browser (overrides other logging options)",
        "bidi-mapper-path",
        "custom bidi mapper path",
    // TODO(crbug.com/40118868): Revisit the macro expression once build flag
    // switch of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
        "disable-dev-shm-usage",
        "do not use /dev/shm "
        "(add this switch if seeing errors related to shared memory)",
#endif
        // TODO(crbug.com/354135326): This is a temporary flag needed to
        // smooothly migrate the web platform tests to auto-assigned port.
        // This switch will be removed in M132. Don't rely on it!
        "ignore-explicit-port",
        "(experimental) ignore the port specified explicitly, "
        "find a free port instead",
    };
    for (size_t i = 0; i < std::size(kOptionAndDescriptions) - 1; i += 2) {
      options += base::StringPrintf(
          "  --%-30s%s\n",
          kOptionAndDescriptions[i], kOptionAndDescriptions[i + 1]);
    }

    // Add helper info for `allowed-ips` and `allowed-origins` since the product
    // name may be different.
    options += base::StringPrintf(
        "  --%-30scomma-separated allowlist of remote IP addresses which are "
        "allowed to connect to %s\n",
        "allowed-ips=LIST", kChromeDriverProductShortName);
    options += base::StringPrintf(
        "  --%-30scomma-separated allowlist of request origins which are "
        "allowed to connect to %s. Using `*` to allow any host origin is "
        "dangerous!\n",
        "allowed-origins=LIST", kChromeDriverProductShortName);

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
  if (cmd_line->HasSwitch("ignore-explicit-port")) {
    port = 0;
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
  if (cmd_line->HasSwitch("allowed-ips") ||
      cmd_line->HasSwitch("whitelisted-ips")) {
    allow_remote = true;
    if (cmd_line->HasSwitch("allowed-ips"))
      allowlist_ips = cmd_line->GetSwitchValueASCII("allowed-ips");
    else
      allowlist_ips = cmd_line->GetSwitchValueASCII("whitelisted-ips");

    std::vector<std::string> allowlist_ip_strs = base::SplitString(
        allowlist_ips, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (!allowlist_ip_strs.empty()) {
      // Convert IP address strings into net::IPAddress objects.
      for (const auto& ip_str : allowlist_ip_strs) {
        std::string_view ip_str_piece(ip_str);
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
        allowed_ips.push_back(ip);
        if (ip.IsIPv4()) {
          allowed_ips.push_back(net::ConvertIPv4ToIPv4MappedIPv6(ip));
        } else if (ip.IsIPv4MappedIPv6()) {
          allowed_ips.push_back(net::ConvertIPv4MappedIPv6ToIPv4(ip));
        }
      }
      allowed_ips.push_back(net::IPAddress::IPv4Localhost());
      allowed_ips.push_back(net::IPAddress::IPv6Localhost());
      allowed_ips.push_back(
          net::ConvertIPv4ToIPv4MappedIPv6(net::IPAddress::IPv4Localhost()));
    }
  }

  if (cmd_line->HasSwitch("allowed-origins")) {
    allowlist_origins = cmd_line->GetSwitchValueASCII("allowed-origins");
    allowed_origins = base::SplitString(
        allowlist_origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }

  if (!cmd_line->HasSwitch("silent") &&
      cmd_line->GetSwitchValueASCII("log-level") != "OFF") {
    printf("Starting %s %s on port %u\n", kChromeDriverProductShortName,
           kChromeDriverVersion, port);
    if (!allow_remote) {
      printf("Only local connections are allowed.\n");
    } else if (!allowed_ips.empty()) {
      printf("Remote connections are allowed by an allowlist (%s).\n",
             allowlist_ips.c_str());
    } else {
      printf("All remote connections are allowed. Use an allowlist instead!\n");
    }
    printf("%s\n", GetPortProtectionMessage());
    fflush(stdout);
  }

  if (!InitLogging()) {
    printf("Unable to initialize logging. Exiting...\n");
    return 1;
  }

  if (cmd_line->HasSwitch("log-path")) {
    VLOG(0) << "Starting " << kChromeDriverProductFullName << " "
            << kChromeDriverVersion << " on port " << port;
    VLOG(0) << GetPortProtectionMessage();
  }

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  EnsureSharedMemory(cmd_line);
#endif

  mojo::core::Init();

#if BUILDFLAG(IS_OZONE)
  InitializeOzoneKeyboardEngineManager();
#endif

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      kChromeDriverProductShortName);

  RunServer(port, allow_remote, allowed_ips, allowed_origins, url_base,
            adb_port);

  // clean up
  base::ThreadPoolInstance::Get()->Shutdown();
  return 0;
}
