// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DEVICE_MANAGEMENT_DM_CLIENT_H_
#define CHROME_UPDATER_DEVICE_MANAGEMENT_DM_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"

class GURL;

namespace update_client {
class NetworkFetcher;
}

namespace updater {

class CachedPolicyInfo;
class DMStorage;

// This class is responsible for everything related to communication with the
// device management server.
// The class maintains the intermediate state of a network request, thus it
// cannot handle multiple requests in parallel.
class DMClient {
 public:
  class Configurator {
   public:
    virtual ~Configurator() = default;

    // URL at which to contact the DM server.
    virtual std::string GetDMServerUrl() const = 0;

    // Agent reported in the "agent" query parameter.
    virtual std::string GetAgentParameter() const = 0;

    // The platform reported in the "platform" query parameter.
    virtual std::string GetPlatformParameter() const = 0;

    virtual std::unique_ptr<update_client::NetworkFetcher>
    CreateNetworkFetcher() const = 0;
  };

  enum class RequestResult {
    // DM request is completed successfully.
    kSuccess = 0,

    // Request is not sent because there's no device ID.
    kNoDeviceID,

    // Register request is not sent since the device is already registered.
    kAleadyRegistered,

    // Request is not sent because the device is not managed.
    kNotManaged,

    // Request is not sent because the device is de-registered.
    kDeregistered,

    // Policy fetch request is not sent because there's no DM token.
    kNoDMToken,

    // Request is not sent because network fetcher fails to create.
    kFetcherError,

    // Request failed with network error.
    kNetworkError,

    // Request failed with an HTTP error from server.
    kHttpError,

    // Failed to persist the response into storage.
    kSerializationError,

    // Got an unexpected response for the request.
    kUnexpectedResponse,
  };

  using DMRequestCallback = base::OnceCallback<void(RequestResult)>;

  DMClient();
  DMClient(std::unique_ptr<Configurator> config,
           scoped_refptr<DMStorage> storage);
  DMClient(const DMClient&) = delete;
  DMClient& operator=(const DMClient&) = delete;
  ~DMClient();

  // Returns the storage where this client saves the data from DM server.
  scoped_refptr<DMStorage> GetStorage() const;

  // Posts a device register request to the server. Upon success, a new DM
  // token is saved into the storage before |request_callback| is called.
  void PostRegisterRequest(DMRequestCallback request_callback);

  // Posts a policy fetch request to the server. Upon success, new polices
  // are saved into the storage before |request_callback| is called.
  void PostPolicyFetchRequest(DMRequestCallback request_callback);

 private:
  // Gets the full request URL to DM server for the given request type.
  // Additional device specific values, such as device ID, platform etc. will
  // be appended to the URL as query parameters.
  GURL BuildURL(const std::string& request_type) const;

  // Callback functions for the URLFetcher.
  void OnRequestStarted(int response_code, int64_t content_length);
  void OnRequestProgress(int64_t current);
  void OnRegisterRequestComplete(std::unique_ptr<std::string> response_body,
                                 int net_error,
                                 const std::string& header_etag,
                                 const std::string& header_x_cup_server_proof,
                                 int64_t xheader_retry_after_sec);
  void OnPolicyFetchRequestComplete(
      std::unique_ptr<std::string> response_body,
      int net_error,
      const std::string& header_etag,
      const std::string& header_x_cup_server_proof,
      int64_t xheader_retry_after_sec);

  std::unique_ptr<Configurator> config_;
  scoped_refptr<DMStorage> storage_;
  std::unique_ptr<CachedPolicyInfo> cached_info_;

  std::unique_ptr<update_client::NetworkFetcher> network_fetcher_;
  DMRequestCallback request_callback_;
  int http_status_code_;

  SEQUENCE_CHECKER(sequence_checker_);
};

inline std::ostream& operator<<(std::ostream& os,
                                const DMClient::RequestResult& request_result) {
  return os << static_cast<int>(request_result);
}

}  // namespace updater

#endif  // CHROME_UPDATER_DEVICE_MANAGEMENT_DM_CLIENT_H_
