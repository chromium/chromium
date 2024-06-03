// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_SERVER_PROXY_H_
#define COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_SERVER_PROXY_H_

#include <queue>
#include <string>
#include <unordered_map>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

class EndpointFetcher;
struct EndpointResponse;

namespace commerce {

enum class SubscriptionType;
struct CommerceSubscription;

using ManageSubscriptionsFetcherCallback = base::OnceCallback<void(
    SubscriptionsRequestStatus,
    std::unique_ptr<std::vector<CommerceSubscription>>)>;
using GetSubscriptionsFetcherCallback = base::OnceCallback<void(
    SubscriptionsRequestStatus,
    std::unique_ptr<std::vector<CommerceSubscription>>)>;

class SubscriptionsServerProxy {
 public:
  SubscriptionsServerProxy(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  SubscriptionsServerProxy(const SubscriptionsServerProxy&) = delete;
  SubscriptionsServerProxy& operator=(const SubscriptionsServerProxy&) = delete;
  virtual ~SubscriptionsServerProxy();

  // Make an HTTPS call to backend to create the new |subscriptions|.
  virtual void Create(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      ManageSubscriptionsFetcherCallback callback);

  // Make an HTTPS call to backend to delete the existing |subscriptions|.
  virtual void Delete(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      ManageSubscriptionsFetcherCallback callback);

  // Make an HTTP call to backend to get all subscriptions for specified type.
  virtual void Get(SubscriptionType type,
                   GetSubscriptionsFetcherCallback callback);

 protected:
  // This method could be overridden in tests.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::string& http_method,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag);

 private:
  // Handle Create or Delete response.
  void HandleManageSubscriptionsResponses(
      ManageSubscriptionsFetcherCallback callback,
      // Passing the endpoint_fetcher ensures the endpoint_fetcher's
      // lifetime extends to the callback and is not destroyed
      // prematurely (which would result in cancellation of the request).
      // TODO(crbug.com/40238190): Avoid passing this fetcher.
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> responses);

  // This is called when Create or Delete response is parsed.
  void OnManageSubscriptionsJsonParsed(
      ManageSubscriptionsFetcherCallback callback,
      data_decoder::DataDecoder::ValueOrError result);

  // Handle Get response.
  void HandleGetSubscriptionsResponses(
      GetSubscriptionsFetcherCallback callback,
      // Passing the endpoint_fetcher ensures the endpoint_fetcher's
      // lifetime extends to the callback and is not destroyed
      // prematurely (which would result in cancellation of the request).
      // TODO(crbug.com/40238190): Avoid passing this fetcher.
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> responses);

  // This is called when Get response is parsed.
  void OnGetSubscriptionsJsonParsed(
      GetSubscriptionsFetcherCallback callback,
      data_decoder::DataDecoder::ValueOrError result);

  std::unique_ptr<std::vector<CommerceSubscription>>
  GetSubscriptionsFromParsedJson(
      const data_decoder::DataDecoder::ValueOrError& result);

  bool IsPriceTrackingLocaleKeyEnabled();

  base::Value::Dict Serialize(const CommerceSubscription& subscription);

  std::optional<CommerceSubscription> Deserialize(const base::Value& value);

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const raw_ptr<signin::IdentityManager> identity_manager_;

  base::WeakPtrFactory<SubscriptionsServerProxy> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SUBSCRIPTIONS_SUBSCRIPTIONS_SERVER_PROXY_H_
