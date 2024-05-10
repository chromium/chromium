// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_NETWORK_IMPL_H_
#define COMPONENTS_FEED_CORE_V2_FEED_NETWORK_IMPL_H_

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/types.h"
#include "components/version_info/channel.h"
#include "url/gurl.h"

class PrefService;
namespace signin {
class IdentityManager;
}  // namespace signin
namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace feed {
constexpr base::TimeDelta kAccessTokenFetchTimeout = base::Seconds(10);
constexpr char kClientInfoHeader[] = "search.now.clientinfo-bin";

class FeedNetworkImpl : public FeedNetwork {
 public:
  class NetworkFetch;
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns a string which represents the top locale and region of the
    // device.
    virtual std::string GetLanguageTag() = 0;
    // Returns the AccountInfo for the signed in user if they are sync-enabled,
    // or empty otherwise.
    virtual AccountInfo GetAccountInfo() = 0;
    // Returns whether the device is offline.
    virtual bool IsOffline() = 0;
  };

  FeedNetworkImpl(Delegate* delegate,
                  signin::IdentityManager* identity_manager,
                  const std::string& api_key,
                  scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
                  PrefService* pref_service);
  ~FeedNetworkImpl() override;
  FeedNetworkImpl(const FeedNetworkImpl&) = delete;
  FeedNetworkImpl& operator=(FeedNetworkImpl&) = delete;

  // FeedNetwork.

  void SendQueryRequest(
      NetworkRequestType request_type,
      const feedwire::Request& request,
      const AccountInfo& account_info,
      base::OnceCallback<void(QueryRequestResult)> callback) override;

  void SendDiscoverApiRequest(
      NetworkRequestType request_type,
      std::string_view api_path,
      std::string_view method,
      std::string request_bytes,
      const AccountInfo& account_info,
      std::optional<RequestMetadata> request_metadata,
      base::OnceCallback<void(RawResponse)> callback) override;

  void SendAsyncDataRequest(
      const GURL& url,
      std::string_view request_method,
      net::HttpRequestHeaders request_headers,
      std::string request_body,
      const AccountInfo& account_info,
      base::OnceCallback<void(RawResponse)> callback) override;

  // Cancels all pending requests immediately. This could be used, for example,
  // if there are pending requests for a user who just signed out.
  void CancelRequests() override;

 private:
  // Start a request to |url| with |request_method| and |request_body|.
  // |callback| will be called when the response is received or if there is
  // an error, including non-protocol errors. The contents of |request_body|
  // will be gzipped.
  void Send(const GURL& url,
            std::string_view request_method,
            std::string request_body,
            bool allow_bless_auth,
            const AccountInfo& account_info,
            net::HttpRequestHeaders request_metadata,
            bool is_feed_query,
            base::OnceCallback<void(FeedNetworkImpl::RawResponse)> callback);

  void SendComplete(NetworkFetch* fetch,
                    base::OnceCallback<void(RawResponse)> callback,
                    RawResponse raw_response);

  // Override url if requested.
  GURL GetOverriddenUrl(const GURL& url) const;

  raw_ptr<Delegate> delegate_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  const std::string api_key_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  raw_ptr<PrefService> pref_service_;
  base::flat_set<std::unique_ptr<NetworkFetch>, base::UniquePtrComparator>
      pending_requests_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_NETWORK_IMPL_H_
