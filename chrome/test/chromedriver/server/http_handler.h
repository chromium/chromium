// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_
#define CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/test/chromedriver/command.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/connection_session_map.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/session_commands.h"
#include "chrome/test/chromedriver/session_connection_map.h"
#include "chrome/test/chromedriver/session_thread_map.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "net/http/http_status_code.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class HttpServerRequestInfo;
class HttpServerResponseInfo;
}

namespace network {
class TransitionalURLLoaderFactoryOwner;
}

class Adb;
class DeviceManager;
class URLRequestContextGetter;
class WrapperURLLoaderFactory;

class HttpServer;
class HttpServerInterface;

enum HttpMethod {
  kGet,
  kPost,
  kDelete,
};

struct CommandMapping {
  CommandMapping(HttpMethod method,
                 const std::string& path_pattern,
                 const Command& command);
  CommandMapping(const CommandMapping& other);
  ~CommandMapping();

  HttpMethod method;
  std::string path_pattern;
  Command command;
};

extern const char kCreateWebSocketPath[];
extern const char kSendCommandFromWebSocket[];

typedef base::RepeatingCallback<void(
    std::unique_ptr<net::HttpServerResponseInfo>)>
    HttpResponseSenderFunc;

class HttpHandler {
 public:
  explicit HttpHandler(const std::string& url_base);
  HttpHandler(const base::RepeatingClosure& quit_func,
              const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
              const scoped_refptr<base::SingleThreadTaskRunner> cmd_task_runner,
              const std::string& url_base,
              int adb_port);

  HttpHandler(const HttpHandler&) = delete;
  HttpHandler& operator=(const HttpHandler&) = delete;

  ~HttpHandler();

  void Handle(const net::HttpServerRequestInfo& request,
              const HttpResponseSenderFunc& send_response_func);

  void OnWebSocketMessage(HttpServerInterface* http_server,
                          int connection_id,
                          const std::string& data);

  void OnWebSocketRequest(HttpServerInterface* http_server,
                          int connection_id,
                          const net::HttpServerRequestInfo& info);

  base::WeakPtr<HttpHandler> WeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleUnknownCommand);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleNewSession);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleInvalidPost);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleUnimplementedCommand);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleCommand);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, StandardResponse_ErrorNoMessage);
  typedef std::vector<CommandMapping> CommandMap;

  friend class HttpServer;
  friend class WebSocketMessageTest;
  friend class WebSocketRequestTest;

  Command WrapToCommand(const char* name,
                        const SessionCommand& session_command,
                        bool w3c_standard_command = true);
  Command WrapToCommand(const char* name,
                        const WindowCommand& window_command,
                        bool w3c_standard_command = true);
  Command WrapToCommand(const char* name,
                        const ElementCommand& element_command,
                        bool w3c_standard_command = true);
  void HandleCommand(const net::HttpServerRequestInfo& request,
                     const std::string& trimmed_path,
                     const HttpResponseSenderFunc& send_response_func);
  void PrepareResponse(const std::string& trimmed_path,
                       const HttpResponseSenderFunc& send_response_func,
                       const Status& status,
                       std::unique_ptr<base::Value> value,
                       const std::string& session_id,
                       bool w3c_compliant);
  std::unique_ptr<net::HttpServerResponseInfo> PrepareLegacyResponse(
      const std::string& trimmed_path,
      const Status& status,
      std::unique_ptr<base::Value> value,
      const std::string& session_id);

  std::unique_ptr<net::HttpServerResponseInfo> PrepareStandardResponse(
      const std::string& trimmed_path,
      const Status& status,
      std::unique_ptr<base::Value> value,
      const std::string& session_id);

  void OnWebSocketAttachToSessionRequest(
      HttpServerInterface* http_server,
      int connection_id,
      const std::string& session_id,
      const net::HttpServerRequestInfo& info);

  void OnWebSocketUnboundConnectionRequest(
      HttpServerInterface* http_server,
      int connection_id,
      const net::HttpServerRequestInfo& info);

  void SendResponseOverWebSocket(HttpServerInterface* http_server,
                                 int connection_id,
                                 const std::optional<base::Value>& maybe_id,
                                 const Status& status,
                                 std::unique_ptr<base::Value> result,
                                 const std::string& session_id,
                                 bool w3c);

  void CloseConnectionOnCommandThread(HttpServerInterface* http_server,
                                      int connection_id);

  void SendForwardedResponseOnCommandThread(HttpServerInterface* http_server,
                                            int connection_id,
                                            std::string message);

  void OnWebSocketResponseOnCmdThread(HttpServerInterface* http_server,
                                      int connection_id,
                                      const std::string& data);

  void OnWebSocketResponseOnSessionThread(HttpServerInterface* http_server,
                                          int connection_id,
                                          const std::string& data);
  Command WrapCreateNewSessionCommand(Command command);
  void OnNewSessionCreated(const CommandCallback& next_callback,
                           const Status& status,
                           std::unique_ptr<base::Value> result,
                           const std::string& session_id,
                           bool w3c);
  void OnNewBidiSessionOnCmdThread(HttpServerInterface* http_server,
                                   int connection_id,
                                   const std::optional<base::Value>& maybe_id,
                                   const Status& status,
                                   std::unique_ptr<base::Value> result,
                                   const std::string& session_id,
                                   bool w3c);

  void OnClose(HttpServerInterface* http_server, int connection_id);

  void SendWebSocketRejectResponse(
      base::RepeatingCallback<void(int,
                                   const net::HttpServerResponseInfo&,
                                   const net::NetworkTrafficAnnotationTag&)>
          send_http_response,
      int connection_id,
      net::HttpStatusCode code,
      const std::string& msg);

  base::ThreadChecker thread_checker_;
  base::RepeatingClosure quit_func_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> cmd_task_runner_;
  std::string url_base_;
  bool received_shutdown_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  std::unique_ptr<WrapperURLLoaderFactory> wrapper_url_loader_factory_;
  SyncWebSocketFactory socket_factory_;
  SessionThreadMap session_thread_map_;
  SessionConnectionMap session_connection_map_;
  ConnectionSessionMap connection_session_map_;
  std::unique_ptr<CommandMap> command_map_;
  std::unique_ptr<Adb> adb_;
  std::unique_ptr<DeviceManager> device_manager_;
  std::map<std::string, Command> static_bidi_command_map_;
  std::map<std::string, Command> session_bidi_command_map_;
  Command forward_session_command_;

  base::WeakPtrFactory<HttpHandler> weak_ptr_factory_{this};
};

namespace internal {

extern const char kNewSessionPathPattern[];

bool MatchesCommand(const std::string& method,
                    const std::string& path,
                    const CommandMapping& command,
                    std::string* session_id,
                    base::Value::Dict* out_params);

bool IsNewSession(const CommandMapping& command);

Status ParseBidiCommand(const std::string& data, base::Value::Dict& parsed);

base::Value::Dict CreateBidiErrorResponse(
    Status status,
    std::optional<base::Value> maybe_id = std::nullopt);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_
