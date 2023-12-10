// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/remote_debugging_server.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/devtools/cast_devtools_manager_delegate.h"
#include "chromecast/common/cast_content_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/devtools_auth.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace chromecast {
namespace shell {

namespace {

const uint16_t kDefaultRemoteDebuggingPort = 9222;

const int kBackLog = 10;

#if BUILDFLAG(IS_ANDROID)
class UnixDomainServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name) {}

  UnixDomainServerSocketFactory(const UnixDomainServerSocketFactory&) = delete;
  UnixDomainServerSocketFactory& operator=(
      const UnixDomainServerSocketFactory&) = delete;

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::BindRepeating(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return nullptr;

    return std::move(socket);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    return nullptr;
  }

  std::string socket_name_;
};
#else
class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit TCPServerSocketFactory(const net::IPEndPoint& endpoint)
      : endpoint_(endpoint) {}

  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));

    if (socket->Listen(endpoint_, kBackLog, /*ipv6_only=*/std::nullopt) !=
        net::OK) {
      return nullptr;
    }

    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    return nullptr;
  }

  const net::IPEndPoint endpoint_;
};
#endif

std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory(
    uint16_t port) {
#if BUILDFLAG(IS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string socket_name = "cast_shell_devtools_remote";
  if (command_line->HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name = command_line->GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
  return std::unique_ptr<content::DevToolsSocketFactory>(
      new UnixDomainServerSocketFactory(socket_name));
#else
  net::IPEndPoint endpoint(net::IPAddress::IPv6AllZeros(), port);
  return std::unique_ptr<content::DevToolsSocketFactory>(
      new TCPServerSocketFactory(endpoint));
#endif
}

uint16_t GetPort() {
  std::string port_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRemoteDebuggingPort);

  if (port_str.empty())
    return kDefaultRemoteDebuggingPort;

  int port;
  if (base::StringToInt(port_str, &port))
    return port;

  return kDefaultRemoteDebuggingPort;
}

}  // namespace

class RemoteDebuggingServer::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  WebContentsObserver(content::WebContents* contents,
                      RemoteDebuggingServer* server)
      : server_(server) {
    Observe(contents);
  }

  WebContentsObserver(const WebContentsObserver&) = delete;
  WebContentsObserver& operator=(const WebContentsObserver&) = delete;

  ~WebContentsObserver() override {}

  // content::WebContentsObserver implementation:
  void WebContentsDestroyed() override {
    // Alert the server that |web_contents_| will be destroyed. Do not call
    // anything after this line; |this| will be destroyed.
    server_->DisableWebContentsForDebugging(web_contents());
  }

 private:
  RemoteDebuggingServer* const server_;
};

RemoteDebuggingServer::RemoteDebuggingServer(bool start_immediately)
    : port_(GetPort()), is_started_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  StopIfNeeded();
  for (const auto& observer : observers_)
    DisableWebContentsForDebugging(observer.first);
}

CastDevToolsManagerDelegate* RemoteDebuggingServer::GetDevtoolsDelegate() {
  // TODO(slan): This class uses a broken pattern of ownership and access which
  // requires careful handling when obtaining a reference. DevToolsManager
  // "owns" the single instance of this class, and could theoretically destroy
  // it without warning.
  auto* dev_tools_delegate =
      chromecast::shell::CastDevToolsManagerDelegate::GetInstance();
  DCHECK(dev_tools_delegate);
  return dev_tools_delegate;
}

void RemoteDebuggingServer::StartIfNeeded() {
  if (is_started_)
    return;

  base::FilePath output_dir;
  if (!port_) {
    // Providing a defined output_dir causes DevTools to write the port
    // number chosen to a file named DevToolsActivePort. This file is
    // used by test automation tools like ChromeDriver. ChromeDriver passes
    // in --remote-debugging-port=0 so that cast_shell picks its own port to
    // binds to so that there is no race condition.
    bool result =
        base::PathService::Get(chromecast::DIR_CAST_HOME, &output_dir);
    DCHECK(result);
  }

  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      CreateSocketFactory(port_), output_dir, base::FilePath());
  LOG(INFO) << "Devtools started";
  is_started_ = true;
}

void RemoteDebuggingServer::StopIfNeeded() {
  // TODO(seantopping): The debugging server goes down temporarily when there
  // are no active apps. This can sometimes break in-progress traces. Find a
  // fix for this.
  if (!is_started_ || GetDevtoolsDelegate()->HasEnabledWebContents())
    return;

  LOG(INFO) << "Stopping Devtools server.";
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
  is_started_ = false;
}

void RemoteDebuggingServer::EnableWebContentsForDebugging(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  StartIfNeeded();

  GetDevtoolsDelegate()->EnableWebContentsForDebugging(web_contents);
  observers_.insert(std::make_pair(
      web_contents, std::make_unique<WebContentsObserver>(web_contents, this)));
}

void RemoteDebuggingServer::DisableWebContentsForDebugging(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  observers_.erase(web_contents);
  GetDevtoolsDelegate()->DisableWebContentsForDebugging(web_contents);

  StopIfNeeded();
}

}  // namespace shell
}  // namespace chromecast
