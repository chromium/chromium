// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_SERVER_H_
#define CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_SERVER_H_

#include "base/json/json_reader.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/test/chromedriver/server/http_handler.h"
#include "net/base/url_util.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

typedef base::RepeatingCallback<void(const net::HttpServerRequestInfo&,
                                     const HttpResponseSenderFunc&)>
    HttpRequestHandlerFunc;

class HttpServerInterface {
 public:
  virtual ~HttpServerInterface() = default;
  virtual void Close(int connection_id) = 0;

  virtual void AcceptWebSocket(int connection_id,
                               const net::HttpServerRequestInfo& request) = 0;

  virtual void SendOverWebSocket(int connection_id,
                                 const std::string& data) = 0;

  virtual void SendResponse(
      int connection_id,
      const net::HttpServerResponseInfo& response,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
};

class HttpServer : public net::HttpServer::Delegate,
                   public virtual HttpServerInterface {
 public:
  explicit HttpServer(const std::string& url_base,
                      const std::vector<net::IPAddress>& whitelisted_ips,
                      const std::vector<std::string>& allowed_origins,
                      const HttpRequestHandlerFunc& handle_request_func,
                      base::WeakPtr<HttpHandler> handler,
                      scoped_refptr<base::SingleThreadTaskRunner> cmd_runner);

  ~HttpServer() override;

  int Start(uint16_t port, bool allow_remote, bool use_ipv4);

  // Overridden from net::HttpServer::Delegate:
  void OnConnect(int connection_id) override;

  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;

  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;

  void OnWebSocketMessage(int connection_id, std::string data) override;

  void OnClose(int connection_id) override;

  void Close(int connection_id) override;

  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request) override;

  void SendOverWebSocket(int connection_id, const std::string& data) override;

  void SendResponse(
      int connection_id,
      const net::HttpServerResponseInfo& response,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

  const net::IPEndPoint& LocalAddress() const;

 private:
  void OnResponse(int connection_id,
                  bool keep_alive,
                  std::unique_ptr<net::HttpServerResponseInfo> response);
  const std::string url_base_;
  HttpRequestHandlerFunc handle_request_func_;
  std::unique_ptr<net::HttpServer> server_;
  std::map<int, std::string> connection_to_session_map;
  bool allow_remote_;
  const std::vector<net::IPAddress> whitelisted_ips_;
  const std::vector<std::string> allowed_origins_;
  base::WeakPtr<HttpHandler> handler_;
  scoped_refptr<base::SingleThreadTaskRunner> cmd_runner_;
  net::IPEndPoint local_address_;
  base::WeakPtrFactory<HttpServer> weak_factory_{this};  // Should be last.
};

#endif  // CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_SERVER_H_
