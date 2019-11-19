// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_
#define COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "url/gurl.h"

namespace client_update_protocol {
class Ecdsa;
}

namespace update_client {

class Configurator;
class NetworkFetcher;

// Sends a request to one of the urls provided. The class implements a chain
// of responsibility design pattern, where the urls are tried in the order they
// are specified, until the request to one of them succeeds or all have failed.
// CUP signing is optional.
class RequestSender {
 public:
  // If |error| is 0, then the response is provided in the |response| parameter.
  // |retry_after_sec| contains the value of the X-Retry-After response header,
  // when the response was received from a cryptographically secure URL. The
  // range for this value is [-1, 86400]. If |retry_after_sec| is -1 it means
  // that the header could not be found, or trusted, or had an invalid value.
  // The upper bound represents a delay of one day.
  using RequestSenderCallback = base::OnceCallback<
      void(int error, const std::string& response, int retry_after_sec)>;

  explicit RequestSender(scoped_refptr<Configurator> config);
  ~RequestSender();

  // |use_signing| enables CUP signing of protocol messages exchanged using
  // this class. |is_foreground| controls the presence and the value for the
  // X-GoogleUpdate-Interactvity header serialized in the protocol request.
  // If this optional parameter is set, the values of "fg" or "bg" are sent
  // for true or false values of this parameter. Otherwise the header is not
  // sent at all.
  void Send(
      const std::vector<GURL>& urls,
      const base::flat_map<std::string, std::string>& request_extra_headers,
      const std::string& request_body,
      bool use_signing,
      RequestSenderCallback request_sender_callback);

 private:
  // Combines the |url| and |query_params| parameters.
  static GURL BuildUpdateUrl(const GURL& url, const std::string& query_params);

  // Decodes and returns the public key used by CUP.
  static std::string GetKey(const char* key_bytes_base64);

  void OnResponseStarted(int response_code, int64_t content_length);

  void OnNetworkFetcherComplete(const GURL& original_url,
                                std::unique_ptr<std::string> response_body,
                                int net_error,
                                const std::string& header_etag,
                                int64_t xheader_retry_after_sec);

  // Implements the error handling and url fallback mechanism.
  void SendInternal();

  // Called when SendInternal completes. |response_body| and |response_etag|
  // contain the body and the etag associated with the HTTP response.
  void SendInternalComplete(int error,
                            const std::string& response_body,
                            const std::string& response_etag,
                            int retry_after_sec);

  // Helper function to handle a non-continuable error in Send.
  void HandleSendError(int error, int retry_after_sec);

  base::ThreadChecker thread_checker_;

  const scoped_refptr<Configurator> config_;

  std::vector<GURL> urls_;
  base::flat_map<std::string, std::string> request_extra_headers_;
  std::string request_body_;
  bool use_signing_ = false;  // True if CUP signing is used.
  RequestSenderCallback request_sender_callback_;

  std::string public_key_;
  std::vector<GURL>::const_iterator cur_url_;
  std::unique_ptr<NetworkFetcher> network_fetcher_;
  std::unique_ptr<client_update_protocol::Ecdsa> signer_;

  int response_code_ = -1;

  DISALLOW_COPY_AND_ASSIGN(RequestSender);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_
