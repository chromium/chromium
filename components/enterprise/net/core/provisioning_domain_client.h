// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_PROVISIONING_DOMAIN_CLIENT_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_PROVISIONING_DOMAIN_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace enterprise_net {

// LINT.IfChange(EnterpriseProvisioningDomainClientFetchResult)
enum class ProvisioningDomainClientFetchResult {
  kSuccess = 0,
  kNetError = 1,
  kHttpError = 2,
  kMaxValue = kHttpError,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:EnterpriseProvisioningDomainClientFetchResult)

// Result error states for lower-level Provisioning Domain HTTP requests.
struct ProvisioningDomainClientError {
  int net_error = 0;
  int response_code = -1;

  bool operator==(const ProvisioningDomainClientError&) const = default;
};

using ProvisioningDomainClientResult =
    base::expected<std::string, ProvisioningDomainClientError>;

// Low-level HTTP client wrapper around SimpleURLLoader dedicated to fetching
// Provisioning Domain (PvD) JSON configurations over HTTPS.
class ProvisioningDomainClient {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns any resolved extra HTTP request headers that should be attached
    // when fetching the Provisioning Domain JSON configuration.
    virtual net::HttpRequestHeaders GetExtraHeaders() const = 0;
  };

  using FetchCallback =
      base::OnceCallback<void(ProvisioningDomainClientResult result)>;

  // Constructs a ProvisioningDomainClient.
  // - `url`: The target Provisioning Domain endpoint URL to fetch.
  // - `delegate`: Optional delegate to provide request customization such as
  //   extra headers.
  // - `url_loader_factory`: Factory used to instantiate network loader.
  ProvisioningDomainClient(
      const GURL& url,
      Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ProvisioningDomainClient(const ProvisioningDomainClient&) = delete;
  ProvisioningDomainClient& operator=(const ProvisioningDomainClient&) = delete;
  ~ProvisioningDomainClient();

  // Initiates an HTTP GET request to `url_` with headers from `delegate_` and
  // optional `access_token`.
  // If a fetch is already in-flight, queues `callback` to receive the result
  // when complete.
  void Fetch(const std::optional<std::string>& access_token,
             FetchCallback callback);

  // Cancels any in-flight HTTP request and clears pending callbacks.
  void Cancel();

  // Returns true if an HTTP request is currently in-flight.
  bool is_fetching() const { return url_loader_ != nullptr; }

 private:
  void OnURLLoaderComplete(std::optional<std::string> response_body);

  SEQUENCE_CHECKER(sequence_checker_);

  const GURL url_;
  const raw_ptr<Delegate> delegate_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::vector<FetchCallback> pending_callbacks_;

  base::WeakPtrFactory<ProvisioningDomainClient> weak_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_PROVISIONING_DOMAIN_CLIENT_H_
