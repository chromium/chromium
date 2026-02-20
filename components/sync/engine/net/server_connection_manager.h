// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_
#define COMPONENTS_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_

#include <stdint.h>

#include <iosfwd>
#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/signin/public/identity_manager/access_token_info.h"

namespace syncer {

// HttpResponse gathers the relevant output properties of an HTTP request.
// Depending on the value of the server_status code, response_code, and
// content_length may not be valid.
struct HttpResponse {
  enum ServerConnectionCode {
    // For uninitialized state.
    NONE,

    // CONNECTION_UNAVAILABLE means either the request got canceled or it
    // encountered a network error.
    CONNECTION_UNAVAILABLE,

    // SYNC_SERVER_ERROR is returned when the HTTP status code indicates that
    // a non-auth error has occurred.
    SYNC_SERVER_ERROR,

    // SYNC_AUTH_ERROR is returned when the HTTP status code indicates that an
    // auth error has occurred (i.e. a 401).
    SYNC_AUTH_ERROR,

    // SERVER_CONNECTION_OK is returned when request was handled correctly.
    SERVER_CONNECTION_OK,
  };

  // Identifies the type of failure, if any.
  ServerConnectionCode server_status;

  // The network error code.
  int net_error_code;

  // The HTTP Status code.
  int http_status_code;

  // The value of the Content-length header.
  int64_t content_length;

  static HttpResponse Uninitialized();
  static HttpResponse ForNetError(int net_error_code);
  static HttpResponse ForUnspecifiedError();
  static HttpResponse ForHttpStatusCode(int http_status_code);

  // For testing only.
  static HttpResponse ForSuccessForTest();

 private:
  // Private to prevent accidental usage. Use Uninitialized() if you really need
  // a "default" instance.
  HttpResponse();
};

struct ServerConnectionEvent {
  HttpResponse::ServerConnectionCode connection_code;
  explicit ServerConnectionEvent(HttpResponse::ServerConnectionCode code)
      : connection_code(code) {}
};

class ServerConnectionEventListener : public base::CheckedObserver {
 public:
  virtual void OnServerConnectionEvent(const ServerConnectionEvent& event) = 0;

 protected:
  ~ServerConnectionEventListener() override = default;
};

// Use this class to interact with the sync server.
// The ServerConnectionManager currently supports POSTing protocol buffers.
//
class ServerConnectionManager {
 public:
  ServerConnectionManager();

  ServerConnectionManager(const ServerConnectionManager&) = delete;
  ServerConnectionManager& operator=(const ServerConnectionManager&) = delete;

  virtual ~ServerConnectionManager();

  // POSTs `buffer_in` and reads the body of the response into `buffer_out`.
  // Uses the currently set access token in the headers.
  HttpResponse PostBufferWithCachedAuth(const std::string& buffer_in,
                                        std::string* buffer_out);

  void AddListener(ServerConnectionEventListener* listener);
  void RemoveListener(ServerConnectionEventListener* listener);

  HttpResponse::ServerConnectionCode server_status() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return server_response_.server_status;
  }

  int net_error_code() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return server_response_.net_error_code;
  }

  int http_status_code() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return server_response_.http_status_code;
  }

  // Sets a new access token. If `access_token_info` is empty, the current token
  // is invalidated and cleared. Returns false if the server is in
  // authentication error state.
  bool SetAccessTokenInfo(const signin::AccessTokenInfo& access_token_info);

  // Returns true if the current access token is invalid (e.g. expired or
  // empty).
  bool HasInvalidAccessToken() const;

 protected:
  // Updates `server_response_` and notifies listeners if the server status
  // changed.
  void SetServerResponse(const HttpResponse& server_response);

  // Internal PostBuffer base function which subclasses are expected to
  // implement.
  virtual HttpResponse PostBuffer(const std::string& buffer_in,
                                  std::string* buffer_out) = 0;

  // Clears the current access token.
  void ClearAccessToken();

  // Returns the current raw access token, empty if there is no valid token.
  std::string GetAccessToken() const;

 private:
  void NotifyStatusChanged();

  // The access token to use in authenticated requests.
  signin::AccessTokenInfo access_token_info_;

  base::ObserverList<ServerConnectionEventListener> listeners_;

  HttpResponse server_response_;

  SEQUENCE_CHECKER(sequence_checker_);
};

std::ostream& operator<<(std::ostream& s, const struct HttpResponse& hr);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_
