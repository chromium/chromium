// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_
#define COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

namespace client_update_protocol {
class Ecdsa;
}

namespace update_client {

// Sends a request to one of the urls provided. The class implements a chain
// of responsibility design pattern, where the urls are tried in the order they
// are specified, until the request to one of them succeeds or all have failed.
// CUP signing is optional.
class RequestSender : public base::RefCountedThreadSafe<RequestSender> {
 public:
  // If |error| is 0, then the response is provided in the |response| parameter.
  // |retry_after_sec| contains the value of the X-Retry-After response header,
  // when the response was received from a cryptographically secure URL. The
  // range for this value is [-1, 86400]. If |retry_after_sec| is -1 it means
  // that the header could not be found, or trusted, or had an invalid value.
  // The upper bound represents a delay of one day.
  using RequestSenderCallback = base::OnceCallback<
      void(int error, const std::string& response, int retry_after_sec)>;

  explicit RequestSender(scoped_refptr<NetworkFetcherFactory> fetcher_factory);

  RequestSender(const RequestSender&) = delete;
  RequestSender& operator=(const RequestSender&) = delete;

  // |use_signing| enables CUP signing of protocol messages exchanged using
  // this class. |is_foreground| controls the presence and the value for the
  // X-GoogleUpdate-Interactvity header serialized in the protocol request.
  // If this optional parameter is set, the values of "fg" or "bg" are sent
  // for true or false values of this parameter. Otherwise the header is not
  // sent at all. Returns a callback that can be used to cancel the request.
  base::OnceClosure Send(
      const std::vector<GURL>& urls,
      const base::flat_map<std::string, std::string>& request_extra_headers,
      const std::string& request_body,
      bool use_signing,
      RequestSenderCallback request_sender_callback);

 private:
  friend class base::RefCountedThreadSafe<RequestSender>;
  virtual ~RequestSender();

  // Combines the |url| and |query_params| parameters.
  static GURL BuildUpdateUrl(const GURL& url, const std::string& query_params);

  // Decodes and returns the public key used by CUP.
  static std::string GetKey(const char* key_bytes_base64);

  void OnResponseStarted(int response_code, int64_t content_length);

  void OnNetworkFetcherComplete(const GURL& original_url,
                                std::unique_ptr<std::string> response_body,
                                int net_error,
                                const std::string& header_etag,
                                const std::string& xheader_cup_server_proof,
                                int64_t xheader_retry_after_sec);

  // Implements the error handling and url fallback mechanism.
  void SendInternal();

  // Called when SendInternal completes. |response_body| and |response_etag|
  // contain the body and the etag associated with the HTTP response.
  void SendInternalComplete(int error,
                            const std::string& response_body,
                            const std::string& response_etag,
                            const std::string& response_cup_server_proof,
                            int retry_after_sec);

  // Helper function to handle a non-continuable error in Send.
  void HandleSendError(int error, int retry_after_sec);

  // Cancels any ongoing fetches and destroys the network_fetcher_. Public
  // callers must use the callback returned from Send.
  void Cancel();

  // Returns request_sender_callback_, replacing it with base::DoNothing().
  // The network operations and Cancel can race, causing multiple flows to
  // access the callback. Use TakeRequestSenderCallback so that the code that
  // loses the race doesn't crash.
  RequestSenderCallback TakeRequestSenderCallback();

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<NetworkFetcherFactory> fetcher_factory_;

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
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_
